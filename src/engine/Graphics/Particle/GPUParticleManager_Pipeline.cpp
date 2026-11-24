#include "GPUParticleManager.h"
#include <cassert>
#include <cstring>
#include <string>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif // USE_IMGUI

void GPUParticleManager::CreateBuffers() {
	// u0..u5 用に連続した6個のUAVディスクリプタを割り当て
	particleUavIndex_ = srvManager_->Allocate();
	freeListIndexUavIndex_ = srvManager_->Allocate();
	freeListUavIndex_ = srvManager_->Allocate();
	aliveListAlphaUavIndex_ = srvManager_->Allocate();
	aliveListAddUavIndex_ = srvManager_->Allocate();
	indirectArgsUavIndex_ = srvManager_->Allocate();

	// グラフィックスVS用SRV（t0: Particle, t1: Alpha AliveList / Add AliveList）
	particleSrvIndex_ = srvManager_->Allocate();
	aliveListAlphaSrvIndex_ = srvManager_->Allocate();
	aliveListAddSrvIndex_ = srvManager_->Allocate();

	// 深度SRV未初期化時の型一致フォールバック用 Null Texture2D<float> SRV
	nullDepthSrvIndex_ = srvManager_->Allocate();
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC nullDepthDesc{};
		nullDepthDesc.Format = DXGI_FORMAT_R32_FLOAT;
		nullDepthDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		nullDepthDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		nullDepthDesc.Texture2D.MipLevels = 1;
		dxCommon_->GetDevice()->CreateShaderResourceView(
			nullptr,
			&nullDepthDesc,
			srvManager_->GetCPUDescriptorHandle(nullDepthSrvIndex_));
	}

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

	// 4. AliveList Buffers (2バケット: Alpha / Additive)
	aliveListAlphaBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(aliveListAlphaUavIndex_, aliveListAlphaBuffer_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateSRVforStructuredBuffer(aliveListAlphaSrvIndex_, aliveListAlphaBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	aliveListAddBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(uint32_t) * kMaxParticles);
	srvManager_->CreateUAVforStructuredBuffer(aliveListAddUavIndex_, aliveListAddBuffer_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateSRVforStructuredBuffer(aliveListAddSrvIndex_, aliveListAddBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	// 5. IndirectArgs Buffer (2バケット分の D3D12_DRAW_ARGUMENTS = 8 uint32)
	indirectArgsBuffer_ = dxCommon_->CreateUAVBufferResource(sizeof(D3D12_DRAW_ARGUMENTS) * 2u);
	srvManager_->CreateUAVforStructuredBuffer(indirectArgsUavIndex_, indirectArgsBuffer_.Get(), 8u, sizeof(uint32_t));

	// 6. EmitRequest Buffers (フレーム別二重化)
	for (uint32_t i = 0; i < kFrameCount; ++i) {
		emitRequestSrvIndex_[i] = srvManager_->Allocate();
		emitRequestBuffer_[i] = dxCommon_->CreateBufferResource(sizeof(GpuEmitRequest) * kMaxEmitRequests);
		emitRequestBuffer_[i]->Map(0, nullptr, reinterpret_cast<void**>(&emitRequestMapped_[i]));
		std::memset(emitRequestMapped_[i], 0, sizeof(GpuEmitRequest) * kMaxEmitRequests);
		srvManager_->CreateSRVforStructuredBuffer(emitRequestSrvIndex_[i], emitRequestBuffer_[i].Get(), kMaxEmitRequests, sizeof(GpuEmitRequest));
	}

	// 7. PerFrame (フレーム別二重化 / 512バイトアライメント)
	perFrameBuffer_ = dxCommon_->CreateBufferResource(kPerFrameCbAlignment * kFrameCount);
	perFrameBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameMapped_));
	std::memset(perFrameMapped_, 0, kPerFrameCbAlignment * kFrameCount);

	// 8. PerView (フレーム別二重化 / 256バイトアライメント)
	perViewBuffer_ = dxCommon_->CreateBufferResource(kCbAlignment * kFrameCount);
	perViewBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perViewMapped_));
	std::memset(perViewMapped_, 0, kCbAlignment * kFrameCount);

	// 9. Readback Buffer (デバッグ要求時のみ使用: IndirectArgs[8] + FreeListIndex[1])
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(uint32_t) * 9u;
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
	uavRange[0].NumDescriptors = 6; // u0..u5
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

	D3D12_ROOT_PARAMETER rootParameters[5] = {};

	// [0] VS t0: gParticles
	D3D12_DESCRIPTOR_RANGE rangeParticles[1] = {};
	rangeParticles[0].BaseShaderRegister = 0;
	rangeParticles[0].NumDescriptors = 1;
	rangeParticles[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeParticles[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].DescriptorTable.pDescriptorRanges = rangeParticles;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// [1] VS t1: gAliveList (バケット切替用)
	D3D12_DESCRIPTOR_RANGE rangeAlive[1] = {};
	rangeAlive[0].BaseShaderRegister = 1;
	rangeAlive[0].NumDescriptors = 1;
	rangeAlive[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeAlive[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = rangeAlive;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2] ALL b0: gPerView
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	// [3] PS t0: gTexture
	D3D12_DESCRIPTOR_RANGE rangeTex[1] = {};
	rangeTex[0].BaseShaderRegister = 0;
	rangeTex[0].NumDescriptors = 1;
	rangeTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = rangeTex;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	// [4] PS t1: gDepthTexture (ソフトパーティクル用)
	D3D12_DESCRIPTOR_RANGE rangeDepth[1] = {};
	rangeDepth[0].BaseShaderRegister = 1;
	rangeDepth[0].NumDescriptors = 1;
	rangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = rangeDepth;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
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

void GPUParticleManager::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("GPU Particle System");

	if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable GPU Readback Stats", &enableReadbackDebug_);
		ImGui::Text("Max Particles: %u | Max Emit Requests/Frame: %u", kMaxParticles, kMaxEmitRequests);

		uint32_t activeParticles = debugInstanceCount_;
		int32_t freeParticles = debugFreeListIndex_ + 1;

		ImGui::Text("Alive Total: %u (Alpha: %u | Additive HDR: %u)",
			activeParticles, debugAlphaInstanceCount_, debugAdditiveInstanceCount_);
		ImGui::Text("Free Particles: %d | Dropped Requests: %u", freeParticles, lastFrameDroppedEmitRequests_);

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
