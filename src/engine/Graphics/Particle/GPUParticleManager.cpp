#include "GPUParticleManager.h"
#include "../System/TextureManager.h"
#include <algorithm>
#include <cassert>
#include <cstring>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif // USE_IMGUI

std::unique_ptr<GPUParticleManager> GPUParticleManager::instance_ = nullptr;

GPUParticleManager* GPUParticleManager::GetInstance() {
	if (!instance_) {
		instance_ = std::make_unique<GPUParticleManager>();
	}
	return instance_.get();
}

void GPUParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateBuffers();
	CreateComputePipeline();
	CreateGraphicsPipeline();
	CreateCommandSignature();
	CreateModel();

	needsInitializeParticles_ = true;
	isInitialized_ = true;
}

void GPUParticleManager::Finalize() {
	particleBuffer_.Reset();
	freeListIndexBuffer_.Reset();
	freeListBuffer_.Reset();
	aliveListBuffer_.Reset();
	indirectArgsBuffer_.Reset();

	for (uint32_t i = 0; i < kFrameCount; ++i) {
		emitRequestBuffer_[i].Reset();
		emitRequestMapped_[i] = nullptr;
	}

	perFrameBuffer_.Reset();
	perFrameMapped_ = nullptr;
	perViewBuffer_.Reset();
	perViewMapped_ = nullptr;
	vertexBuffer_.Reset();
	readbackBuffer_.Reset();

	computeRootSignature_.Reset();
	initPipelineState_.Reset();
	emitPipelineState_.Reset();
	resetArgsPipelineState_.Reset();
	updatePipelineState_.Reset();

	graphicsRootSignature_.Reset();
	for (auto& pso : graphicsPipelineStates_) {
		pso.Reset();
	}
	commandSignature_.Reset();

	pendingEmitRequests_.clear();
	isInitialized_ = false;
	needsInitializeParticles_ = true;
	hasCustomTexture_ = false;
	manualEmitRequested_ = false;
}

void GPUParticleManager::CreateBuffers() {
	// u0..u4 用に連続した5個のUAVディスクリプタを割り当て
	particleUavIndex_ = srvManager_->Allocate();
	freeListIndexUavIndex_ = srvManager_->Allocate();
	freeListUavIndex_ = srvManager_->Allocate();
	aliveListUavIndex_ = srvManager_->Allocate();
	indirectArgsUavIndex_ = srvManager_->Allocate();

	// グラフィックスルートパラメータ0 (t0..t1) 用に連続した2個のSRVディスクリプタを割り当て
	particleSrvIndex_ = srvManager_->Allocate();
	aliveListSrvIndex_ = srvManager_->Allocate();

	// 1. Particle Buffer
	particleBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(ParticleCS) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

	// 2. FreeListIndex Buffer
	freeListIndexBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t));
	srvManager_->CreateUAVforStructuredBuffer(freeListIndexUavIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));

	// 3. FreeList Buffer
	freeListBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(freeListUavIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	// 4. AliveList Buffer
	aliveListBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(aliveListUavIndex_, aliveListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateSRVforStructuredBuffer(aliveListSrvIndex_, aliveListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	// 5. IndirectArgs Buffer
	indirectArgsBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_ARGUMENTS));
	srvManager_->CreateUAVforStructuredBuffer(indirectArgsUavIndex_, indirectArgsBuffer_.Get(), 4, sizeof(uint32_t));

	// 6. EmitRequest Buffers (フレーム別二重化)
	for (uint32_t i = 0; i < kFrameCount; ++i) {
		emitRequestSrvIndex_[i] = srvManager_->Allocate();
		emitRequestBuffer_[i] = dxCommon_->CreateBufferResource(sizeof(GpuEmitRequest) * kMaxEmitRequests);
		emitRequestBuffer_[i]->Map(0, nullptr, reinterpret_cast<void**>(&emitRequestMapped_[i]));
		std::memset(emitRequestMapped_[i], 0, sizeof(GpuEmitRequest) * kMaxEmitRequests);
		srvManager_->CreateSRVforStructuredBuffer(emitRequestSrvIndex_[i], emitRequestBuffer_[i].Get(), kMaxEmitRequests, sizeof(GpuEmitRequest));
	}

	// 7. PerFrame (フレーム別二重化)
	perFrameBuffer_ = dxCommon_->CreateBufferResource(kCbAlignment * kFrameCount);
	perFrameBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameMapped_));
	std::memset(perFrameMapped_, 0, kCbAlignment * kFrameCount);

	// 8. PerView (フレーム別二重化)
	perViewBuffer_ = dxCommon_->CreateBufferResource(kCbAlignment * kFrameCount);
	perViewBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perViewMapped_));
	std::memset(perViewMapped_, 0, kCbAlignment * kFrameCount);

	// 9. Readback Buffer (デバッグ要求時のみ使用)
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(uint32_t) * 5;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&readbackBuffer_)
	);
}

void GPUParticleManager::CreateComputePipeline() {
	auto device = dxCommon_->GetDevice();

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	D3D12_DESCRIPTOR_RANGE uavRange[1] = {};
	uavRange[0].BaseShaderRegister = 0;
	uavRange[0].NumDescriptors = 5;
	uavRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
	srvRange[0].BaseShaderRegister = 0;
	srvRange[0].NumDescriptors = 1;
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = srvRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = 3;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> initCS = dxCommon_->CompileShader(L"assets/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> emitCS = dxCommon_->CompileShader(L"assets/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> resetCS = dxCommon_->CompileShader(L"assets/shaders/ResetParticleArgs.CS.hlsl", L"cs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> updateCS = dxCommon_->CompileShader(L"assets/shaders/UpdateParticle.CS.hlsl", L"cs_6_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = computeRootSignature_.Get();

	psoDesc.CS = { initCS->GetBufferPointer(), initCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&initPipelineState_));
	assert(SUCCEEDED(hr));

	psoDesc.CS = { emitCS->GetBufferPointer(), emitCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&emitPipelineState_));
	assert(SUCCEEDED(hr));

	psoDesc.CS = { resetCS->GetBufferPointer(), resetCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&resetArgsPipelineState_));
	assert(SUCCEEDED(hr));

	psoDesc.CS = { updateCS->GetBufferPointer(), updateCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&updatePipelineState_));
	assert(SUCCEEDED(hr));
}

void GPUParticleManager::CreateGraphicsPipeline() {
	auto device = dxCommon_->GetDevice();

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	D3D12_DESCRIPTOR_RANGE descriptorRangeSRV[1] = {};
	descriptorRangeSRV[0].BaseShaderRegister = 0;
	descriptorRangeSRV[0].NumDescriptors = 2;
	descriptorRangeSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeSRV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE descriptorRangeTex[1] = {};
	descriptorRangeTex[0].BaseShaderRegister = 0;
	descriptorRangeTex[0].NumDescriptors = 1;
	descriptorRangeTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeTex;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = 3;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&graphicsRootSignature_));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"assets/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"assets/shaders/GPUParticle.PS.hlsl", L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = graphicsRootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	graphicsPipelineStateDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	graphicsPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCountOf); ++i) {
		graphicsPipelineStateDesc.BlendState = GetBlendDesc(static_cast<BlendMode>(i));
		hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i]));
		assert(SUCCEEDED(hr));
	}
}

void GPUParticleManager::CreateModel() {
	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 4);

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	vertexData[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} };
	vertexData[1] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} };
	vertexData[2] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} };
	vertexData[3] = { { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} };
}

void GPUParticleManager::CreateCommandSignature() {
	auto device = dxCommon_->GetDevice();

	D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
	argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

	D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc = {};
	cmdSigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	cmdSigDesc.NumArgumentDescs = 1;
	cmdSigDesc.pArgumentDescs = &argDesc;

	HRESULT hr = device->CreateCommandSignature(&cmdSigDesc, nullptr, IID_PPV_ARGS(&commandSignature_));
	assert(SUCCEEDED(hr));
}

void GPUParticleManager::InitializeParticles() {
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(initPipelineState_.Get());
	commandList->SetComputeRootSignature(computeRootSignature_.Get());

	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

	const uint32_t initGroups = (kMaxParticles + kInitThreadGroupSize - 1u) / kInitThreadGroupSize;
	commandList->Dispatch(initGroups, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListBuffer_.Get());
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());
}

void GPUParticleManager::EnqueueEmit(const Vector3& position, const ParticleParameters& params, uint32_t count) {
	if (count == 0u || pendingEmitRequests_.size() >= kMaxEmitRequests) {
		return;
	}

	GpuEmitRequest req{};
	req.origin = position;
	req.randomPositionRange = params.randomPositionRange;
	req.minVelocity = params.minVelocity;
	req.minLifeTime = params.minLifeTime;
	req.maxVelocity = params.maxVelocity;
	req.maxLifeTime = params.maxLifeTime;
	req.acceleration = params.acceleration;
	req.maxRiseHeight = params.maxRiseHeight;
	req.minColor = params.minColor;
	req.maxColor = params.maxColor;
	req.minScale = params.minScale;
	req.maxScale = params.maxScale;
	req.endScale = params.endScale;
	req.scaleEasing = params.scaleEasing;
	req.minRotation = params.minRotation;
	req.maxRotation = params.maxRotation;
	req.minRotationSpeed = params.minRotationSpeed;
	req.maxRotationSpeed = params.maxRotationSpeed;
	req.startOffset = 0u;
	req.count = count;
	req.flags = params.fadeOut ? kParticleFlagFadeOut : 0u;
	req.seedSalt = seedCounter_++;

	pendingEmitRequests_.push_back(req);
}

void GPUParticleManager::SetEmitParams(const Vector3& position, uint32_t count, float radius, float frequency) {
	legacyTranslate_ = position;
	legacyCount_ = count;
	legacyRadius_ = radius;
	legacyFrequency_ = frequency;
}

void GPUParticleManager::Emit() {
	manualEmitRequested_ = true;
}

void GPUParticleManager::ClearAllParticles() {
	pendingEmitRequests_.clear();
	manualEmitRequested_ = false;
	legacyFrequencyTime_ = 0.0f;
	needsInitializeParticles_ = true;
	debugInstanceCount_ = 0u;
	lastFrameEmitCount_ = 0u;
}

void GPUParticleManager::Update() {
	if (!isInitialized_) {
		return;
	}

	if (needsInitializeParticles_) {
		InitializeParticles();
		needsInitializeParticles_ = false;
	}

	// デバッグ時のみ前回のReadback結果を取得
	if (enableReadbackDebug_ && readbackBuffer_) {
		uint32_t* mappedData = nullptr;
		if (SUCCEEDED(readbackBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)))) {
			debugInstanceCount_ = mappedData[1];
			debugFreeListIndex_ = *reinterpret_cast<int32_t*>(&mappedData[4]);
			readbackBuffer_->Unmap(0, nullptr);
		}
	}

	const float clampedDt = std::clamp(deltaTime_, 0.0f, 0.1f);
	time_ += clampedDt;

	// 既存単一エミッタ互換（DebugScene等）
	bool shouldLegacyEmit = manualEmitRequested_;
	manualEmitRequested_ = false;
	if (legacyFrequency_ > 0.0f) {
		legacyFrequencyTime_ += clampedDt;
		if (legacyFrequencyTime_ >= legacyFrequency_) {
			legacyFrequencyTime_ -= legacyFrequency_;
			shouldLegacyEmit = true;
		}
	}
	if (shouldLegacyEmit && legacyCount_ > 0u) {
		ParticleParameters legacyParams{};
		legacyParams.randomPositionRange = legacyRadius_;
		legacyParams.minVelocity = { -0.1f, -0.1f, -0.1f };
		legacyParams.maxVelocity = { 0.1f,  0.1f,  0.1f };
		legacyParams.minScale = 0.5f;
		legacyParams.maxScale = 0.5f;
		legacyParams.endScale = 0.5f;
		legacyParams.minLifeTime = 1.0f;
		legacyParams.maxLifeTime = 2.0f;
		legacyParams.fadeOut = true;
		EnqueueEmit(legacyTranslate_, legacyParams, legacyCount_);
	}

	const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex() % kFrameCount;

	// リクエスト配列をフレーム別StructuredBufferへアップロード
	const uint32_t requestCount = static_cast<uint32_t>((std::min)(pendingEmitRequests_.size(), static_cast<size_t>(kMaxEmitRequests)));
	uint32_t totalEmitCount = 0u;
	for (uint32_t i = 0; i < requestCount; ++i) {
		pendingEmitRequests_[i].startOffset = totalEmitCount;
		emitRequestMapped_[frameIndex][i] = pendingEmitRequests_[i];
		totalEmitCount += pendingEmitRequests_[i].count;
	}
	pendingEmitRequests_.clear();
	lastFrameEmitCount_ = totalEmitCount;

	// フレーム別PerFrame CBV更新
	PerFrame* perFrame = reinterpret_cast<PerFrame*>(perFrameMapped_ + static_cast<size_t>(frameIndex) * kCbAlignment);
	perFrame->time = time_;
	perFrame->deltaTime = clampedDt;
	perFrame->requestCount = requestCount;
	perFrame->totalEmitCount = totalEmitCount;

	const D3D12_GPU_VIRTUAL_ADDRESS perFrameGpuAddr = perFrameBuffer_->GetGPUVirtualAddress() + static_cast<size_t>(frameIndex) * kCbAlignment;
	auto commandList = dxCommon_->GetCommandList();
	commandList->SetComputeRootSignature(computeRootSignature_.Get());

	// 1. Emit Pass（発生要求があるときのみDispatch）
	if (totalEmitCount > 0u) {
		commandList->SetPipelineState(emitPipelineState_.Get());
		srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);
		commandList->SetComputeRootConstantBufferView(1, perFrameGpuAddr);
		srvManager_->SetComputeRootDescriptorTable(2, emitRequestSrvIndex_[frameIndex]);

		const uint32_t emitGroups = (totalEmitCount + kEmitThreadGroupSize - 1u) / kEmitThreadGroupSize;
		commandList->Dispatch(emitGroups, 1, 1);

		dxCommon_->UAVBarrier(particleBuffer_.Get());
		dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
		dxCommon_->UAVBarrier(freeListBuffer_.Get());
	}

	// 2. Reset IndirectArgs Pass（複数Updateスレッドグループ間の競合を完全排除）
	commandList->SetPipelineState(resetArgsPipelineState_.Get());
	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);
	commandList->SetComputeRootConstantBufferView(1, perFrameGpuAddr);
	commandList->Dispatch(1, 1, 1);
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());

	// 3. Update Pass
	commandList->SetPipelineState(updatePipelineState_.Get());
	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);
	commandList->SetComputeRootConstantBufferView(1, perFrameGpuAddr);

	const uint32_t updateGroups = (kMaxParticles + kUpdateThreadGroupSize - 1u) / kUpdateThreadGroupSize;
	commandList->Dispatch(updateGroups, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListBuffer_.Get());
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());
}

void GPUParticleManager::Draw(const Matrix4x4& viewProjection, const Matrix4x4& billboardMatrix) {
	if (!isInitialized_ || needsInitializeParticles_) {
		return;
	}

	if (!hasCustomTexture_) {
		TextureManager::GetInstance()->LoadTexture("assets/textures/circle.png");
		textureSrvIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle.png");
		hasCustomTexture_ = true;
	}

	const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex() % kFrameCount;
	PerView* perView = reinterpret_cast<PerView*>(perViewMapped_ + static_cast<size_t>(frameIndex) * kCbAlignment);
	perView->viewProjection = viewProjection;
	perView->billboardMatrix = billboardMatrix;

	auto commandList = dxCommon_->GetCommandList();
	const size_t blendIdx = static_cast<size_t>(blendMode_);
	commandList->SetPipelineState(graphicsPipelineStates_[blendIdx].Get());
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	srvManager_->SetGraphicsRootDescriptorTable(0, particleSrvIndex_);
	commandList->SetGraphicsRootConstantBufferView(1, perViewBuffer_->GetGPUVirtualAddress() + static_cast<size_t>(frameIndex) * kCbAlignment);
	srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex_);

	D3D12_RESOURCE_BARRIER barriers[3] = {};

	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = indirectArgsBuffer_.Get();
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = particleBuffer_.Get();
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[2].Transition.pResource = aliveListBuffer_.Get();
	barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(3, barriers);

	commandList->ExecuteIndirect(commandSignature_.Get(), 1, indirectArgsBuffer_.Get(), 0, nullptr, 0);

	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	commandList->ResourceBarrier(3, barriers);

	// デバッグ要求時のみGPU→CPU Readbackを実行
	if (enableReadbackDebug_ && readbackBuffer_) {
		D3D12_RESOURCE_BARRIER copyBarriers[2] = {};
		copyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		copyBarriers[0].Transition.pResource = freeListIndexBuffer_.Get();
		copyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		copyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		copyBarriers[1].Transition.pResource = indirectArgsBuffer_.Get();
		copyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		copyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		copyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		commandList->ResourceBarrier(2, copyBarriers);

		commandList->CopyBufferRegion(readbackBuffer_.Get(), 0, indirectArgsBuffer_.Get(), 0, sizeof(uint32_t) * 4);
		commandList->CopyBufferRegion(readbackBuffer_.Get(), 16, freeListIndexBuffer_.Get(), 0, sizeof(int32_t));

		copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		copyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		copyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(2, copyBarriers);
	}
}

void GPUParticleManager::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("GPU Particle System");

	if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable GPU Readback Stats", &enableReadbackDebug_);
		ImGui::Text("Max Particles: %u", kMaxParticles);

		uint32_t activeParticles = debugInstanceCount_;
		int32_t freeParticles = debugFreeListIndex_ + 1;

		ImGui::Text("Alive Particles: %u", activeParticles);
		ImGui::Text("Free Particles: %d", freeParticles);

		float util = static_cast<float>(activeParticles) / static_cast<float>(kMaxParticles);
		ImGui::ProgressBar(util, ImVec2(0.f, 0.f), std::to_string(activeParticles).c_str());
	}

	if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Translate", &guiEmitTranslate_.x, 0.1f);
		ImGui::SliderInt("Count", &guiEmitCount_, 1, static_cast<int>(kMaxParticles));
		ImGui::DragFloat("Radius", &guiEmitRadius_, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Frequency (s)", &guiEmitFrequency_, 0.01f, 0.0f, 5.0f);

		if (ImGui::Button("Manual Emit")) {
			SetEmitParams(guiEmitTranslate_, static_cast<uint32_t>(guiEmitCount_), guiEmitRadius_, guiEmitFrequency_);
			Emit();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear GPU Particles")) {
			ClearAllParticles();
		}
	}

	ImGui::End();
#endif
}
