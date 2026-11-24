#include "GPUParticleManager.h"
#include "../System/TextureManager.h"
#include <algorithm>
#include <cassert>

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

	ClearWakeFields();
	ambientVolume_ = {};
	pendingDroppedEmitRequests_ = 0u;
	lastFrameDroppedEmitRequests_ = 0u;
	needsInitializeParticles_ = true;
	isInitialized_ = true;
}

void GPUParticleManager::Finalize() {
	particleBuffer_.Reset();
	freeListIndexBuffer_.Reset();
	freeListBuffer_.Reset();
	aliveListAlphaBuffer_.Reset();
	aliveListAddBuffer_.Reset();
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
	ClearWakeFields();
	ambientVolume_ = {};
	pendingDroppedEmitRequests_ = 0u;
	lastFrameDroppedEmitRequests_ = 0u;
	isInitialized_ = false;
	needsInitializeParticles_ = true;
	hasCustomTexture_ = false;
	manualEmitRequested_ = false;
}

void GPUParticleManager::InitializeParticles() {
	auto commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();

	commandList->SetPipelineState(initPipelineState_.Get());
	commandList->SetComputeRootSignature(computeRootSignature_.Get());

	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

	const uint32_t initGroups = (kMaxParticles + kInitThreadGroupSize - 1u) / kInitThreadGroupSize;
	commandList->Dispatch(initGroups, 1, 1);

	dxCommon_->UAVBarrier(particleBuffer_.Get());
	dxCommon_->UAVBarrier(freeListIndexBuffer_.Get());
	dxCommon_->UAVBarrier(freeListBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListAlphaBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListAddBuffer_.Get());
	dxCommon_->UAVBarrier(indirectArgsBuffer_.Get());
}

void GPUParticleManager::SetWakeField(uint32_t slot, const GpuWakeField& field) {
	if (slot >= kMaxWakeFields) {
		return;
	}
	wakeFields_[slot] = field;
}

void GPUParticleManager::ClearWakeFields() {
	for (auto& wf : wakeFields_) {
		wf = GpuWakeField{};
		wf.active = 0u;
	}
}

void GPUParticleManager::EnqueueRichEmit(const GpuParticleEmitDesc& desc) {
	if (desc.count == 0u) {
		return;
	}
	if (pendingEmitRequests_.size() >= kMaxEmitRequests) {
		pendingDroppedEmitRequests_++;
		return;
	}

	GpuEmitRequest req{};
	req.origin = desc.origin;
	req.randomPositionRange = desc.randomPositionRange;
	req.minVelocity = desc.minVelocity;
	req.minLifeTime = desc.minLifeTime;
	req.maxVelocity = desc.maxVelocity;
	req.maxLifeTime = desc.maxLifeTime;
	req.acceleration = desc.acceleration;
	req.maxRiseHeight = desc.maxRiseHeight;
	req.minColor = desc.minColor;
	req.maxColor = desc.maxColor;
	req.minScale = desc.minScale;
	req.maxScale = desc.maxScale;
	req.endScale = desc.endScale;
	req.scaleEasing = desc.scaleEasing;
	req.minRotation = desc.minRotation;
	req.maxRotation = desc.maxRotation;
	req.minRotationSpeed = desc.minRotationSpeed;
	req.maxRotationSpeed = desc.maxRotationSpeed;
	req.startOffset = 0u;
	req.count = desc.count;
	req.flags = desc.flags;
	req.seedSalt = seedCounter_++;
	req.shapeType = static_cast<uint32_t>(desc.shape);
	req.drag = desc.drag;
	req.curlStrength = desc.curlStrength;
	req.curlFrequency = desc.curlFrequency;
	req.stretchFactor = desc.stretchFactor;
	req.minTemperature = desc.minTemperature;
	req.maxTemperature = desc.maxTemperature;
	req.coolingRate = desc.coolingRate;
	req.floorY = desc.useCustomFloorY ? desc.floorY : defaultFloorY_;
	req.restitution = desc.restitution;
	req.emissiveIntensity = desc.emissiveIntensity;
	req.softDistance = desc.softDistance;
	req.emitAxis = desc.emitAxis;
	req.radialSpeedMin = desc.radialSpeedMin;
	req.radialSpeedMax = desc.radialSpeedMax;
	req.ringRadius = desc.ringRadius;
	req.wakeSensitivity = desc.wakeSensitivity;
	req.padding0 = 0.0f;

	pendingEmitRequests_.push_back(req);
}

void GPUParticleManager::EnqueueEmit(const Vector3& position, const ParticleParameters& params, uint32_t count) {
	if (count == 0u) {
		return;
	}
	if (pendingEmitRequests_.size() >= kMaxEmitRequests) {
		pendingDroppedEmitRequests_++;
		return;
	}

	uint32_t flags = params.fadeOut ? GPUParticleFlags::kFadeOut : GPUParticleFlags::kNone;
	if (blendMode_ == BlendMode::kAdd || blendMode_ == BlendMode::kScreen) {
		flags |= GPUParticleFlags::kAdditive;
	}
	if (params.isStretched) {
		flags |= GPUParticleFlags::kStretched;
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
	req.flags = flags;
	req.seedSalt = seedCounter_++;
	req.shapeType = static_cast<uint32_t>(GPUParticleShape::SoftCircle);
	req.drag = 0.0f;
	req.curlStrength = 0.0f;
	req.curlFrequency = 0.12f;
	req.stretchFactor = params.stretchFactor;
	req.minTemperature = 0.0f;
	req.maxTemperature = 0.0f;
	req.coolingRate = 0.0f;
	req.floorY = defaultFloorY_;
	req.restitution = 0.45f;
	req.emissiveIntensity = 1.0f;
	req.softDistance = 3.0f;
	req.emitAxis = { 0.0f, 1.0f, 0.0f };
	req.radialSpeedMin = 0.0f;
	req.radialSpeedMax = 0.0f;
	req.ringRadius = 0.0f;
	req.wakeSensitivity = 1.0f;
	req.padding0 = 0.0f;

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
	debugAlphaInstanceCount_ = 0u;
	debugAdditiveInstanceCount_ = 0u;
	debugFreeListIndex_ = static_cast<int32_t>(kMaxParticles - 1u);
	lastFrameEmitCount_ = 0u;
	pendingDroppedEmitRequests_ = 0u;
	lastFrameDroppedEmitRequests_ = 0u;
}

void GPUParticleManager::Update() {
	if (!isInitialized_) {
		return;
	}

	// コンピュートディスパッチ前に必ずSRV/UAVディスクリプタヒープをバインド
	srvManager_->PreDraw();

	if (needsInitializeParticles_) {
		InitializeParticles();
		needsInitializeParticles_ = false;
	}

	// デバッグ時のみ前回のReadback結果を取得
	if (enableReadbackDebug_ && readbackBuffer_) {
		uint32_t* mappedData = nullptr;
		if (SUCCEEDED(readbackBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)))) {
			debugAlphaInstanceCount_ = mappedData[1];
			debugAdditiveInstanceCount_ = mappedData[5];
			debugInstanceCount_ = debugAlphaInstanceCount_ + debugAdditiveInstanceCount_;
			debugFreeListIndex_ = *reinterpret_cast<int32_t*>(&mappedData[8]);
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
	lastFrameDroppedEmitRequests_ = pendingDroppedEmitRequests_;
	pendingDroppedEmitRequests_ = 0u;

	// アクティブなウェイク場数を集計
	uint32_t activeWakeCount = 0u;
	for (uint32_t w = 0u; w < kMaxWakeFields; ++w) {
		if (wakeFields_[w].active != 0u) {
			activeWakeCount = w + 1u;
		}
	}

	// フレーム別PerFrame CBV更新
	PerFrame* perFrame = reinterpret_cast<PerFrame*>(perFrameMapped_ + static_cast<size_t>(frameIndex) * kPerFrameCbAlignment);
	perFrame->time = time_;
	perFrame->deltaTime = clampedDt;
	perFrame->requestCount = requestCount;
	perFrame->totalEmitCount = totalEmitCount;
	perFrame->ambient = ambientVolume_;
	for (uint32_t w = 0u; w < kMaxWakeFields; ++w) {
		perFrame->wakeFields[w] = wakeFields_[w];
	}
	perFrame->defaultFloorY = defaultFloorY_;
	perFrame->activeWakeCount = activeWakeCount;
	perFrame->globalCurlTimeScale = 1.0f;
	perFrame->pad0 = 0.0f;

	const D3D12_GPU_VIRTUAL_ADDRESS perFrameGpuAddr = perFrameBuffer_->GetGPUVirtualAddress() + static_cast<size_t>(frameIndex) * kPerFrameCbAlignment;
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

	// 2. Reset IndirectArgs Pass（2バケット両方をリセット）
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
	dxCommon_->UAVBarrier(aliveListAlphaBuffer_.Get());
	dxCommon_->UAVBarrier(aliveListAddBuffer_.Get());
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

	const uint32_t depthSrvIndex = dxCommon_->GetDepthSrvIndex();
	const bool useDepthSoftParticles = (depthSrvIndex != 0u && dxCommon_->GetDepthStencilBuffer() != nullptr);

	const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex() % kFrameCount;
	PerView* perView = reinterpret_cast<PerView*>(perViewMapped_ + static_cast<size_t>(frameIndex) * kCbAlignment);
	perView->viewProjection = viewProjection;
	perView->billboardMatrix = billboardMatrix;
	perView->cameraPosition = { billboardMatrix.m[3][0], billboardMatrix.m[3][1], billboardMatrix.m[3][2] };
	perView->nearClip = cameraNearClip_;
	perView->farClip = cameraFarClip_;
	perView->time = time_;
	perView->hasDepthTexture = useDepthSoftParticles ? 1u : 0u;
	perView->pad0 = 0.0f;

	auto commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 深度バッファをソフトパーティクル読取状態へ遷移
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRenderTextureRTVHandle(0);
	if (useDepthSoftParticles) {
		D3D12_RESOURCE_BARRIER depthBarrier = {};
		depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthBarrier.Transition.pResource = dxCommon_->GetDepthStencilBuffer();
		depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &depthBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsvHandle = dxCommon_->GetReadOnlyDSVHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &readOnlyDsvHandle);
	}

	// GPUパーティクルバッファをDraw用状態へ遷移
	D3D12_RESOURCE_BARRIER barriers[4] = {};

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
	barriers[2].Transition.pResource = aliveListAlphaBuffer_.Get();
	barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[3].Transition.pResource = aliveListAddBuffer_.Get();
	barriers[3].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[3].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(4, barriers);

	// 共通ルートバインド（深度未設定時は R32_FLOAT Null SRV で型一致させる）
	srvManager_->SetGraphicsRootDescriptorTable(0, particleSrvIndex_);
	commandList->SetGraphicsRootConstantBufferView(2, perViewBuffer_->GetGPUVirtualAddress() + static_cast<size_t>(frameIndex) * kCbAlignment);
	srvManager_->SetGraphicsRootDescriptorTable(3, textureSrvIndex_);
	srvManager_->SetGraphicsRootDescriptorTable(4, useDepthSoftParticles ? depthSrvIndex : nullDepthSrvIndex_);

	// Pass 1: Normal Alpha バケット（爆煙・ベイパー・吹雪・チャフ等）
	commandList->SetPipelineState(graphicsPipelineStates_[static_cast<size_t>(BlendMode::kNormal)].Get());
	srvManager_->SetGraphicsRootDescriptorTable(1, aliveListAlphaSrvIndex_);
	commandList->ExecuteIndirect(commandSignature_.Get(), 1, indirectArgsBuffer_.Get(), 0, nullptr, 0);

	// Pass 2: Additive HDR バケット（黒体爆炎・閃光・火花・衝撃波等）
	commandList->SetPipelineState(graphicsPipelineStates_[static_cast<size_t>(BlendMode::kAdd)].Get());
	srvManager_->SetGraphicsRootDescriptorTable(1, aliveListAddSrvIndex_);
	commandList->ExecuteIndirect(commandSignature_.Get(), 1, indirectArgsBuffer_.Get(), sizeof(D3D12_DRAW_ARGUMENTS), nullptr, 0);

	// UAV状態へ戻す
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[3].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	commandList->ResourceBarrier(4, barriers);

	// 深度バッファを通常 DEPTH_WRITE 状態へ復元
	if (useDepthSoftParticles) {
		D3D12_RESOURCE_BARRIER depthBarrier = {};
		depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthBarrier.Transition.pResource = dxCommon_->GetDepthStencilBuffer();
		depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		commandList->ResourceBarrier(1, &depthBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE normalDsvHandle = dxCommon_->GetDSVHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &normalDsvHandle);
	}

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

		commandList->CopyBufferRegion(readbackBuffer_.Get(), 0, indirectArgsBuffer_.Get(), 0, sizeof(uint32_t) * 8u);
		commandList->CopyBufferRegion(readbackBuffer_.Get(), sizeof(uint32_t) * 8u, freeListIndexBuffer_.Get(), 0, sizeof(int32_t));

		copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		copyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		copyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(2, copyBarriers);
	}
}
