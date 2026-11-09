#include "ParticleManager.h"
#include "../../base/WinApp.h"
#include "../System/TextureManager.h"
#include "../Camera/CameraManager.h"
#include <cassert>
#include <cmath>
#include <d3dcompiler.h>
#include <algorithm>

#pragma comment(lib, "d3dcompiler.lib")

using namespace MyMath;

std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

ParticleManager* ParticleManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<ParticleManager>();
	}
	return instance_.get();
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// ランダムエンジンの初期化
	std::random_device seed_gen;
	randomEngine_.seed(seed_gen());

	// パイプライン生成
	CreateRootSignature();
	CreateGraphicsPipeline();

	// 板ポリゴン（モデル）生成
	CreateModel();

	// カメラ用定数バッファの生成
	cameraBuffer_ = dxCommon_->CreateBufferResource(sizeof(CameraData));
	cameraBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cameraDataPtr_));
}

void ParticleManager::Update() {
	// カメラ情報の取得
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();

	// カメラ定数データの更新
	if (cameraDataPtr_) {
		cameraDataPtr_->nearClip = 0.1f; // 固定値でも良いですが実際の値に合わせてください
		cameraDataPtr_->farClip = 1000.0f; // 実際の FarClip
		cameraDataPtr_->screenWidth = static_cast<float>(WinApp::kClientWidth); // WinAppのサイズ
		cameraDataPtr_->screenHeight = static_cast<float>(WinApp::kClientHeight);
	}

	Matrix4x4 cameraWorldMatrix = camera->GetWorldMatrix();
	Matrix4x4 billboardMatrix = Identity4x4();
	billboardMatrix.m[0][0] = cameraWorldMatrix.m[0][0];
	billboardMatrix.m[0][1] = cameraWorldMatrix.m[0][1];
	billboardMatrix.m[0][2] = cameraWorldMatrix.m[0][2];
	billboardMatrix.m[1][0] = cameraWorldMatrix.m[1][0];
	billboardMatrix.m[1][1] = cameraWorldMatrix.m[1][1];
	billboardMatrix.m[1][2] = cameraWorldMatrix.m[1][2];
	billboardMatrix.m[2][0] = cameraWorldMatrix.m[2][0];
	billboardMatrix.m[2][1] = cameraWorldMatrix.m[2][1];
	billboardMatrix.m[2][2] = cameraWorldMatrix.m[2][2];


	// 全てのパーティクルグループについて処理
	for (auto& [name, group] : particleGroups_) {

		// グループ内のパーティクルリストを走査
		group.instanceCount = 0; // 描画カウントリセット

		for (auto it = group.particles.begin(); it != group.particles.end();) {

			// 寿命チェック
			if (it->currentTime >= it->lifeTime) {
				it = group.particles.erase(it);
				continue;
			}

			// フィールドの影響計算
			Vector3 fieldAcceleration = { 0.0f, 0.0f, 0.0f };
			for (const auto& [fieldName, field] : accelerationFields_) {
				if (it->transform.translate.x >= field.area.min.x && it->transform.translate.x <= field.area.max.x &&
					it->transform.translate.y >= field.area.min.y && it->transform.translate.y <= field.area.max.y &&
					it->transform.translate.z >= field.area.min.z && it->transform.translate.z <= field.area.max.z) {
					fieldAcceleration = Add(fieldAcceleration, field.acceleration);
				}
			}

			// 移動処理 (速度を加算: パーティクルの個別加速度 + フィールド加速度)
			Vector3 effectiveAccel = Add(it->acceleration, fieldAcceleration);
			if (it->maxRiseHeight > ParticleHeightCapDefaults::kNoHeightLimit) {
				const float currentRise = it->transform.translate.y - it->spawnY;
				const float heightRatio = std::clamp(
					currentRise / it->maxRiseHeight,
					ParticleHeightCapDefaults::kNoHeightLimit,
					ParticleHeightCapDefaults::kUnitRatio);
				if (effectiveAccel.y > ParticleHeightCapDefaults::kNoHeightLimit) {
					effectiveAccel.y *= (ParticleHeightCapDefaults::kUnitRatio - heightRatio);
				}
			}

			it->velocity = Add(it->velocity, effectiveAccel);

			if (it->maxRiseHeight > ParticleHeightCapDefaults::kNoHeightLimit) {
				const float currentRise = it->transform.translate.y - it->spawnY;
				const float heightRatio = std::clamp(
					currentRise / it->maxRiseHeight,
					ParticleHeightCapDefaults::kNoHeightLimit,
					ParticleHeightCapDefaults::kUnitRatio);
				if (heightRatio > ParticleHeightCapDefaults::kSoftBrakeStartRatio &&
					it->velocity.y > ParticleHeightCapDefaults::kNoHeightLimit) {
					const float brakeT = std::clamp(
						(heightRatio - ParticleHeightCapDefaults::kSoftBrakeStartRatio) /
						ParticleHeightCapDefaults::kSoftBrakeSpanRatio,
						ParticleHeightCapDefaults::kNoHeightLimit,
						ParticleHeightCapDefaults::kUnitRatio);
					it->velocity.y *= (ParticleHeightCapDefaults::kUnitRatio - brakeT);
				}
			}

			it->transform.translate = Add(it->transform.translate, it->velocity);

			if (it->maxRiseHeight > ParticleHeightCapDefaults::kNoHeightLimit) {
				const float ceilingY = it->spawnY + it->maxRiseHeight;
				if (it->transform.translate.y > ceilingY) {
					it->transform.translate.y = ceilingY;
					if (it->velocity.y > ParticleHeightCapDefaults::kNoHeightLimit) {
						it->velocity.y = ParticleHeightCapDefaults::kNoHeightLimit;
					}
				}
			}

			// 経過時間を加算
			it->currentTime += 1.0f / 60.0f; // 60FPS想定

			// 回転の更新
			it->transform.rotate = Add(it->transform.rotate, Multiply(1.0f / 60.0f, it->angularVelocity));

			// ============================
			// ライフタイムベースのアニメーション
			// ============================
			float t = it->currentTime / it->lifeTime; // 0.0 ~ 1.0
			if (t > 1.0f) { t = 1.0f; }

			// スケール補間（イージング付き）
			float easedT = std::pow(t, it->scaleEasing);
			float currentScale = it->startScale + (it->endScale - it->startScale) * easedT;
			it->transform.scale = { currentScale, currentScale, currentScale };

			// アルファフェードアウト
			it->color.w = it->startAlpha * (1.0f - t);

			// インスタンス数が最大を超えたらそれ以上処理しない（描画しない）
			if (group.instanceCount < kMaxInstanceCount_) {

				// ワールド行列の計算
				Matrix4x4 scaleMatrix = MakeScaleMatrix(it->transform.scale);
				Matrix4x4 translateMatrix = MakeTranslateMatrix(it->transform.translate);
				
				Matrix4x4 finalBillboard = billboardMatrix;
				
				if (it->isStretched) {
					Vector3 camToParticle = Subtract(it->transform.translate, camera->GetTranslate());
					if (Length(camToParticle) > 0.001f) {
						camToParticle = Normalize(camToParticle);
						
						// ストレッチ方向の基準と強度を決定
						Vector3 stretchVec = { 0.0f, 0.0f, 0.0f };
						float stretchAmount = 0.0f;
						if (Length(it->stretchDir) > 0.001f) {
							stretchVec = it->stretchDir;
							stretchAmount = Length(it->stretchDir);
						} else {
							stretchVec = it->velocity;
							stretchAmount = Length(it->velocity);
						}

						if (stretchAmount > 0.001f) {
							Vector3 velDir = Normalize(stretchVec);
							// 速度（または指定方向）ベクトルをスクリーン平面（法線camToParticle）に投影
							float dot = Dot(velDir, camToParticle);
							Vector3 projVel = Subtract(velDir, Multiply(dot, camToParticle));
							if (Length(projVel) > 0.0001f) {
								projVel = Normalize(projVel);
								Vector3 right = Normalize(Cross(camToParticle, projVel));
								
								// 進行方向（Y軸）と右方向（X軸）でカスタムビルボード行列を構築
								finalBillboard.m[0][0] = right.x;
								finalBillboard.m[0][1] = right.y;
								finalBillboard.m[0][2] = right.z;
								finalBillboard.m[1][0] = projVel.x;
								finalBillboard.m[1][1] = projVel.y;
								finalBillboard.m[1][2] = projVel.z;
								finalBillboard.m[2][0] = -camToParticle.x; // 手前を向く
								finalBillboard.m[2][1] = -camToParticle.y;
								finalBillboard.m[2][2] = -camToParticle.z;
								
								scaleMatrix.m[1][1] *= (1.0f + stretchAmount * it->stretchFactor);
							}
						}
					}
				}

				Matrix4x4 rotateZMatrix = MakeRotateZMatrix(it->transform.rotate.z);
				Matrix4x4 worldMatrix = Multiply(scaleMatrix, Multiply(rotateZMatrix, Multiply(finalBillboard, translateMatrix)));

				Matrix4x4 wvp = Multiply(worldMatrix, viewProjectionMatrix);

				// インスタンシングデータへの書き込み
				group.instancingDataPtr[group.instanceCount].WVP = wvp;
				group.instancingDataPtr[group.instanceCount].color = it->color;

				group.instanceCount++;
			}

			++it;
		}
	}
}

void ParticleManager::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();
	// ルートシグネチャの設定
	commandList->SetGraphicsRootSignature(rootSignature_.Get());


	// プリミティブトポロジーの設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 深度バッファを SRV 用 (PIXEL_SHADER_RESOURCE | DEPTH_READ) に遷移させる
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = dxCommon_->GetDepthStencilBuffer();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRenderTextureRTVHandle(0);
	D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsvHandle = dxCommon_->GetReadOnlyDSVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &readOnlyDsvHandle);

	// 全てのパーティクルグループについて処理
	BlendMode currentBlendMode = BlendMode::kCountOf; // 初期値として無効な値をセット

	for (auto& [name, group] : particleGroups_) {
		if (group.instanceCount == 0) { continue; }

		// ブレンドモードが切り替わったらPSO再設定
		if (group.blendMode != currentBlendMode) {
			commandList->SetPipelineState(graphicsPipelineStates_[static_cast<size_t>(group.blendMode)].Get());
			currentBlendMode = group.blendMode;
		}

		// テクスチャのSRVを設定
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = srvManager_->GetGPUDescriptorHandle(group.textureSrvIndex);
		commandList->SetGraphicsRootDescriptorTable(0, textureHandle);

		// インスタンシングデータのSRVを設定
		D3D12_GPU_DESCRIPTOR_HANDLE instancingHandle = srvManager_->GetGPUDescriptorHandle(group.instancingSrvIndex);
		commandList->SetGraphicsRootDescriptorTable(1, instancingHandle);

		// 深度テクスチャのSRVを設定 (t2)
		D3D12_GPU_DESCRIPTOR_HANDLE depthHandle = srvManager_->GetGPUDescriptorHandle(dxCommon_->GetDepthSrvIndex());
		commandList->SetGraphicsRootDescriptorTable(2, depthHandle);

		// カメラ用定数バッファを設定 (b0)
		commandList->SetGraphicsRootConstantBufferView(3, cameraBuffer_->GetGPUVirtualAddress());

		// インスタンシング描画
		commandList->DrawInstanced(6, group.instanceCount, 0, 0);
	}

	// 深度バッファの状態を DEPTH_WRITE に戻す
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE normalDsvHandle = dxCommon_->GetDSVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &normalDsvHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
}

void ParticleManager::SetBlendMode(const std::string& name, BlendMode mode) {
	if (particleGroups_.contains(name)) {
		particleGroups_[name].blendMode = mode;
	}
}

void ParticleManager::AddAccelerationField(const std::string& name, const Vector3& acceleration, const AABB& area) {
	AccelerationField field;
	field.acceleration = acceleration;
	field.area = area;
	accelerationFields_[name] = field;
}

void ParticleManager::ClearAllParticles() {
	for (auto& [name, group] : particleGroups_) {
		group.particles.clear();
		group.instanceCount = ParticleHeightCapDefaults::kZeroCount;
	}
}

uint32_t ParticleManager::GetTotalActiveParticleCount() const {
	uint32_t total = ParticleHeightCapDefaults::kZeroCount;
	for (const auto& [name, group] : particleGroups_) {
		total += static_cast<uint32_t>(group.particles.size());
	}
	return total;
}

uint32_t ParticleManager::GetGroupActiveParticleCount(const std::string& name) const {
	auto it = particleGroups_.find(name);
	if (it == particleGroups_.end()) {
		return ParticleHeightCapDefaults::kZeroCount;
	}
	return static_cast<uint32_t>(it->second.particles.size());
}


void ParticleManager::Finalize() {
	// リソースの解放など
	particleGroups_.clear();
	instance_.reset();
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath) {
	// 登録済みの名前かチェック
	if (particleGroups_.contains(name)) {
		return;
	}

	// 新たなグループを作成
	ParticleGroup& group = particleGroups_[name];

	// マテリアルデータ設定
	group.textureFilePath = textureFilePath;

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group.textureSrvIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// インスタンシング用リソースの生成 (StructuredBuffer)
	// サイズ = データ1個分サイズ * 最大数
	uint32_t sizeInBytes = sizeof(ParticleInstancingData) * kMaxInstanceCount_;
	group.instancingResource = dxCommon_->CreateBufferResource(sizeInBytes);

	// データを書き込むためのポインタを取得 (Mapしたままにする)
	group.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instancingDataPtr));

	// インスタンシング用にSRVを確保
	group.instancingSrvIndex = srvManager_->Allocate();

	srvManager_->CreateSRVforStructuredBuffer(
		group.instancingSrvIndex,
		group.instancingResource.Get(),
		kMaxInstanceCount_,
		sizeof(ParticleInstancingData)
	);

	group.instanceCount = 0;
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
	ParticleParameters params; // デフォルトパラメータ
	Emit(name, position, params, count);
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, const ParticleParameters& params, uint32_t count) {
	// 登録済みのグループかチェック
	assert(particleGroups_.contains(name));

	ParticleGroup& group = particleGroups_[name];

	std::uniform_real_distribution<float> distVelX((std::min)(params.minVelocity.x, params.maxVelocity.x), (std::max)(params.minVelocity.x, params.maxVelocity.x));
	std::uniform_real_distribution<float> distVelY((std::min)(params.minVelocity.y, params.maxVelocity.y), (std::max)(params.minVelocity.y, params.maxVelocity.y));
	std::uniform_real_distribution<float> distVelZ((std::min)(params.minVelocity.z, params.maxVelocity.z), (std::max)(params.minVelocity.z, params.maxVelocity.z));

	std::uniform_real_distribution<float> distColorR((std::min)(params.minColor.x, params.maxColor.x), (std::max)(params.minColor.x, params.maxColor.x));
	std::uniform_real_distribution<float> distColorG((std::min)(params.minColor.y, params.maxColor.y), (std::max)(params.minColor.y, params.maxColor.y));
	std::uniform_real_distribution<float> distColorB((std::min)(params.minColor.z, params.maxColor.z), (std::max)(params.minColor.z, params.maxColor.z));
	std::uniform_real_distribution<float> distColorA((std::min)(params.minColor.w, params.maxColor.w), (std::max)(params.minColor.w, params.maxColor.w));

	std::uniform_real_distribution<float> distTime((std::min)(params.minLifeTime, params.maxLifeTime), (std::max)(params.minLifeTime, params.maxLifeTime));

	// スケールのランダム分布
	std::uniform_real_distribution<float> distScale((std::min)(params.minScale, params.maxScale), (std::max)(params.minScale, params.maxScale));

	// 位置のランダム分布（0.0に設定された場合はズレを無くす）
	std::uniform_real_distribution<float> distPos(-params.randomPositionRange, params.randomPositionRange);

	for (uint32_t i = 0; i < count; ++i) {
		Particle particle{};
		particle.transform.scale = { 1.0f, 1.0f, 1.0f };
		particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

		// 位置に少しランダム要素を加える
		Vector3 randomPos = { distPos(randomEngine_), distPos(randomEngine_), distPos(randomEngine_) };
		particle.transform.translate = Add(position, randomPos);
		particle.spawnY = particle.transform.translate.y;
		particle.maxRiseHeight = params.maxRiseHeight;

		// 速度設定
		particle.velocity = { distVelX(randomEngine_), distVelY(randomEngine_), distVelZ(randomEngine_) };

		// 回転設定
		std::uniform_real_distribution<float> distRot((std::min)(params.minRotation, params.maxRotation), (std::max)(params.minRotation, params.maxRotation));
		std::uniform_real_distribution<float> distRotSpeed((std::min)(params.minRotationSpeed, params.maxRotationSpeed), (std::max)(params.minRotationSpeed, params.maxRotationSpeed));
		particle.transform.rotate.z = distRot(randomEngine_);
		particle.angularVelocity.z = distRotSpeed(randomEngine_);

		particle.acceleration = params.acceleration;

		particle.color = { distColorR(randomEngine_), distColorG(randomEngine_), distColorB(randomEngine_), distColorA(randomEngine_) };
		particle.lifeTime = distTime(randomEngine_);
		particle.currentTime = 0.0f;

		// アニメーションパラメータの設定
		particle.startScale = distScale(randomEngine_);
		particle.endScale = params.endScale;
		particle.startAlpha = particle.color.w; // 初期アルファを保存
		particle.scaleEasing = params.scaleEasing;

		particle.isStretched = params.isStretched;
		particle.stretchFactor = params.stretchFactor;
		particle.stretchDir = params.stretchDir;

		// 初期スケールを適用
		particle.transform.scale = { particle.startScale, particle.startScale, particle.startScale };

		group.particles.push_back(particle);
	}
}

void ParticleManager::CreateModel() {
	VertexData vertices[6];


	Vector4 leftBottom = { -0.5f, -0.5f, 0.0f, 1.0f };
	Vector4 leftTop = { -0.5f,  0.5f, 0.0f, 1.0f };
	Vector4 rightBottom = { 0.5f, -0.5f, 0.0f, 1.0f };
	Vector4 rightTop = { 0.5f,  0.5f, 0.0f, 1.0f };

	Vector2 uvLeftBottom = { 0.0f, 1.0f };
	Vector2 uvLeftTop = { 0.0f, 0.0f };
	Vector2 uvRightBottom = { 1.0f, 1.0f };
	Vector2 uvRightTop = { 1.0f, 0.0f };

	vertices[0].position = leftBottom;  vertices[0].texcoord = uvLeftBottom;
	vertices[1].position = leftTop;     vertices[1].texcoord = uvLeftTop;
	vertices[2].position = rightBottom; vertices[2].texcoord = uvRightBottom;
	vertices[3].position = leftTop;     vertices[3].texcoord = uvLeftTop;
	vertices[4].position = rightTop;    vertices[4].texcoord = uvRightTop;
	vertices[5].position = rightBottom; vertices[5].texcoord = uvRightBottom;

	uint32_t sizeInBytes = sizeof(VertexData) * 6;

	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeInBytes);

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeInBytes;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// データ転送
	VertexData* data = nullptr;
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&data));
	std::memcpy(data, vertices, sizeInBytes);
	vertexBuffer_->Unmap(0, nullptr);
}


void ParticleManager::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; 
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeInstancing[1] = {};
	descriptorRangeInstancing[0].BaseShaderRegister = 1;
	descriptorRangeInstancing[0].NumDescriptors = 1;
	descriptorRangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeDepth[1] = {};
	descriptorRangeDepth[0].BaseShaderRegister = 2;
	descriptorRangeDepth[0].NumDescriptors = 1;
	descriptorRangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// テクスチャ用 (t0)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// インスタンシングデータ用 (StructuredBuffer, t1)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstancing);

	// 深度バッファ用 (t2)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeDepth;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeDepth);

	// 定数バッファ (b0) - カメラパラメータ用
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0;

	// サンプラー設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズと生成
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}



void ParticleManager::CreateGraphicsPipeline() {
	// シェーダーコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"./assets/shaders/Particle.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"./assets/shaders/Particle.PS.hlsl", L"ps_6_0");

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	// ラスタライザステート
	graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 両面描画
	graphicsPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	// デプスステンシル
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 全ブレンドモードのPSOを生成
	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCountOf); ++i) {
		BlendMode mode = static_cast<BlendMode>(i);
		graphicsPipelineStateDesc.BlendState = GetBlendDesc(mode);

		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i]));
		assert(SUCCEEDED(hr));
	}
}
