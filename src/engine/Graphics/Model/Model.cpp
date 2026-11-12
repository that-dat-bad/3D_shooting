#include "Model.h"
#include "ModelCommon.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "../System/TextureManager.h"
#include "PrimitiveModel.h"
#include "../Camera/Camera.h"
#include "../../Physics/Collision3DManager.h"

#include <cassert>
#include <map>
#include <fstream>
#include <sstream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cmath>



//--補助関数群--


// 時間に合わせて「移動」を計算する
Vector3 CalculateTranslate(const std::vector<Model::KeyframeVector3>& keys, float time) {
	if (keys.empty()) { return { 0,0,0 }; }
	if (keys.size() == 1 || time <= keys.front().time) { return keys.front().value; }
	if (time >= keys.back().time) { return keys.back().value; }

	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			return Lerp(keys[i].value, keys[i + 1].value, t);
		}
	}
	return keys.back().value;
}

// 時間に合わせて「回転」を計算する
Quaternion CalculateRotate(const std::vector<Model::KeyframeQuaternion>& keys, float time) {
	if (keys.empty()) { return { 0,0,0,1 }; }
	if (keys.size() == 1 || time <= keys.front().time) { return keys.front().value; }
	if (time >= keys.back().time) { return keys.back().value; }

	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			return Slerp(keys[i].value, keys[i + 1].value, t);
		}
	}
	return keys.back().value;
}

// 時間に合わせて「拡縮」を計算する
Vector3 CalculateScale(const std::vector<Model::KeyframeVector3>& keys, float time) {
	if (keys.empty()) { return { 1,1,1 }; }
	if (keys.size() == 1 || time <= keys.front().time) { return keys.front().value; }
	if (time >= keys.back().time) { return keys.back().value; }

	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			return Lerp(keys[i].value, keys[i + 1].value, t);
		}
	}
	return keys.back().value;
}




void Model::Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename) {
	ModelData data = LoadModelFile(directorypath, filename);
	InitializeFromModelData(modelCommon, std::move(data));
}

void Model::InitializeFromModelData(ModelCommon* modelCommon, ModelData modelData) {
	modelCommon_ = modelCommon;
	DirectXCommon* dxCommon = modelCommon_->GetDirectXCommon();
	modelData_ = std::move(modelData);

	// 2. 頂点データの初期化 (GPUリソース作成)
	// 頂点がない場合のガード
	if (modelData_.vertices.empty() || modelData_.indices.empty()) {
		return;
	}

	SrvManager* srvManager = modelCommon_->GetSrvManager();

	vertexBuffer_ = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// データの書き込み
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
	vertexBuffer_->Unmap(0, nullptr);

	// ボーンを持たない静的メッシュ（地形チャンク等）では CS スキニング用リソースは不要なので生成しない
	// （UAVバッファ・パラメータCB・SRV/UAVディスクリプタの生成コストとVRAMを節約）
	if (!modelData_.bones.empty()) {
		if (srvManager && srvManager->CanAllocate()) {
			rawVertexSrvIndex_ = srvManager->Allocate();
			srvManager->CreateSRVforStructuredBuffer(rawVertexSrvIndex_, vertexBuffer_.Get(), static_cast<UINT>(modelData_.vertices.size()), sizeof(VertexData));
		}

		// CSスキニング用バッファとUAVの初期化
		skinnedVertexBuffer_ = dxCommon->CreateUAVBufferResource(sizeof(VertexData) * modelData_.vertices.size());
		skinnedVertexBufferView_.BufferLocation = skinnedVertexBuffer_->GetGPUVirtualAddress();
		skinnedVertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
		skinnedVertexBufferView_.StrideInBytes = sizeof(VertexData);

		if (srvManager && srvManager->CanAllocate()) {
			skinnedVertexUavIndex_ = srvManager->Allocate();
			srvManager->CreateUAVforStructuredBuffer(skinnedVertexUavIndex_, skinnedVertexBuffer_.Get(), static_cast<UINT>(modelData_.vertices.size()), sizeof(VertexData));
		}

		skinningParamsResource_ = dxCommon->CreateBufferResource(sizeof(SkinningParams));
		SkinningParams* params = nullptr;
		skinningParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&params));
		params->numVertices = static_cast<uint32_t>(modelData_.vertices.size());
		skinningParamsResource_->Unmap(0, nullptr);
	}

	// インデックスバッファの初期化
	indexBuffer_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());

	indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexMap = nullptr;
	indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
	std::memcpy(indexMap, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
	indexBuffer_->Unmap(0, nullptr);

	// 3. マテリアルの初期化
	if (modelData_.materials.empty()) {
		modelData_.materials.push_back(MaterialData());
	}

	materialResources_.clear();
	materialDatas_.clear();

	for (auto& matData : modelData_.materials) {
		Microsoft::WRL::ComPtr<ID3D12Resource> matRes = dxCommon->CreateBufferResource(sizeof(Material));
		Material* matPtr = nullptr;
		matRes->Map(0, nullptr, reinterpret_cast<void**>(&matPtr));

		matPtr->color = matData.baseColor;
		matPtr->enableLighting = true;
		matPtr->shininess = matData.shininess;
		matPtr->environmentCoefficient = matData.environmentCoefficient;
		matPtr->specularIntensity = matData.specularIntensity;
		matPtr->emissiveColor = matData.emissiveColor;
		matPtr->emissiveIntensity = matData.emissiveIntensity;
		matPtr->uvTransform = Identity4x4();

		if (!matData.textureFilePath.empty()) {
			TextureManager::GetInstance()->LoadTexture(matData.textureFilePath);
			matData.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(matData.textureFilePath);
		} else {
			matData.textureIndex = 0;
		}

		materialResources_.push_back(matRes);
		materialDatas_.push_back(matPtr);
	}

	//7.ボーン用のバッファ作成
	boneResource_ = dxCommon->CreateBufferResource(sizeof(BoneMatrix));
	boneResource_->Map(0, nullptr, reinterpret_cast<void**>(&boneData_));
	for (int i = 0; i < 100; i++) {
		boneData_->matrices[i] = Identity4x4();
	}

}

namespace {
	void BindMaterialAndDrawMesh(
		const Model::MeshInfo& mesh,
		ID3D12GraphicsCommandList* commandList,
		const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& materialResources,
		const std::vector<Model::MaterialData>& materials)
	{
		if (mesh.indexCount == 0) return;

		uint32_t matIdx = mesh.materialIndex;
		if (matIdx >= materialResources.size()) { matIdx = 0; }

		// 1. マテリアル定数バッファの設定 (RootParameter Index: 0)
		if (!materialResources.empty() && materialResources[matIdx]) {
			commandList->SetGraphicsRootConstantBufferView(0, materialResources[matIdx]->GetGPUVirtualAddress());
		}

		// 2. テクスチャ (DescriptorTable) の設定 (RootParameter Index: 2)
		uint32_t useTextureIndex = 0;
		if (matIdx < materials.size()) {
			useTextureIndex = materials[matIdx].textureIndex;
		}
		if (useTextureIndex == 0) {
			TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
			useTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
		}

		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(useTextureIndex);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

		// 3. メッシュ描画コマンド発行
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}

	void DrawNodeMeshesRecursive(
		const Model::Node& node,
		ID3D12GraphicsCommandList* commandList,
		const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& materialResources,
		const std::vector<Model::MaterialData>& materials)
	{
		for (const auto& mesh : node.meshes) {
			BindMaterialAndDrawMesh(mesh, commandList, materialResources, materials);
		}
		for (const auto& child : node.children) {
			DrawNodeMeshesRecursive(child, commandList, materialResources, materials);
		}
	}

	bool IntersectRayNodeMeshes(
		const Model::Node& node,
		const Ray& ray,
		const Matrix4x4& worldMatrix,
		const Model::ModelData& modelData,
		RaycastHit* outHit)
	{
		bool hitAny = false;
		RaycastHit closestHit;
		closestHit.distance = ray.maxDistance;

		for (const auto& mesh : node.meshes) {
			for (uint32_t i = 0; i < mesh.indexCount; i += 3) {
				if (mesh.indexOffset + i + 2 >= modelData.indices.size()) break;
				uint32_t idx0 = modelData.indices[mesh.indexOffset + i];
				uint32_t idx1 = modelData.indices[mesh.indexOffset + i + 1];
				uint32_t idx2 = modelData.indices[mesh.indexOffset + i + 2];

				if (idx0 >= modelData.vertices.size() || idx1 >= modelData.vertices.size() || idx2 >= modelData.vertices.size()) continue;

				Vector3 p0 = { modelData.vertices[idx0].position.x, modelData.vertices[idx0].position.y, modelData.vertices[idx0].position.z };
				Vector3 p1 = { modelData.vertices[idx1].position.x, modelData.vertices[idx1].position.y, modelData.vertices[idx1].position.z };
				Vector3 p2 = { modelData.vertices[idx2].position.x, modelData.vertices[idx2].position.y, modelData.vertices[idx2].position.z };

				Vector3 v0 = TransformV3(p0, worldMatrix);
				Vector3 v1 = TransformV3(p1, worldMatrix);
				Vector3 v2 = TransformV3(p2, worldMatrix);

				RaycastHit tempHit;
				if (Collision3DManager::RayTriangleIntersect(ray, v0, v1, v2, &tempHit)) {
					if (tempHit.distance < closestHit.distance) {
						closestHit = tempHit;
						closestHit.nodeName = node.name;
						closestHit.triangleIndex = i / 3;
						hitAny = true;
					}
				}
			}
		}

		if (hitAny && outHit) {
			*outHit = closestHit;
		}
		return hitAny;
	}

	bool IntersectRayTreeRecursive(
		const Model::Node& node,
		const Ray& ray,
		const Matrix4x4& worldMatrix,
		const Model::ModelData& modelData,
		RaycastHit* outHit)
	{
		bool hitAny = false;
		RaycastHit closestHit;
		closestHit.distance = ray.maxDistance;

		RaycastHit tempHit;
		if (IntersectRayNodeMeshes(node, ray, worldMatrix, modelData, &tempHit)) {
			if (tempHit.distance < closestHit.distance) {
				closestHit = tempHit;
				hitAny = true;
			}
		}

		for (const auto& child : node.children) {
			if (IntersectRayTreeRecursive(child, ray, worldMatrix, modelData, &tempHit)) {
				if (tempHit.distance < closestHit.distance) {
					closestHit = tempHit;
					hitAny = true;
				}
			}
		}

		if (hitAny && outHit) {
			*outHit = closestHit;
		}
		return hitAny;
	}

	const Model::Node* FindNodeByName(const Model::Node& node, const std::string& name) {
		if (node.name == name) return &node;
		for (const auto& child : node.children) {
			const Model::Node* found = FindNodeByName(child, name);
			if (found) return found;
		}
		return nullptr;
	}
}

void Model::Draw() {
	if (modelData_.vertices.empty()) { return; }

	// コマンドリストの取得
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDirectXCommon()->GetCommandList();
	SrvManager* srvManager = modelCommon_->GetSrvManager();

	D3D12_VERTEX_BUFFER_VIEW* vbView = &vertexBufferView_;

	// ボーンが存在する場合、Compute Shader でスキニング計算を実行
	if (!modelData_.bones.empty() && skinnedVertexBuffer_ && srvManager) {
		commandList->SetPipelineState(modelCommon_->GetSkinningPipelineState());
		commandList->SetComputeRootSignature(modelCommon_->GetSkinningRootSignature());

		commandList->SetComputeRootConstantBufferView(0, skinningParamsResource_->GetGPUVirtualAddress());
		commandList->SetComputeRootConstantBufferView(1, boneResource_->GetGPUVirtualAddress());

		srvManager->SetComputeRootDescriptorTable(2, rawVertexSrvIndex_);
		srvManager->SetComputeRootDescriptorTable(3, skinnedVertexUavIndex_);

		UINT dispatchX = (static_cast<UINT>(modelData_.vertices.size()) + 127) / 128;
		commandList->Dispatch(dispatchX, 1, 1);

		modelCommon_->GetDirectXCommon()->UAVBarrier(skinnedVertexBuffer_.Get());
		vbView = &skinnedVertexBufferView_;
	}

	// 頂点バッファ・インデックスバッファ・ボーンバッファの設定
	commandList->IASetVertexBuffers(0, 1, vbView);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(7, boneResource_->GetGPUVirtualAddress());

	// ルートノード配下の全メッシュをマテリアル切り替えしながら描画
	DrawNodeMeshesRecursive(modelData_.rootNode, commandList, materialResources_, modelData_.materials);
}

void Model::DrawNode(const std::string& nodeName) {
	if (modelData_.vertices.empty()) { return; }

	DrawNodeRecursive(modelData_.rootNode, nodeName, false);
}

void Model::DrawNodeRecursive(const Node& node, const std::string& targetName, bool isTargetFound) {
	if (node.name == targetName) {
		ID3D12GraphicsCommandList* commandList = modelCommon_->GetDirectXCommon()->GetCommandList();
		SrvManager* srvManager = modelCommon_->GetSrvManager();

		D3D12_VERTEX_BUFFER_VIEW* vbView = &vertexBufferView_;

		if (!modelData_.bones.empty() && skinnedVertexBuffer_ && srvManager) {
			commandList->SetPipelineState(modelCommon_->GetSkinningPipelineState());
			commandList->SetComputeRootSignature(modelCommon_->GetSkinningRootSignature());

			commandList->SetComputeRootConstantBufferView(0, skinningParamsResource_->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(1, boneResource_->GetGPUVirtualAddress());

			srvManager->SetComputeRootDescriptorTable(2, rawVertexSrvIndex_);
			srvManager->SetComputeRootDescriptorTable(3, skinnedVertexUavIndex_);

			UINT dispatchX = (static_cast<UINT>(modelData_.vertices.size()) + 127) / 128;
			commandList->Dispatch(dispatchX, 1, 1);

			modelCommon_->GetDirectXCommon()->UAVBarrier(skinnedVertexBuffer_.Get());
			vbView = &skinnedVertexBufferView_;
		}

		// 頂点バッファ・インデックスバッファ・ボーンバッファの設定
		commandList->IASetVertexBuffers(0, 1, vbView);
		commandList->IASetIndexBuffer(&indexBufferView_);
		commandList->SetGraphicsRootConstantBufferView(7, boneResource_->GetGPUVirtualAddress());

		// ターゲットノード自身の全メッシュを描画
		for (const auto& mesh : node.meshes) {
			BindMaterialAndDrawMesh(mesh, commandList, materialResources_, modelData_.materials);
		}
	}

	for (const auto& child : node.children) {
		DrawNodeRecursive(child, targetName, false);
	}
}



Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = filename;
	if (!directoryPath.empty()) {
		filePath = directoryPath + "/" + filename;
	}
	//テクスチャのためにディレクトリを抽出
	std::string baseDirectory = std::filesystem::path(filePath).parent_path().string();

	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices);
	if (!scene || !scene->HasMeshes()) {
		return modelData;
	}

	std::function<Model::Node(aiNode*, const aiMatrix4x4&)> processNode = [&](aiNode* node, const aiMatrix4x4& parentTransform) -> Model::Node {
		Model::Node resultNode;
		resultNode.name = node->mName.C_Str();

		aiMatrix4x4 aiLocalMatrix = node->mTransformation;
		aiLocalMatrix.Transpose();
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				resultNode.localMatrix.m[i][j] = aiLocalMatrix[i][j];
			}
		}

		// 行列の掛け合わせの順序は (ローカル * 親)
		aiMatrix4x4 globalTransform = node->mTransformation * parentTransform;

		for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			bool hasNormals = mesh->HasNormals();
			bool hasTexCoords = mesh->HasTextureCoords(0);

			std::vector<std::vector<std::pair<uint32_t, float>>> vertexWeightMap(mesh->mNumVertices);

			if (mesh->HasBones()) {
				for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
					aiBone* ai_bone = mesh->mBones[boneIndex];
					std::string boneName = ai_bone->mName.C_Str();

					// 1. ボーンを登録してインデックスを決める
					uint32_t myBoneIndex = 0;
					if (!modelData.boneIndexMap.contains(boneName)) {
						myBoneIndex = static_cast<uint32_t>(modelData.bones.size());
						modelData.boneIndexMap[boneName] = myBoneIndex;

						ModelData::BoneData boneData;
						boneData.name = boneName;
						aiMatrix4x4 m = ai_bone->mOffsetMatrix;
						boneData.inverseBindPoseMatrix = {
							m.a1, m.b1, m.c1, m.d1,
							m.a2, m.b2, m.c2, m.d2,
							m.a3, m.b3, m.c3, m.d3,
							m.a4, m.b4, m.c4, m.d4
						};
						modelData.bones.push_back(boneData);
					} else {
						myBoneIndex = modelData.boneIndexMap[boneName];
					}

					// 2. このボーンが影響を与える頂点を、事前整理リストに追加していく
					for (uint32_t weightIndex = 0; weightIndex < ai_bone->mNumWeights; ++weightIndex) {
						const aiVertexWeight& weight = ai_bone->mWeights[weightIndex];
						vertexWeightMap[weight.mVertexId].push_back({ myBoneIndex, weight.mWeight });
					}
				}
			}
			// =================================================================

			uint32_t indexOffset = static_cast<uint32_t>(modelData.vertices.size());

			// 頂点の抽出
			for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
				aiVector3D position = mesh->mVertices[vertexIndex];


				VertexData vertex;
				vertex.position = { position.x, position.y, position.z, 1.0f };

				if (hasNormals) {
					aiVector3D normal = mesh->mNormals[vertexIndex];
					aiMatrix3x3 normalMatrix = aiMatrix3x3(globalTransform);
					normal.Normalize();
					vertex.normal = { normal.x, normal.y, normal.z };
				} else {
					vertex.normal = { 0.0f, 1.0f, 0.0f };
				}

				if (hasTexCoords) {
					aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
					vertex.texcoord = { texcoord.x, texcoord.y };
				} else {
					vertex.texcoord = { 0.0f, 0.0f };
				}

				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;

				// アニメーション関連初期化 (ゼロクリア)
				vertex.weight = { 1.0f, 0.0f, 0.0f, 0.0f };
				vertex.indices[0] = 0;
				vertex.indices[1] = 0;
				vertex.indices[2] = 0;
				vertex.indices[3] = 0;

				auto& weights = vertexWeightMap[vertexIndex];
				for (size_t w = 0; w < weights.size(); ++w) {
					if (w >= 4) { break; }

					if (w == 0) { vertex.weight.x = weights[w].second; vertex.indices[0] = weights[w].first; } else if (w == 1) { vertex.weight.y = weights[w].second; vertex.indices[1] = weights[w].first; } else if (w == 2) { vertex.weight.z = weights[w].second; vertex.indices[2] = weights[w].first; } else if (w == 3) { vertex.weight.w = weights[w].second; vertex.indices[3] = weights[w].first; }
				}
				// =================================================================

				modelData.vertices.push_back(vertex);
			}

			uint32_t meshIndexStart = static_cast<uint32_t>(modelData.indices.size());

			// 面（三角形）の展開 (インデックス抽出)
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
				aiFace& face = mesh->mFaces[faceIndex];
				if (face.mNumIndices != 3) { continue; } // 三角形以外はスキップ

				for (uint32_t element = 0; element < face.mNumIndices; ++element) {
					modelData.indices.push_back(face.mIndices[element] + indexOffset);
				}
			}

			uint32_t meshIndexCount = static_cast<uint32_t>(modelData.indices.size()) - meshIndexStart;
			if (meshIndexCount > 0) {
				MeshInfo mInfo;
				mInfo.indexOffset = meshIndexStart;
				mInfo.indexCount = meshIndexCount;
				mInfo.materialIndex = mesh->mMaterialIndex;
				resultNode.meshes.push_back(mInfo);
			}
		}

		for (uint32_t i = 0; i < node->mNumChildren; ++i) {
			resultNode.children.push_back(processNode(node->mChildren[i], globalTransform));
		}
		return resultNode;
		};

	aiMatrix4x4 identity;
	modelData.rootNode = processNode(scene->mRootNode, identity);

	if (!modelData.vertices.empty()) {
		float minX = modelData.vertices[0].position.x;
		float maxX = minX;
		float minY = modelData.vertices[0].position.y;
		float maxY = minY;
		float minZ = modelData.vertices[0].position.z;
		float maxZ = minZ;
		for (const auto& v : modelData.vertices) {
			if (v.position.x < minX) { minX = v.position.x; }
			if (v.position.x > maxX) { maxX = v.position.x; }
			if (v.position.y < minY) { minY = v.position.y; }
			if (v.position.y > maxY) { maxY = v.position.y; }
			if (v.position.z < minZ) { minZ = v.position.z; }
			if (v.position.z > maxZ) { maxZ = v.position.z; }
		}
		char buf[256];
		sprintf_s(buf, "[Model.cpp] Bound Box for %s: X[%.3f, %.3f], Y[%.3f, %.3f], Z[%.3f, %.3f], Verts:%zu\n",
			filePath.c_str(), minX, maxX, minY, maxY, minZ, maxZ, modelData.vertices.size());
		OutputDebugStringA(buf);
	}

	// ディレクトリパスの取得 (テクスチャ読み込み用)
	size_t pos = filePath.find_last_of('/');
	if (pos != std::string::npos) {
		baseDirectory = filePath.substr(0, pos);
	}

	if (scene->HasMaterials()) {
		for (uint32_t matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx) {
			aiMaterial* material = scene->mMaterials[matIdx];
			MaterialData matData;

			if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
				aiString textureFilePath;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);

				if (textureFilePath.C_Str()[0] == '*') {
					uint32_t textureIndex = static_cast<uint32_t>(std::stoi(textureFilePath.C_Str() + 1));

					if (textureIndex < scene->mNumTextures) {
						aiTexture* embeddedTexture = scene->mTextures[textureIndex];
						std::string embeddedTexName = filePath + "_tex" + std::to_string(textureIndex);

						if (embeddedTexture->mHeight == 0) {
							TextureManager::GetInstance()->LoadTextureFromMemory(
								embeddedTexName,
								embeddedTexture->pcData,
								embeddedTexture->mWidth
							);
						}
						matData.textureFilePath = embeddedTexName;
					}
				} else {
					std::string texPath;
					if (!baseDirectory.empty()) {
						texPath = baseDirectory + "/" + textureFilePath.C_Str();
					} else {
						texPath = textureFilePath.C_Str();
					}

					if (!std::filesystem::exists(texPath)) {
						std::string texName = std::filesystem::path(textureFilePath.C_Str()).filename().string();
						std::string fallback = "assets/textures/" + texName;
						if (std::filesystem::exists(fallback)) {
							texPath = fallback;
						}
					}
					matData.textureFilePath = texPath;
				}
			}

			aiColor4D color;
			if (AI_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, color)) {
				matData.baseColor = { color.r, color.g, color.b, color.a };
			} else if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
				matData.baseColor = { color.r, color.g, color.b, color.a };
			}

			float roughness = 1.0f;
			if (AI_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) {
				if (roughness >= 0.99f) {
					matData.shininess = 1.0f;
				} else {
					matData.shininess = 2.0f / powf(roughness, 4.0f) - 2.0f;
					if (matData.shininess < 1.0f) { matData.shininess = 1.0f; }
					if (matData.shininess > 256.0f) { matData.shininess = 256.0f; }
				}
			} else {
				float shininess = 0.0f;
				if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess)) {
					if (shininess < 1.0f) { shininess = 1.0f; }
					if (shininess > 256.0f) { shininess = 256.0f; }
					matData.shininess = shininess;
				}
			}

			aiColor4D emissiveColor;
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor)) {
				matData.emissiveColor = { emissiveColor.r, emissiveColor.g, emissiveColor.b };
				matData.emissiveIntensity = 1.0f;
			}

			modelData.materials.push_back(matData);
		}
	}
	if (modelData.materials.empty()) {
		modelData.materials.push_back(MaterialData());
	}




	return modelData;
}

// ============================================================================
// 軽量OBJローダー（Assimp非経由）
// - 地形チャンクのような「テクスチャ少・ボーン無し・大量ファイル」の静的メッシュ向け
// - Assimp 経路 (FlipWindingOrder | FlipUVs | Triangulate | JoinIdenticalVertices + X反転) と同じ頂点規約で出力
// ============================================================================
namespace {
	// 空白スキップ
	inline const char* SkipSpaces(const char* p, const char* end) {
		while (p < end && (*p == ' ' || *p == '\t')) { ++p; }
		return p;
	}
	// 行末までスキップ（改行文字の次へ）
	inline const char* SkipLine(const char* p, const char* end) {
		while (p < end && *p != '\n') { ++p; }
		return (p < end) ? p + 1 : end;
	}
	// 高速 float パース（Debugビルドでも strtof より大幅に速い）
	inline const char* ParseFloat(const char* p, const char* end, float& out) {
		p = SkipSpaces(p, end);
		bool neg = false;
		if (p < end && (*p == '-' || *p == '+')) { neg = (*p == '-'); ++p; }
		double value = 0.0;
		while (p < end && *p >= '0' && *p <= '9') { value = value * 10.0 + (*p - '0'); ++p; }
		if (p < end && *p == '.') {
			++p;
			double scale = 0.1;
			while (p < end && *p >= '0' && *p <= '9') { value += (*p - '0') * scale; scale *= 0.1; ++p; }
		}
		if (p < end && (*p == 'e' || *p == 'E')) {
			++p;
			bool expNeg = false;
			if (p < end && (*p == '-' || *p == '+')) { expNeg = (*p == '-'); ++p; }
			int e = 0;
			while (p < end && *p >= '0' && *p <= '9') { e = e * 10 + (*p - '0'); ++p; }
			value *= std::pow(10.0, expNeg ? -e : e);
		}
		out = static_cast<float>(neg ? -value : value);
		return p;
	}
	inline const char* ParseInt(const char* p, const char* end, int& out) {
		bool neg = false;
		if (p < end && (*p == '-' || *p == '+')) { neg = (*p == '-'); ++p; }
		int v = 0;
		while (p < end && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; }
		out = neg ? -v : v;
		return p;
	}
	// 行の残り（トークン）を取得（前後空白・CR除去）
	inline std::string ReadRestOfLine(const char* p, const char* end) {
		p = SkipSpaces(p, end);
		const char* s = p;
		while (p < end && *p != '\n' && *p != '\r') { ++p; }
		const char* e = p;
		while (e > s && (e[-1] == ' ' || e[-1] == '\t')) { --e; }
		return std::string(s, e);
	}
	inline bool ReadWholeFile(const std::string& path, std::string& out) {
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		if (!ifs) { return false; }
		std::streamsize size = ifs.tellg();
		if (size <= 0) { return false; }
		out.resize(static_cast<size_t>(size));
		ifs.seekg(0);
		ifs.read(out.data(), size);
		return true;
	}

	struct ObjVertexKey {
		int v, vt, vn;
		bool operator==(const ObjVertexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
	};
	struct ObjVertexKeyHash {
		size_t operator()(const ObjVertexKey& k) const {
			uint64_t h = static_cast<uint32_t>(k.v);
			h = h * 0x9E3779B97F4A7C15ull ^ static_cast<uint32_t>(k.vt);
			h = h * 0x9E3779B97F4A7C15ull ^ static_cast<uint32_t>(k.vn);
			return static_cast<size_t>(h ^ (h >> 29));
		}
	};

	// .mtl の読み込み（Kd / d / Ns / Ke / map_Kd のみ対応）
	void ParseMtlFile(const std::string& mtlPath, const std::string& baseDir,
		std::unordered_map<std::string, Model::MaterialData>& outMaterials) {
		std::string buf;
		if (!ReadWholeFile(mtlPath, buf)) { return; }
		const char* p = buf.data();
		const char* end = p + buf.size();
		Model::MaterialData* cur = nullptr;
		while (p < end) {
			p = SkipSpaces(p, end);
			if (p >= end) { break; }
			if (std::strncmp(p, "newmtl", 6) == 0 && (p + 6 < end) && (p[6] == ' ' || p[6] == '\t')) {
				std::string name = ReadRestOfLine(p + 6, end);
				cur = &outMaterials[name];
			} else if (cur && p + 2 < end && p[0] == 'K' && p[1] == 'd' && (p[2] == ' ' || p[2] == '\t')) {
				const char* q = p + 2;
				q = ParseFloat(q, end, cur->baseColor.x);
				q = ParseFloat(q, end, cur->baseColor.y);
				q = ParseFloat(q, end, cur->baseColor.z);
			} else if (cur && p + 1 < end && p[0] == 'd' && (p[1] == ' ' || p[1] == '\t')) {
				ParseFloat(p + 1, end, cur->baseColor.w);
			} else if (cur && p + 2 < end && p[0] == 'N' && p[1] == 's' && (p[2] == ' ' || p[2] == '\t')) {
				float ns = 0.0f;
				ParseFloat(p + 2, end, ns);
				cur->shininess = (std::min)((std::max)(ns, 1.0f), 256.0f);
			} else if (cur && p + 2 < end && p[0] == 'K' && p[1] == 'e' && (p[2] == ' ' || p[2] == '\t')) {
				const char* q = p + 2;
				q = ParseFloat(q, end, cur->emissiveColor.x);
				q = ParseFloat(q, end, cur->emissiveColor.y);
				q = ParseFloat(q, end, cur->emissiveColor.z);
				cur->emissiveIntensity = 1.0f; // Assimp 経路と同じ扱い
			} else if (cur && std::strncmp(p, "map_Kd", 6) == 0) {
				std::string tex = ReadRestOfLine(p + 6, end);
				if (!tex.empty()) {
					std::string texPath = baseDir.empty() ? tex : (baseDir + "/" + tex);
					if (!std::filesystem::exists(texPath)) {
						std::string fallback = "assets/textures/" + std::filesystem::path(tex).filename().string();
						if (std::filesystem::exists(fallback)) { texPath = fallback; }
					}
					cur->textureFilePath = texPath;
				}
			}
			p = SkipLine(p, end);
		}
	}
}

Model::ModelData Model::LoadObjFileFast(const std::string& filePath) {
	ModelData modelData;

	std::string buf;
	if (!ReadWholeFile(filePath, buf)) {
		return modelData;
	}
	std::string baseDirectory = std::filesystem::path(filePath).parent_path().string();
	std::replace(baseDirectory.begin(), baseDirectory.end(), '\\', '/');

	// 生データ（OBJは1始まりインデックス）
	std::vector<Vector3> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	// 概算で予約（1行 ~30byte 程度）
	positions.reserve(buf.size() / 64);
	normals.reserve(buf.size() / 64);

	// マテリアル
	std::unordered_map<std::string, MaterialData> mtlLibrary;
	std::unordered_map<std::string, uint32_t> materialIndexByName;
	std::vector<std::string> materialNames;
	uint32_t currentMaterial = 0;
	auto getMaterialIndex = [&](const std::string& name) -> uint32_t {
		auto it = materialIndexByName.find(name);
		if (it != materialIndexByName.end()) { return it->second; }
		uint32_t idx = static_cast<uint32_t>(materialNames.size());
		materialNames.push_back(name);
		materialIndexByName.emplace(name, idx);
		return idx;
	};
	bool hasAnyMaterial = false;

	// オブジェクト（o）ごと → マテリアルごとの三角形インデックス
	struct ObjGroup {
		std::string name;
		std::map<uint32_t, std::vector<uint32_t>> trisByMaterial;
	};
	std::vector<ObjGroup> groups;
	groups.push_back({ "default", {} });

	std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> vertexCache;
	vertexCache.reserve(buf.size() / 64);
	modelData.vertices.reserve(buf.size() / 64);

	auto resolveIndex = [](int idx, size_t count) -> int {
		// 負数は末尾からの相対指定
		if (idx < 0) { return static_cast<int>(count) + idx; }
		return idx - 1;
	};

	auto emitVertex = [&](const ObjVertexKey& key) -> uint32_t {
		auto it = vertexCache.find(key);
		if (it != vertexCache.end()) { return it->second; }

		VertexData vertex{};
		const Vector3& pos = positions[key.v];
		// Assimp 経路と同じ: X反転
		vertex.position = { -pos.x, pos.y, pos.z, 1.0f };
		if (key.vt >= 0 && key.vt < static_cast<int>(texcoords.size())) {
			// aiProcess_FlipUVs 相当
			vertex.texcoord = { texcoords[key.vt].x, 1.0f - texcoords[key.vt].y };
		} else {
			vertex.texcoord = { 0.0f, 0.0f };
		}
		if (key.vn >= 0 && key.vn < static_cast<int>(normals.size())) {
			Vector3 n = normals[key.vn];
			float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			if (len > 0.0f) { n.x /= len; n.y /= len; n.z /= len; }
			vertex.normal = { -n.x, n.y, n.z };
		} else {
			vertex.normal = { 0.0f, 1.0f, 0.0f };
		}
		vertex.weight = { 1.0f, 0.0f, 0.0f, 0.0f };
		vertex.indices[0] = vertex.indices[1] = vertex.indices[2] = vertex.indices[3] = 0;

		uint32_t newIndex = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.push_back(vertex);
		vertexCache.emplace(key, newIndex);
		return newIndex;
	};

	const char* p = buf.data();
	const char* end = p + buf.size();
	uint32_t faceVerts[64];

	while (p < end) {
		p = SkipSpaces(p, end);
		if (p >= end) { break; }
		const char c0 = *p;
		const char c1 = (p + 1 < end) ? p[1] : '\0';

		if (c0 == 'v' && (c1 == ' ' || c1 == '\t')) {
			Vector3 v{};
			const char* q = p + 1;
			q = ParseFloat(q, end, v.x);
			q = ParseFloat(q, end, v.y);
			q = ParseFloat(q, end, v.z);
			positions.push_back(v);
		} else if (c0 == 'v' && c1 == 'n') {
			Vector3 n{};
			const char* q = p + 2;
			q = ParseFloat(q, end, n.x);
			q = ParseFloat(q, end, n.y);
			q = ParseFloat(q, end, n.z);
			normals.push_back(n);
		} else if (c0 == 'v' && c1 == 't') {
			Vector2 t{};
			const char* q = p + 2;
			q = ParseFloat(q, end, t.x);
			q = ParseFloat(q, end, t.y);
			texcoords.push_back(t);
		} else if (c0 == 'f' && (c1 == ' ' || c1 == '\t')) {
			const char* q = p + 1;
			uint32_t count = 0;
			while (true) {
				q = SkipSpaces(q, end);
				if (q >= end || *q == '\r' || *q == '\n') { break; }
				int vi = 0, ti = 0, ni = 0;
				bool hasT = false, hasN = false;
				q = ParseInt(q, end, vi);
				if (q < end && *q == '/') {
					++q;
					if (q < end && *q != '/') { q = ParseInt(q, end, ti); hasT = true; }
					if (q < end && *q == '/') { ++q; q = ParseInt(q, end, ni); hasN = true; }
				}
				// 不正トークンで無限ループしないよう、数字以外は読み飛ばす
				while (q < end && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') { ++q; }

				ObjVertexKey key;
				key.v = resolveIndex(vi, positions.size());
				key.vt = hasT ? resolveIndex(ti, texcoords.size()) : -1;
				key.vn = hasN ? resolveIndex(ni, normals.size()) : -1;
				if (key.v < 0 || key.v >= static_cast<int>(positions.size())) { continue; }
				if (count < 64) { faceVerts[count++] = emitVertex(key); }
			}
			if (count >= 3) {
				auto& tris = groups.back().trisByMaterial[currentMaterial];
				// 扇形三角形分割 (0,i,i+1) + FlipWindingOrder 相当で逆順に格納
				for (uint32_t i = 1; i + 1 < count; ++i) {
					tris.push_back(faceVerts[i + 1]);
					tris.push_back(faceVerts[i]);
					tris.push_back(faceVerts[0]);
				}
			}
		} else if (c0 == 'o' && (c1 == ' ' || c1 == '\t')) {
			groups.push_back({ ReadRestOfLine(p + 1, end), {} });
		} else if (std::strncmp(p, "usemtl", 6) == 0) {
			currentMaterial = getMaterialIndex(ReadRestOfLine(p + 6, end));
			hasAnyMaterial = true;
		} else if (std::strncmp(p, "mtllib", 6) == 0) {
			std::string mtlName = ReadRestOfLine(p + 6, end);
			std::string mtlPath = baseDirectory.empty() ? mtlName : (baseDirectory + "/" + mtlName);
			ParseMtlFile(mtlPath, baseDirectory, mtlLibrary);
		}
		p = SkipLine(p, end);
	}

	if (modelData.vertices.empty()) {
		return modelData;
	}

	// マテリアル配列の構築（usemtl 出現順）
	if (!hasAnyMaterial) {
		modelData.materials.push_back(MaterialData());
	} else {
		for (const auto& name : materialNames) {
			auto it = mtlLibrary.find(name);
			modelData.materials.push_back(it != mtlLibrary.end() ? it->second : MaterialData());
		}
	}

	// ノード構築: root の子として o ごとのノード、各ノードにマテリアルごとの MeshInfo
	modelData.rootNode.name = std::filesystem::path(filePath).filename().string();
	size_t totalIndices = 0;
	for (const auto& g : groups) {
		for (const auto& [mat, tris] : g.trisByMaterial) { totalIndices += tris.size(); }
	}
	modelData.indices.reserve(totalIndices);

	for (auto& g : groups) {
		if (g.trisByMaterial.empty()) { continue; }
		Node child;
		child.name = g.name;
		for (auto& [mat, tris] : g.trisByMaterial) {
			if (tris.empty()) { continue; }
			MeshInfo info;
			info.indexOffset = static_cast<uint32_t>(modelData.indices.size());
			info.indexCount = static_cast<uint32_t>(tris.size());
			info.materialIndex = mat;
			modelData.indices.insert(modelData.indices.end(), tris.begin(), tris.end());
			child.meshes.push_back(info);
		}
		modelData.rootNode.children.push_back(std::move(child));
	}

	return modelData;
}

Model::Animation Model::LoadAnimationFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName) {
	Animation animation;
	Assimp::Importer importer;
	std::string filePath = filename;
	if (!directoryPath.empty()) {
		filePath = directoryPath + "/" + filename;
	}

	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
	if (!scene || scene->mNumAnimations == 0) {
		std::string errorOut = "Assimp Error: No animation found\n";
		errorOut += importer.GetErrorString();
		errorOut += "\nFailed to load: " + filePath;
		MessageBoxA(nullptr, errorOut.c_str(), "Animation Load Error", MB_OK | MB_ICONERROR);
		return animation;
	}

	aiAnimation* aiAnimation = nullptr;

	if (!animationName.empty()) {
		// 名前が指定されている場合はループして探す
		for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
			if (std::string(scene->mAnimations[i]->mName.C_Str()) == animationName) {
				aiAnimation = scene->mAnimations[i];
				break;
			}
		}
	}
	if (!aiAnimation) {
		aiAnimation = scene->mAnimations[0];
	}

	float ticksPerSecond = 1.0f;
	if (aiAnimation->mTicksPerSecond != 0.0) {
		ticksPerSecond = float(aiAnimation->mTicksPerSecond);
	}
	animation.duration = float(aiAnimation->mDuration) / ticksPerSecond;

	for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex) {
		aiNodeAnim* nodeAnim = aiAnimation->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnim->mNodeName.C_Str()];

		for (uint32_t keyframeIndex = 0; keyframeIndex < nodeAnim->mNumPositionKeys; ++keyframeIndex) {
			aiVectorKey& key = nodeAnim->mPositionKeys[keyframeIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(key.mTime) / ticksPerSecond;
			keyframe.value = { float(key.mValue.x), float(key.mValue.y), float(key.mValue.z) };
			nodeAnimation.translate.push_back(keyframe);
		}

		for (uint32_t keyframeIndex = 0; keyframeIndex < nodeAnim->mNumRotationKeys; ++keyframeIndex) {
			aiQuatKey& key = nodeAnim->mRotationKeys[keyframeIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(key.mTime) / ticksPerSecond;
			keyframe.value = { float(key.mValue.x), float(key.mValue.y), float(key.mValue.z), float(key.mValue.w) };
			nodeAnimation.rotate.push_back(keyframe);
		}

		for (uint32_t keyframeIndex = 0; keyframeIndex < nodeAnim->mNumScalingKeys; ++keyframeIndex) {
			aiVectorKey& key = nodeAnim->mScalingKeys[keyframeIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(key.mTime) / ticksPerSecond;
			keyframe.value = { float(key.mValue.x), float(key.mValue.y), float(key.mValue.z) };
			nodeAnimation.scale.push_back(keyframe);
		}
	}

	return animation;
}

std::vector<std::string> Model::LoadAnimationNames(const std::string& directoryPath, const std::string& filename,const std::string& animationName) {
	std::vector<std::string> names;
	Assimp::Importer importer;
	std::string filePath = filename;
	if (!directoryPath.empty()) {
		filePath = directoryPath + "/" + filename;
	}

	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
	if (!scene || scene->mNumAnimations == 0) {
		return names;
	}

	for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
		names.push_back(scene->mAnimations[i]->mName.C_Str());
	}

	return names;
}

void Model::PlayAnimation(Animation* animation) {
	playingAnimation_ = animation;
	animationTime_ = 0.0f;
}

void Model::Update(float deltaTime) {
	if (!playingAnimation_) { return; }

	// 1. 時間を進める
	animationTime_ += deltaTime;
	// ループ再生：再生時間がアニメーションの長さを超えたら最初に戻る
	animationTime_ = std::fmod(animationTime_, playingAnimation_->duration);

	// 2. 全てのボーンの行列を計算する
	// ここで階層構造(Node)をたどって、今のポーズの行列を作成
	UpdateNodeAnimation(modelData_.rootNode, Identity4x4());
}

void Model::UpdateNodeAnimation(const Node& node, const Matrix4x4& parentMatrix) {
	Matrix4x4 localMatrix = node.localMatrix;

	// もしアニメーションデータが存在するなら、時間に合わせて計算する
	if (playingAnimation_ && playingAnimation_->nodeAnimations.contains(node.name)) {
		const NodeAnimation& anim = playingAnimation_->nodeAnimations.at(node.name);

		Vector3 scale = CalculateScale(anim.scale, animationTime_);
		Quaternion rotate = CalculateRotate(anim.rotate, animationTime_);
		Vector3 translate = CalculateTranslate(anim.translate, animationTime_);

		localMatrix = MakeAffineMatrix(scale, rotate, translate);
	}

	// ワールド行列の計算
	Matrix4x4 globalMatrix = localMatrix * parentMatrix;

	// ボーンの行列を書き込む
	if (modelData_.boneIndexMap.contains(node.name)) {
		uint32_t index = modelData_.boneIndexMap[node.name];
		boneData_->matrices[index] = modelData_.bones[index].inverseBindPoseMatrix * globalMatrix;
	}

	// 子供たちへ再帰
	for (const auto& child : node.children) {
		UpdateNodeAnimation(child, globalMatrix);
	}
}

void Model::DebugDrawSkeleton(const Matrix4x4& objectWorldMatrix, Camera* camera, const Vector4& color) {
	if (modelData_.rootNode.children.empty()) { return; }
	// ルートノードから再帰的に描画
	DebugDrawNodeSkeleton(modelData_.rootNode, Identity4x4(), objectWorldMatrix, camera, color);
}

void Model::DebugDrawNodeSkeleton(const Node& node, const Matrix4x4& parentMatrix, const Matrix4x4& objectWorldMatrix, Camera* camera, const Vector4& color) {
	Matrix4x4 localMatrix = node.localMatrix;

	if (playingAnimation_ && playingAnimation_->nodeAnimations.contains(node.name)) {
		const NodeAnimation& anim = playingAnimation_->nodeAnimations.at(node.name);

		Vector3 scale = CalculateScale(anim.scale, animationTime_);
		Quaternion rotate = CalculateRotate(anim.rotate, animationTime_);
		Vector3 translate = CalculateTranslate(anim.translate, animationTime_);

		localMatrix = MakeAffineMatrix(scale, rotate, translate);
	}

	Matrix4x4 globalMatrix = localMatrix * parentMatrix;

	// 親のワールド座標
	Matrix4x4 parentFinal = parentMatrix * objectWorldMatrix;
	Vector3 parentPos = { parentFinal.m[3][0], parentFinal.m[3][1], parentFinal.m[3][2] };

	// 自分のワールド座標
	Matrix4x4 myFinal = globalMatrix * objectWorldMatrix;
	Vector3 myPos = { myFinal.m[3][0], myFinal.m[3][1], myFinal.m[3][2] };

	// 線を描画（親と自分の位置が同じなら描画しない。ルートの初回など）
	float distSq = (myPos.x - parentPos.x) * (myPos.x - parentPos.x) +
				   (myPos.y - parentPos.y) * (myPos.y - parentPos.y) +
				   (myPos.z - parentPos.z) * (myPos.z - parentPos.z);
	
	if (distSq > 0.0001f) {
		PrimitiveModel::GetInstance()->DrawLine3D(parentPos, myPos, color, camera);
	}

	for (const auto& child : node.children) {
		DebugDrawNodeSkeleton(child, globalMatrix, objectWorldMatrix, camera, color);
	}
}

bool Model::IntersectRay(const Ray& ray, const Matrix4x4& worldMatrix, RaycastHit* outHit) const {
	if (modelData_.vertices.empty() || modelData_.indices.empty()) {
		return false;
	}
	return IntersectRayTreeRecursive(modelData_.rootNode, ray, worldMatrix, modelData_, outHit);
}

bool Model::IntersectRayNode(const std::string& nodeName, const Ray& ray, const Matrix4x4& nodeWorldMatrix, RaycastHit* outHit) const {
	if (modelData_.vertices.empty() || modelData_.indices.empty()) {
		return false;
	}
	const Node* targetNode = FindNodeByName(modelData_.rootNode, nodeName);
	if (!targetNode) {
		return false;
	}
	return IntersectRayNodeMeshes(*targetNode, ray, nodeWorldMatrix, modelData_, outHit);
}

void Model::SetMaterialColor(uint32_t materialIndex, const Vector4& color) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->color = color;
	}
}

void Model::SetMaterialEnableLighting(uint32_t materialIndex, bool enableLighting) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->enableLighting = enableLighting ? 1 : 0;
	}
}

void Model::SetMaterialSpecularIntensity(uint32_t materialIndex, float intensity) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->specularIntensity = intensity;
	}
}

void Model::SetMaterialEmissive(uint32_t materialIndex, const Vector3& color, float intensity) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->emissiveColor = color;
		materialDatas_[materialIndex]->emissiveIntensity = intensity;
	}
}

void Model::SetMaterialShininess(uint32_t materialIndex, float shininess) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->shininess = shininess;
	}
}

void Model::SetMaterialEnvironmentCoefficient(uint32_t materialIndex, float coefficient) {
	if (materialIndex < materialDatas_.size() && materialDatas_[materialIndex]) {
		materialDatas_[materialIndex]->environmentCoefficient = coefficient;
	}
}

