#include "GPUParticleManager.h"
#include <cassert>
#include <vector>

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
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateBuffers();
	CreateComputePipeline();
	CreateGraphicsPipeline();
	CreateCommandSignature();
	CreateModel();
}

void GPUParticleManager::Finalize() {
	particleBuffer_.Reset();
	freeListIndexBuffer_.Reset();
	freeListBuffer_.Reset();
	aliveListBuffer_.Reset();
	indirectArgsBuffer_.Reset();
	emitterSphereBuffer_[0].Reset();
	emitterSphereBuffer_[1].Reset();
	perFrameBuffer_.Reset();
	perViewBuffer_.Reset();
	vertexBuffer_.Reset();
	readbackBuffer_.Reset();

	computeRootSignature_.Reset();
	initPipelineState_.Reset();
	emitPipelineState_.Reset();
	updatePipelineState_.Reset();

	graphicsRootSignature_.Reset();
	graphicsPipelineState_.Reset();
	commandSignature_.Reset();
}

void GPUParticleManager::CreateBuffers() {
	auto device = dxCommon_->GetDevice();

	particleUavIndex_ = srvManager_->Allocate();
	freeListIndexUavIndex_ = srvManager_->Allocate();
	freeListUavIndex_ = srvManager_->Allocate();
	aliveListUavIndex_ = srvManager_->Allocate();
	indirectArgsUavIndex_ = srvManager_->Allocate();

	particleSrvIndex_ = srvManager_->Allocate();
	aliveListSrvIndex_ = srvManager_->Allocate();

	// 1. Particle Buffer (UAV/SRV)
	particleBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(ParticleCS) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

	// 2. FreeListIndex Buffer (UAV)
	freeListIndexBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t));
	srvManager_->CreateUAVforStructuredBuffer(freeListIndexUavIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));

	// 3. FreeList Buffer (UAV)
	freeListBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(freeListUavIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	// 4. AliveList Buffer (UAV/SRV)
	aliveListBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(aliveListUavIndex_, aliveListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateSRVforStructuredBuffer(aliveListSrvIndex_, aliveListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	// 5. IndirectArgs Buffer (UAV)
	indirectArgsBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_ARGUMENTS));
	srvManager_->CreateUAVforStructuredBuffer(indirectArgsUavIndex_, indirectArgsBuffer_.Get(), 4, sizeof(uint32_t));

	// 4. EmitterSphere (Constant Buffer) -> b0
	for (int i = 0; i < 2; ++i) {
		emitterSphereBuffer_[i] = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));
		emitterSphereBuffer_[i]->Map(0, nullptr, reinterpret_cast<void**>(&emitterSphereData_[i]));
		emitterSphereData_[i]->translate = { 0.0f, 0.0f, 0.0f };
		emitterSphereData_[i]->radius = 1.0f;
		emitterSphereData_[i]->count = 100;
		emitterSphereData_[i]->frequency = 0.5f;
		emitterSphereData_[i]->frequencyTime = 0.0f;
		emitterSphereData_[i]->emit = 0;
	}

	// 5. PerFrame (Constant Buffer)
	perFrameBuffer_ = dxCommon_->CreateBufferResource(sizeof(PerFrame));
	perFrameBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 1.0f / 60.0f;

	// 6. PerView (Constant Buffer)
	perViewBuffer_ = dxCommon_->CreateBufferResource(sizeof(PerView));
	perViewBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	// 7. Readback Buffer for ImGui Debugging
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(uint32_t) * 5; // [0-3]: indirectArgs, [4]: freeListIndex
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

	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 5;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1;

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
	Microsoft::WRL::ComPtr<IDxcBlob> updateCS = dxCommon_->CompileShader(L"assets/shaders/UpdateParticle.CS.hlsl", L"cs_6_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = computeRootSignature_.Get();

	psoDesc.CS = { initCS->GetBufferPointer(), initCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&initPipelineState_));
	assert(SUCCEEDED(hr));

	psoDesc.CS = { emitCS->GetBufferPointer(), emitCS->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&emitPipelineState_));
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

	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.BlendEnable = TRUE;
	blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.DestBlend = D3D12_BLEND_ONE;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
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

	commandList->Dispatch(1, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListBuffer_.Get());
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());
}

void GPUParticleManager::SetEmitParams(const MyMath::Vector3& position, uint32_t count, float radius, float frequency) {
	EmitterSphere* currentEmitterData = emitterSphereData_[currentBufferIndex_];
	currentEmitterData->translate = position;
	currentEmitterData->count = count;
	currentEmitterData->radius = radius;
	currentEmitterData->frequency = frequency;
}

void GPUParticleManager::Emit() {
	manualEmitRequested_ = true;
}

void GPUParticleManager::Update() {
	static bool isFirstFrame = true;
	if (isFirstFrame) {
		InitializeParticles();
		isFirstFrame = false;
	}

	uint32_t* mappedData = nullptr;
	if (readbackBuffer_ && SUCCEEDED(readbackBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)))) {
		uint32_t vertexCount = mappedData[0];
		uint32_t instanceCount = mappedData[1];
		uint32_t startVertex = mappedData[2];
		uint32_t startInstance = mappedData[3];
		int32_t freeListIndex = *reinterpret_cast<int32_t*>(&mappedData[4]);

		readbackBuffer_->Unmap(0, nullptr);

		debugInstanceCount_ = instanceCount;

#ifdef USE_IMGUI
		ImGui::Begin("GPUParticle Debug");
		ImGui::Text("Alive Particles (Instance Count): %u", instanceCount);
		ImGui::Text("Vertex Count: %u", vertexCount);
		ImGui::Text("Start Vertex: %u", startVertex);
		ImGui::Text("Start Instance: %u", startInstance);
		ImGui::Text("FreeList Index: %d", freeListIndex);
		ImGui::End();
#endif // USE_IMGUI
	}

	time_ += perFrameData_->deltaTime;
	perFrameData_->time = time_;

	uint32_t nextBufferIndex = (currentBufferIndex_ + 1) % 2;
	*emitterSphereData_[nextBufferIndex] = *emitterSphereData_[currentBufferIndex_];
	currentBufferIndex_ = nextBufferIndex;
	EmitterSphere* currentEmitterData = emitterSphereData_[currentBufferIndex_];

	bool shouldEmit = manualEmitRequested_;
	manualEmitRequested_ = false; // フラグを消費

	currentEmitterData->frequencyTime += perFrameData_->deltaTime;
	if (currentEmitterData->frequency > 0.0f && currentEmitterData->frequency <= currentEmitterData->frequencyTime) {
		currentEmitterData->frequencyTime -= currentEmitterData->frequency;
		shouldEmit = true;
	}

	currentEmitterData->emit = shouldEmit ? 1 : 0;

	auto commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(emitPipelineState_.Get());
	commandList->SetComputeRootSignature(computeRootSignature_.Get());

	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

	commandList->SetComputeRootConstantBufferView(1, emitterSphereBuffer_[currentBufferIndex_]->GetGPUVirtualAddress());

	commandList->SetComputeRootConstantBufferView(2, perFrameBuffer_->GetGPUVirtualAddress());

	commandList->Dispatch(1, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());

	//---------------------------------------------------------
	//---------------------------------------------------------
	commandList->SetPipelineState(updatePipelineState_.Get());

	commandList->Dispatch(1, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListBuffer_.Get());
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());

}

void GPUParticleManager::Draw(const MyMath::Matrix4x4& viewProjection, const MyMath::Matrix4x4& billboardMatrix) {
	auto commandList = dxCommon_->GetCommandList();

	perViewData_->viewProjection = viewProjection;
	perViewData_->billboardMatrix = billboardMatrix;

	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	srvManager_->SetGraphicsRootDescriptorTable(0, particleSrvIndex_);

	commandList->SetGraphicsRootConstantBufferView(1, perViewBuffer_->GetGPUVirtualAddress());

	srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex_);

	D3D12_RESOURCE_BARRIER barriers[3] = {};

	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[0].Transition.pResource = indirectArgsBuffer_.Get();
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[1].Transition.pResource = particleBuffer_.Get();
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
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

	// --- Copy status to Readback Buffer ---
	if (readbackBuffer_) {
		D3D12_RESOURCE_BARRIER copyBarriers[2] = {};
		copyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		copyBarriers[0].Transition.pResource = freeListIndexBuffer_.Get();
		copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		copyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		copyBarriers[1].Transition.pResource = indirectArgsBuffer_.Get();
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
		ImGui::Text("Max Particles: %d", kMaxParticles);

		int32_t activeParticles = debugInstanceCount_;
		int32_t freeParticles = debugFreeListIndex_ + 1;

		ImGui::Text("Alive Particles: %d", activeParticles);
		ImGui::Text("Free Particles: %d", freeParticles);

		float util = static_cast<float>(activeParticles) / static_cast<float>(kMaxParticles);
		ImGui::ProgressBar(util, ImVec2(0.f, 0.f), std::to_string(activeParticles).c_str());
	}

	if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Translate", &guiEmitTranslate_.x, 0.1f);
		ImGui::SliderInt("Count", &guiEmitCount_, 1, kMaxParticles);
		ImGui::DragFloat("Radius", &guiEmitRadius_, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Frequency (s)", &guiEmitFrequency_, 0.01f, 0.0f, 5.0f);

		if (ImGui::Button("Manual Emit")) {
			SetEmitParams(guiEmitTranslate_, static_cast<uint32_t>(guiEmitCount_), guiEmitRadius_, guiEmitFrequency_);
			Emit();
		}
	}

	ImGui::End();
#endif
}
