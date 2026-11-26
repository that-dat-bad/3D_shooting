#include "AeroVaporMeshRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

#include "../../engine/Graphics/Model/Model.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/System/TextureManager.h"

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

using namespace MyMath;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
	constexpr const char* kNoiseMaskPath = "assets/masks/noise0.png";
	constexpr int kWingSpanSegs = 18;
	constexpr int kWingChordSegs = 18;

	struct WingTri {
		Vector3 v0;
		Vector3 v1;
		Vector3 v2;
	};

	struct WingSlice {
		float xAbs = 0.0f;
		float zLead = 0.0f;
		float zTrail = 0.0f;
		float yUpper = 0.12f;
		bool valid = false;
	};

	bool IsWingNodeName(const std::string& name, int& outSideSignHint) {
		static constexpr const char* kLeftPrefixes[] = { "Wing_L", "Wing1_L", "Wing2_L" };
		static constexpr const char* kRightPrefixes[] = { "Wing_R", "Wing1_R", "Wing2_R" };
		for (const char* prefix : kLeftPrefixes) {
			const std::string p(prefix);
			if (name == p || name.starts_with(p + ".")) {
				outSideSignHint = -1;
				return true;
			}
		}
		for (const char* prefix : kRightPrefixes) {
			const std::string p(prefix);
			if (name == p || name.starts_with(p + ".")) {
				outSideSignHint = 1;
				return true;
			}
		}
		return false;
	}

	void CollectWingTrianglesRecursive(
		const Model::Node& node,
		int inheritedSideHint,
		const Model::ModelData& modelData,
		const Matrix4x4& baseRotMatrix,
		std::vector<WingTri>& outLeftTris,
		std::vector<WingTri>& outRightTris) {

		int sideHint = inheritedSideHint;
		int matchedHint = 0;
		if (IsWingNodeName(node.name, matchedHint)) {
			sideHint = matchedHint;
		}

		if (sideHint != 0) {
			for (const auto& mesh : node.meshes) {
				for (uint32_t i = 0; i + 2 < mesh.indexCount; i += 3) {
					const size_t i0 = static_cast<size_t>(mesh.indexOffset) + i;
					const size_t i1 = i0 + 1;
					const size_t i2 = i0 + 2;
					if (i2 >= modelData.indices.size()) {
						break;
					}
					const uint32_t idx0 = modelData.indices[i0];
					const uint32_t idx1 = modelData.indices[i1];
					const uint32_t idx2 = modelData.indices[i2];
					if (idx0 >= modelData.vertices.size() ||
					    idx1 >= modelData.vertices.size() ||
					    idx2 >= modelData.vertices.size()) {
						continue;
					}

					const auto& rv0 = modelData.vertices[idx0].position;
					const auto& rv1 = modelData.vertices[idx1].position;
					const auto& rv2 = modelData.vertices[idx2].position;

					const Vector3 v0 = TransformV3({ rv0.x, rv0.y, rv0.z }, baseRotMatrix);
					const Vector3 v1 = TransformV3({ rv1.x, rv1.y, rv1.z }, baseRotMatrix);
					const Vector3 v2 = TransformV3({ rv2.x, rv2.y, rv2.z }, baseRotMatrix);

					const float avgX = (v0.x + v1.x + v2.x) / 3.0f;
					if (avgX < 0.0f) {
						outLeftTris.push_back({ v0, v1, v2 });
					} else {
						outRightTris.push_back({ v0, v1, v2 });
					}
				}
			}
		}

		for (const auto& child : node.children) {
			CollectWingTrianglesRecursive(child, sideHint, modelData, baseRotMatrix, outLeftTris, outRightTris);
		}
	}

	std::vector<WingSlice> ExtractWingSlices(const std::vector<WingTri>& tris) {
		std::vector<WingSlice> slices(kWingSpanSegs + 1);
		if (tris.empty()) {
			return slices;
		}

		float xMin = 1e9f;
		float xMax = 0.0f;
		for (const auto& tri : tris) {
			const float ax[3] = { std::fabs(tri.v0.x), std::fabs(tri.v1.x), std::fabs(tri.v2.x) };
			for (float x : ax) {
				xMin = (std::min)(xMin, x);
				xMax = (std::max)(xMax, x);
			}
		}

		const float spanLen = xMax - xMin;
		if (spanLen < 0.1f) {
			return {};
		}

		const float xStart = xMin + (std::min)(0.02f, spanLen * 0.05f);
		const float xEnd = xMax - (std::min)(0.16f, spanLen * 0.06f);

		for (int iu = 0; iu <= kWingSpanSegs; ++iu) {
			const float u = static_cast<float>(iu) / static_cast<float>(kWingSpanSegs);
			const float xTarget = std::lerp(xStart, xEnd, u);
			WingSlice slice{};
			slice.xAbs = xTarget;
			slice.zLead = -1e9f;
			slice.zTrail = 1e9f;
			slice.yUpper = -1e9f;

			for (const auto& tri : tris) {
				const Vector3 pts[3] = { tri.v0, tri.v1, tri.v2 };
				for (int e = 0; e < 3; ++e) {
					const Vector3& a = pts[e];
					const Vector3& b = pts[(e + 1) % 3];
					const float ax = std::fabs(a.x);
					const float bx = std::fabs(b.x);
					if ((ax <= xTarget && bx >= xTarget) || (bx <= xTarget && ax >= xTarget)) {
						const float denom = bx - ax;
						const float t = (std::fabs(denom) > 1e-6f) ? std::clamp((xTarget - ax) / denom, 0.0f, 1.0f) : 0.5f;
						const Vector3 p = Lerp(a, b, t);
						slice.zLead = (std::max)(slice.zLead, p.z);
						slice.zTrail = (std::min)(slice.zTrail, p.z);
						slice.yUpper = (std::max)(slice.yUpper, p.y);
						slice.valid = true;
					}
				}
			}

			slices[iu] = slice;
		}

		// 未交差スライスがあれば近傍から補間
		for (int iu = 0; iu <= kWingSpanSegs; ++iu) {
			if (slices[iu].valid) {
				continue;
			}
			int leftIdx = iu - 1;
			while (leftIdx >= 0 && !slices[leftIdx].valid) { --leftIdx; }
			int rightIdx = iu + 1;
			while (rightIdx <= kWingSpanSegs && !slices[rightIdx].valid) { ++rightIdx; }
			if (leftIdx >= 0 && rightIdx <= kWingSpanSegs) {
				const float t = static_cast<float>(iu - leftIdx) / static_cast<float>(rightIdx - leftIdx);
				slices[iu].zLead = std::lerp(slices[leftIdx].zLead, slices[rightIdx].zLead, t);
				slices[iu].zTrail = std::lerp(slices[leftIdx].zTrail, slices[rightIdx].zTrail, t);
				slices[iu].yUpper = std::lerp(slices[leftIdx].yUpper, slices[rightIdx].yUpper, t);
				slices[iu].valid = true;
			} else if (leftIdx >= 0) {
				slices[iu].zLead = slices[leftIdx].zLead;
				slices[iu].zTrail = slices[leftIdx].zTrail;
				slices[iu].yUpper = slices[leftIdx].yUpper;
				slices[iu].valid = true;
			} else if (rightIdx <= kWingSpanSegs) {
				slices[iu].zLead = slices[rightIdx].zLead;
				slices[iu].zTrail = slices[rightIdx].zTrail;
				slices[iu].yUpper = slices[rightIdx].yUpper;
				slices[iu].valid = true;
			}
		}

		return slices;
	}
}

std::unique_ptr<AeroVaporMeshRenderer> AeroVaporMeshRenderer::instance_ = nullptr;

AeroVaporMeshRenderer* AeroVaporMeshRenderer::GetInstance() {
	if (!instance_) {
		instance_ = std::make_unique<AeroVaporMeshRenderer>();
	}
	return instance_.get();
}

void AeroVaporMeshRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreatePipeline();
	CreateConstantBuffers();
	CreateWingVaporMesh();
	CreateTransonicConeMesh();

	// 深度SRV未登録時の型一致フォールバック用 Null Texture2D<float> SRV
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

	EnsureNoiseTextureLoaded();
	currentDrawCount_ = 0;
	isInitialized_ = true;
}

void AeroVaporMeshRenderer::Finalize() {
	wingMesh_.vertexBuffer.Reset();
	wingMesh_.indexBuffer.Reset();
	wingMesh_.indexCount = 0;

	coneMesh_.vertexBuffer.Reset();
	coneMesh_.indexBuffer.Reset();
	coneMesh_.indexCount = 0;

	if (transformBuffer_) {
		transformBuffer_->Unmap(0, nullptr);
		transformBuffer_.Reset();
		transformMapped_ = nullptr;
	}
	if (paramsBuffer_) {
		paramsBuffer_->Unmap(0, nullptr);
		paramsBuffer_.Reset();
		paramsMapped_ = nullptr;
	}

	pipelineState_.Reset();
	rootSignature_.Reset();

	noiseTextureIndex_ = 0;
	noiseTextureLoaded_ = false;
	nullDepthSrvIndex_ = 0;
	currentDrawCount_ = 0;
	currentWingModel_ = nullptr;
	planformSummary_ = {};
	isInitialized_ = false;
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
}

void AeroVaporMeshRenderer::Reset() {
	currentDrawCount_ = 0;
}

void AeroVaporMeshRenderer::EnsureNoiseTextureLoaded() {
	if (noiseTextureLoaded_) {
		return;
	}
	TextureManager* tm = TextureManager::GetInstance();
	if (!tm) {
		return;
	}
	tm->LoadTexture(kNoiseMaskPath);
	noiseTextureIndex_ = tm->GetTextureIndexByFilePath(kNoiseMaskPath);
	noiseTextureLoaded_ = (noiseTextureIndex_ != 0);
}

void AeroVaporMeshRenderer::CreatePipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParams[4] = {};

	// [0] VS b0: VaporTransformCB
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParams[0].Descriptor.ShaderRegister = 0;

	// [1] ALL b1: VaporParamsCB
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[1].Descriptor.ShaderRegister = 1;

	// [2] PS t0: gNoiseTex (シームレスノイズマスク)
	D3D12_DESCRIPTOR_RANGE rangeNoise[1] = {};
	rangeNoise[0].BaseShaderRegister = 0;
	rangeNoise[0].NumDescriptors = 1;
	rangeNoise[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeNoise[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams[2].DescriptorTable.pDescriptorRanges = rangeNoise;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;

	// [3] PS t1: gDepthTex (深度ソフトフェード用)
	D3D12_DESCRIPTOR_RANGE rangeDepth[1] = {};
	rangeDepth[0].BaseShaderRegister = 1;
	rangeDepth[0].NumDescriptors = 1;
	rangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams[3].DescriptorTable.pDescriptorRanges = rangeDepth;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;

	// シームレスノイズの気流スクロール用 WRAP サンプラー
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc.pParameters = rootParams;
	rootSigDesc.NumParameters = _countof(rootParams);
	rootSigDesc.pStaticSamplers = staticSamplers;
	rootSigDesc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
		}
		assert(false);
	}
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"assets/shaders/AeroVapor.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"assets/shaders/AeroVapor.PS.hlsl", L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState = GetBlendDesc(BlendMode::kNormal);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void AeroVaporMeshRenderer::CreateConstantBuffers() {
	const size_t totalSlots = static_cast<size_t>(kMaxDrawCount) * kFrameCount;
	const size_t bufferBytes = kCbAlignment * totalSlots;

	transformBuffer_ = dxCommon_->CreateBufferResource(bufferBytes);
	paramsBuffer_ = dxCommon_->CreateBufferResource(bufferBytes);

	transformBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformMapped_));
	paramsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&paramsMapped_));
	std::memset(transformMapped_, 0, bufferBytes);
	std::memset(paramsMapped_, 0, bufferBytes);
}

void AeroVaporMeshRenderer::CreateWingVaporMesh() {
	auto evalWingLeadingEdgeZ = [](float xAbs) -> float {
		if (xAbs <= 2.12f) {
			const float t = std::clamp((xAbs - 0.95f) / (2.12f - 0.95f), 0.0f, 1.0f);
			return std::lerp(3.25f, 0.72f, t);
		}
		if (xAbs <= 3.11f) {
			const float t = std::clamp((xAbs - 2.12f) / (3.11f - 2.12f), 0.0f, 1.0f);
			return std::lerp(0.72f, -1.58f, t);
		}
		const float t = std::clamp((xAbs - 3.11f) / (4.50f - 3.11f), 0.0f, 1.0f);
		return std::lerp(-1.58f, -4.78f, t);
	};

	std::vector<WingSlice> defaultSlices(kWingSpanSegs + 1);
	for (int iu = 0; iu <= kWingSpanSegs; ++iu) {
		const float u = static_cast<float>(iu) / static_cast<float>(kWingSpanSegs);
		const float xAbs = std::lerp(0.92f, 4.32f, u);
		defaultSlices[iu].xAbs = xAbs;
		defaultSlices[iu].zLead = evalWingLeadingEdgeZ(xAbs);
		defaultSlices[iu].zTrail = -4.78f;
		defaultSlices[iu].yUpper = std::lerp(0.14f, 0.09f, u);
		defaultSlices[iu].valid = true;
	}

	const std::vector<WingSlice>* sideSlices[2] = { &defaultSlices, &defaultSlices };
	constexpr int kLayers = 3;
	constexpr float kLayerFactors[kLayers] = { 0.0f, 0.5f, 1.0f };
	constexpr float kLayerBaseY[kLayers] = { 0.05f, 0.12f, 0.20f };
	constexpr float kLayerPeakY[kLayers] = { 0.30f, 0.68f, 1.18f };

	float maxChord = 0.01f;
	for (int sideIdx = 0; sideIdx < 2; ++sideIdx) {
		for (const auto& sl : *sideSlices[sideIdx]) {
			maxChord = (std::max)(maxChord, sl.zLead - sl.zTrail);
		}
	}

	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	vertices.reserve(2 * kLayers * (kWingSpanSegs + 1) * (kWingChordSegs + 1));
	indices.reserve(2 * kLayers * kWingSpanSegs * kWingChordSegs * 6);

	auto evalShellPos = [&](int sideIdx, float sideSign, int layerIdx, int iu, float u, float v, float& outDomeH, float& outChordNorm) -> Vector3 {
		const WingSlice& sl = (*sideSlices[sideIdx])[iu];
		const float x = sideSign * sl.xAbs;
		const float rawChord = (std::max)(0.0f, sl.zLead - sl.zTrail);
		outChordNorm = std::clamp(rawChord / maxChord, 0.0f, 1.0f);

		const float zLead = (std::max)(sl.zTrail, sl.zLead - 0.08f);
		const float zTrail = sl.zTrail - 0.12f * rawChord;
		const float z = std::lerp(zLead, zTrail, v);

		const float chordScale = std::pow(outChordNorm, 0.75f);
		const float shoulderLift = 0.20f * std::pow(1.0f - u, 3.0f) * std::sin(kPi * v) * chordScale;
		const float archV = std::sin(kPi * std::pow(std::clamp(v, 0.0f, 1.0f), 0.72f));
		const float archU = 0.30f + 0.70f * std::sin(kPi * std::clamp(1.0f - u * 0.80f, 0.04f, 1.0f));

		const float y = sl.yUpper + shoulderLift +
			kLayerBaseY[layerIdx] * (0.35f + 0.65f * archV) * (0.25f + 0.75f * chordScale);
		outDomeH = kLayerPeakY[layerIdx] * archV * archU * chordScale;
		return { x, y, z };
	};

	for (int sideIdx = 0; sideIdx < 2; ++sideIdx) {
		const float s = (sideIdx == 0) ? -1.0f : 1.0f;
		for (int layerIdx = 0; layerIdx < kLayers; ++layerIdx) {
			const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
			const int strideU = kWingSpanSegs + 1;

			for (int iv = 0; iv <= kWingChordSegs; ++iv) {
				const float v = static_cast<float>(iv) / static_cast<float>(kWingChordSegs);
				for (int iu = 0; iu <= kWingSpanSegs; ++iu) {
					const float u = static_cast<float>(iu) / static_cast<float>(kWingSpanSegs);

					float domeH = 0.0f;
					float chordNorm = 1.0f;
					const Vector3 p = evalShellPos(sideIdx, s, layerIdx, iu, u, v, domeH, chordNorm);

					const int iuPrev = (std::max)(0, iu - 1);
					const int iuNext = (std::min)(kWingSpanSegs, iu + 1);
					const float uPrev = static_cast<float>(iuPrev) / static_cast<float>(kWingSpanSegs);
					const float uNext = static_cast<float>(iuNext) / static_cast<float>(kWingSpanSegs);
					const float vPrev = std::clamp(v - 0.02f, 0.0f, 1.0f);
					const float vNext = std::clamp(v + 0.02f, 0.0f, 1.0f);

					float dhU0 = 0.0f, dhU1 = 0.0f, dhV0 = 0.0f, dhV1 = 0.0f;
					float cnDummy = 0.0f;
					Vector3 pU0 = evalShellPos(sideIdx, s, layerIdx, iuPrev, uPrev, v, dhU0, cnDummy);
					Vector3 pU1 = evalShellPos(sideIdx, s, layerIdx, iuNext, uNext, v, dhU1, cnDummy);
					Vector3 pV0 = evalShellPos(sideIdx, s, layerIdx, iu, u, vPrev, dhV0, cnDummy);
					Vector3 pV1 = evalShellPos(sideIdx, s, layerIdx, iu, u, vNext, dhV1, cnDummy);
					pU0.y += dhU0 * 0.55f;
					pU1.y += dhU1 * 0.55f;
					pV0.y += dhV0 * 0.55f;
					pV1.y += dhV1 * 0.55f;

					Vector3 tangentU = Subtract(pU1, pU0);
					Vector3 tangentV = Subtract(pV1, pV0);
					Vector3 normal = Cross(tangentU, tangentV);
					if (Length(normal) > 1e-5f) {
						normal = Normalize(normal);
						if (normal.y < 0.0f) {
							normal = Multiply(-1.0f, normal);
						}
					} else {
						normal = { 0.0f, 1.0f, 0.0f };
					}

					VertexData vert{};
					vert.position = { p.x, p.y, p.z, 1.0f };
					vert.texcoord = { u, v };
					vert.normal = normal;
					vert.extra = { kLayerFactors[layerIdx], domeH, s, chordNorm };
					vertices.push_back(vert);
				}
			}

			for (int iv = 0; iv < kWingChordSegs; ++iv) {
				for (int iu = 0; iu < kWingSpanSegs; ++iu) {
					const uint32_t i00 = baseVertex + static_cast<uint32_t>(iv * strideU + iu);
					const uint32_t i10 = i00 + 1u;
					const uint32_t i01 = i00 + static_cast<uint32_t>(strideU);
					const uint32_t i11 = i01 + 1u;

					indices.push_back(i00);
					indices.push_back(i01);
					indices.push_back(i10);

					indices.push_back(i10);
					indices.push_back(i01);
					indices.push_back(i11);
				}
			}
		}
	}

	const size_t vbBytes = sizeof(VertexData) * vertices.size();
	const size_t ibBytes = sizeof(uint32_t) * indices.size();

	wingMesh_.vertexBuffer = dxCommon_->CreateBufferResource(vbBytes);
	wingMesh_.indexBuffer = dxCommon_->CreateBufferResource(ibBytes);

	void* mappedVb = nullptr;
	wingMesh_.vertexBuffer->Map(0, nullptr, &mappedVb);
	std::memcpy(mappedVb, vertices.data(), vbBytes);
	wingMesh_.vertexBuffer->Unmap(0, nullptr);

	void* mappedIb = nullptr;
	wingMesh_.indexBuffer->Map(0, nullptr, &mappedIb);
	std::memcpy(mappedIb, indices.data(), ibBytes);
	wingMesh_.indexBuffer->Unmap(0, nullptr);

	wingMesh_.vbView.BufferLocation = wingMesh_.vertexBuffer->GetGPUVirtualAddress();
	wingMesh_.vbView.SizeInBytes = static_cast<UINT>(vbBytes);
	wingMesh_.vbView.StrideInBytes = sizeof(VertexData);

	wingMesh_.ibView.BufferLocation = wingMesh_.indexBuffer->GetGPUVirtualAddress();
	wingMesh_.ibView.SizeInBytes = static_cast<UINT>(ibBytes);
	wingMesh_.ibView.Format = DXGI_FORMAT_R32_UINT;
	wingMesh_.indexCount = static_cast<uint32_t>(indices.size());

	planformSummary_.valid = true;
	planformSummary_.halfWingspan = 4.50f;
	planformSummary_.rootX = 0.95f;
	planformSummary_.tipZ = -4.78f;
	planformSummary_.tipY = 0.09f;
	constexpr float kFracs[5] = { 0.24f, 0.36f, 0.48f, 0.60f, 0.72f };
	constexpr float kZOffs[5] = { -4.40f, -4.50f, -4.58f, -4.65f, -4.72f };
	for (int i = 0; i < 5; ++i) {
		planformSummary_.rightTrailingStations[i] = { 4.50f * kFracs[i], 0.24f, kZOffs[i] };
	}
}

bool AeroVaporMeshRenderer::RebuildWingMeshFromModel(const Model* model, const Vector3& baseRotation) {
	if (!isInitialized_ || !model) {
		return false;
	}
	if (currentWingModel_ == model &&
	    std::fabs(currentBaseRotation_.x - baseRotation.x) < 1e-4f &&
	    std::fabs(currentBaseRotation_.y - baseRotation.y) < 1e-4f &&
	    std::fabs(currentBaseRotation_.z - baseRotation.z) < 1e-4f) {
		return true;
	}

	const Model::ModelData& modelData = model->GetModelData();
	const Matrix4x4 baseRotMat = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, baseRotation, { 0.0f, 0.0f, 0.0f });

	std::vector<WingTri> leftTris;
	std::vector<WingTri> rightTris;
	CollectWingTrianglesRecursive(modelData.rootNode, 0, modelData, baseRotMat, leftTris, rightTris);

	if (leftTris.empty() && rightTris.empty()) {
		return false;
	}
	if (leftTris.empty()) {
		leftTris = rightTris;
		for (auto& tri : leftTris) {
			tri.v0.x = -tri.v0.x;
			tri.v1.x = -tri.v1.x;
			tri.v2.x = -tri.v2.x;
		}
	} else if (rightTris.empty()) {
		rightTris = leftTris;
		for (auto& tri : rightTris) {
			tri.v0.x = -tri.v0.x;
			tri.v1.x = -tri.v1.x;
			tri.v2.x = -tri.v2.x;
		}
	}

	std::vector<WingSlice> leftSlices = ExtractWingSlices(leftTris);
	std::vector<WingSlice> rightSlices = ExtractWingSlices(rightTris);
	if (leftSlices.size() != static_cast<size_t>(kWingSpanSegs + 1) ||
	    rightSlices.size() != static_cast<size_t>(kWingSpanSegs + 1)) {
		return false;
	}

	const std::vector<WingSlice>* sideSlices[2] = { &leftSlices, &rightSlices };
	constexpr int kLayers = 3;
	constexpr float kLayerFactors[kLayers] = { 0.0f, 0.5f, 1.0f };
	constexpr float kLayerBaseY[kLayers] = { 0.05f, 0.12f, 0.20f };
	constexpr float kLayerPeakY[kLayers] = { 0.30f, 0.68f, 1.18f };

	float maxChord = 0.01f;
	for (int sideIdx = 0; sideIdx < 2; ++sideIdx) {
		for (const auto& sl : *sideSlices[sideIdx]) {
			if (!sl.valid) {
				return false;
			}
			maxChord = (std::max)(maxChord, sl.zLead - sl.zTrail);
		}
	}

	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	vertices.reserve(2 * kLayers * (kWingSpanSegs + 1) * (kWingChordSegs + 1));
	indices.reserve(2 * kLayers * kWingSpanSegs * kWingChordSegs * 6);

	auto evalShellPos = [&](int sideIdx, float sideSign, int layerIdx, int iu, float u, float v, float& outDomeH, float& outChordNorm) -> Vector3 {
		const WingSlice& sl = (*sideSlices[sideIdx])[iu];
		const float x = sideSign * sl.xAbs;
		const float rawChord = (std::max)(0.0f, sl.zLead - sl.zTrail);
		outChordNorm = std::clamp(rawChord / maxChord, 0.0f, 1.0f);

		const float zLead = (std::max)(sl.zTrail, sl.zLead - 0.08f);
		const float zTrail = sl.zTrail - 0.12f * rawChord;
		const float z = std::lerp(zLead, zTrail, v);

		const float chordScale = std::pow(outChordNorm, 0.75f);
		const float shoulderLift = 0.20f * std::pow(1.0f - u, 3.0f) * std::sin(kPi * v) * chordScale;
		const float archV = std::sin(kPi * std::pow(std::clamp(v, 0.0f, 1.0f), 0.72f));
		const float archU = 0.30f + 0.70f * std::sin(kPi * std::clamp(1.0f - u * 0.80f, 0.04f, 1.0f));

		const float y = sl.yUpper + shoulderLift +
			kLayerBaseY[layerIdx] * (0.35f + 0.65f * archV) * (0.25f + 0.75f * chordScale);
		outDomeH = kLayerPeakY[layerIdx] * archV * archU * chordScale;
		return { x, y, z };
	};

	for (int sideIdx = 0; sideIdx < 2; ++sideIdx) {
		const float s = (sideIdx == 0) ? -1.0f : 1.0f;
		for (int layerIdx = 0; layerIdx < kLayers; ++layerIdx) {
			const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
			const int strideU = kWingSpanSegs + 1;

			for (int iv = 0; iv <= kWingChordSegs; ++iv) {
				const float v = static_cast<float>(iv) / static_cast<float>(kWingChordSegs);
				for (int iu = 0; iu <= kWingSpanSegs; ++iu) {
					const float u = static_cast<float>(iu) / static_cast<float>(kWingSpanSegs);

					float domeH = 0.0f;
					float chordNorm = 1.0f;
					const Vector3 p = evalShellPos(sideIdx, s, layerIdx, iu, u, v, domeH, chordNorm);

					const int iuPrev = (std::max)(0, iu - 1);
					const int iuNext = (std::min)(kWingSpanSegs, iu + 1);
					const float uPrev = static_cast<float>(iuPrev) / static_cast<float>(kWingSpanSegs);
					const float uNext = static_cast<float>(iuNext) / static_cast<float>(kWingSpanSegs);
					const float vPrev = std::clamp(v - 0.02f, 0.0f, 1.0f);
					const float vNext = std::clamp(v + 0.02f, 0.0f, 1.0f);

					float dhU0 = 0.0f, dhU1 = 0.0f, dhV0 = 0.0f, dhV1 = 0.0f;
					float cnDummy = 0.0f;
					Vector3 pU0 = evalShellPos(sideIdx, s, layerIdx, iuPrev, uPrev, v, dhU0, cnDummy);
					Vector3 pU1 = evalShellPos(sideIdx, s, layerIdx, iuNext, uNext, v, dhU1, cnDummy);
					Vector3 pV0 = evalShellPos(sideIdx, s, layerIdx, iu, u, vPrev, dhV0, cnDummy);
					Vector3 pV1 = evalShellPos(sideIdx, s, layerIdx, iu, u, vNext, dhV1, cnDummy);
					pU0.y += dhU0 * 0.55f;
					pU1.y += dhU1 * 0.55f;
					pV0.y += dhV0 * 0.55f;
					pV1.y += dhV1 * 0.55f;

					Vector3 tangentU = Subtract(pU1, pU0);
					Vector3 tangentV = Subtract(pV1, pV0);
					Vector3 normal = Cross(tangentU, tangentV);
					if (Length(normal) > 1e-5f) {
						normal = Normalize(normal);
						if (normal.y < 0.0f) {
							normal = Multiply(-1.0f, normal);
						}
					} else {
						normal = { 0.0f, 1.0f, 0.0f };
					}

					VertexData vert{};
					vert.position = { p.x, p.y, p.z, 1.0f };
					vert.texcoord = { u, v };
					vert.normal = normal;
					vert.extra = { kLayerFactors[layerIdx], domeH, s, chordNorm };
					vertices.push_back(vert);
				}
			}

			for (int iv = 0; iv < kWingChordSegs; ++iv) {
				for (int iu = 0; iu < kWingSpanSegs; ++iu) {
					const uint32_t i00 = baseVertex + static_cast<uint32_t>(iv * strideU + iu);
					const uint32_t i10 = i00 + 1u;
					const uint32_t i01 = i00 + static_cast<uint32_t>(strideU);
					const uint32_t i11 = i01 + 1u;

					indices.push_back(i00);
					indices.push_back(i01);
					indices.push_back(i10);

					indices.push_back(i10);
					indices.push_back(i01);
					indices.push_back(i11);
				}
			}
		}
	}

	if (wingMesh_.vertexBuffer) {
		dxCommon_->WaitForGPU();
		wingMesh_.vertexBuffer.Reset();
		wingMesh_.indexBuffer.Reset();
	}

	const size_t vbBytes = sizeof(VertexData) * vertices.size();
	const size_t ibBytes = sizeof(uint32_t) * indices.size();

	wingMesh_.vertexBuffer = dxCommon_->CreateBufferResource(vbBytes);
	wingMesh_.indexBuffer = dxCommon_->CreateBufferResource(ibBytes);

	void* mappedVb = nullptr;
	wingMesh_.vertexBuffer->Map(0, nullptr, &mappedVb);
	std::memcpy(mappedVb, vertices.data(), vbBytes);
	wingMesh_.vertexBuffer->Unmap(0, nullptr);

	void* mappedIb = nullptr;
	wingMesh_.indexBuffer->Map(0, nullptr, &mappedIb);
	std::memcpy(mappedIb, indices.data(), ibBytes);
	wingMesh_.indexBuffer->Unmap(0, nullptr);

	wingMesh_.vbView.BufferLocation = wingMesh_.vertexBuffer->GetGPUVirtualAddress();
	wingMesh_.vbView.SizeInBytes = static_cast<UINT>(vbBytes);
	wingMesh_.vbView.StrideInBytes = sizeof(VertexData);

	wingMesh_.ibView.BufferLocation = wingMesh_.indexBuffer->GetGPUVirtualAddress();
	wingMesh_.ibView.SizeInBytes = static_cast<UINT>(ibBytes);
	wingMesh_.ibView.Format = DXGI_FORMAT_R32_UINT;
	wingMesh_.indexCount = static_cast<uint32_t>(indices.size());

	// 抽出済みメッシュの実スパン・翼端・後縁ステーションをGPUパーティクル向けに記録
	float maxAbsX = 0.0f;
	float tipZSum = 0.0f;
	float tipYSum = 0.0f;
	int tipCount = 0;
	for (const auto& tri : rightTris) {
		const Vector3 pts[3] = { tri.v0, tri.v1, tri.v2 };
		for (const auto& pt : pts) {
			const float ax = std::fabs(pt.x);
			if (ax > maxAbsX) {
				maxAbsX = ax;
			}
		}
	}
	for (const auto& tri : rightTris) {
		const Vector3 pts[3] = { tri.v0, tri.v1, tri.v2 };
		for (const auto& pt : pts) {
			if (std::fabs(pt.x) >= maxAbsX - 0.10f) {
				tipZSum += pt.z;
				tipYSum += pt.y;
				++tipCount;
			}
		}
	}

	planformSummary_.valid = true;
	planformSummary_.halfWingspan = (maxAbsX > 0.5f) ? maxAbsX : rightSlices.back().xAbs;
	planformSummary_.rootX = rightSlices.front().xAbs;
	planformSummary_.tipZ = (tipCount > 0) ? (tipZSum / static_cast<float>(tipCount)) : rightSlices.back().zTrail;
	planformSummary_.tipY = (tipCount > 0) ? (tipYSum / static_cast<float>(tipCount)) : rightSlices.back().yUpper;

	constexpr float kFracs[5] = { 0.24f, 0.36f, 0.48f, 0.60f, 0.72f };
	for (int i = 0; i < 5; ++i) {
		const float targetX = planformSummary_.halfWingspan * kFracs[i];
		size_t bestIdx = 0;
		float bestDist = 1e9f;
		for (size_t sIdx = 0; sIdx < rightSlices.size(); ++sIdx) {
			const float d = std::fabs(rightSlices[sIdx].xAbs - targetX);
			if (d < bestDist) {
				bestDist = d;
				bestIdx = sIdx;
			}
		}
		const WingSlice& sl = rightSlices[bestIdx];
		const float chord = (std::max)(0.0f, sl.zLead - sl.zTrail);
		planformSummary_.rightTrailingStations[i] = {
			sl.xAbs,
			sl.yUpper + 0.14f,
			sl.zTrail + (std::min)(0.35f, chord * 0.08f)
		};
	}

	currentWingModel_ = model;
	currentBaseRotation_ = baseRotation;
	return true;
}

void AeroVaporMeshRenderer::CreateTransonicConeMesh() {
	constexpr int kRadialSegs = 36;
	constexpr int kAxialSegs = 16;
	constexpr int kLayers = 2;
	constexpr float kLayerFactors[kLayers] = { 0.0f, 1.0f };
	constexpr float kBackRadii[kLayers] = { 4.4f, 5.6f };

	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	vertices.reserve(kLayers * (kRadialSegs + 1) * (kAxialSegs + 1));
	indices.reserve(kLayers * kRadialSegs * kAxialSegs * 6);

	constexpr float zFront = 0.9f;
	constexpr float zBack = -4.6f;
	constexpr float rFront = 0.95f;

	for (int layerIdx = 0; layerIdx < kLayers; ++layerIdx) {
		const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
		const int strideU = kRadialSegs + 1;
		const float rBack = kBackRadii[layerIdx];
		const float layerFactor = kLayerFactors[layerIdx];

		for (int iv = 0; iv <= kAxialSegs; ++iv) {
			const float v = static_cast<float>(iv) / static_cast<float>(kAxialSegs);
			const float z = std::lerp(zFront, zBack, v);
			const float r = std::lerp(rFront, rBack, std::pow(v, 0.82f));
			const float domeH = (0.15f + 0.55f * layerFactor) * std::sin(kPi * v);

			for (int iu = 0; iu <= kRadialSegs; ++iu) {
				const float u = static_cast<float>(iu) / static_cast<float>(kRadialSegs);
				const float theta = u * 2.0f * kPi;
				const float cosT = std::cos(theta);
				const float sinT = std::sin(theta);

				VertexData vert{};
				vert.position = { r * cosT, r * sinT, z, 1.0f };
				vert.texcoord = { u, v };
				vert.normal = Normalize({ cosT, sinT, 0.48f });
				vert.extra = { layerFactor, domeH, 1.0f, 1.0f };
				vertices.push_back(vert);
			}
		}

		for (int iv = 0; iv < kAxialSegs; ++iv) {
			for (int iu = 0; iu < kRadialSegs; ++iu) {
				const uint32_t i00 = baseVertex + static_cast<uint32_t>(iv * strideU + iu);
				const uint32_t i10 = i00 + 1u;
				const uint32_t i01 = i00 + static_cast<uint32_t>(strideU);
				const uint32_t i11 = i01 + 1u;

				indices.push_back(i00);
				indices.push_back(i01);
				indices.push_back(i10);

				indices.push_back(i10);
				indices.push_back(i01);
				indices.push_back(i11);
			}
		}
	}

	const size_t vbBytes = sizeof(VertexData) * vertices.size();
	const size_t ibBytes = sizeof(uint32_t) * indices.size();

	coneMesh_.vertexBuffer = dxCommon_->CreateBufferResource(vbBytes);
	coneMesh_.indexBuffer = dxCommon_->CreateBufferResource(ibBytes);

	void* mappedVb = nullptr;
	coneMesh_.vertexBuffer->Map(0, nullptr, &mappedVb);
	std::memcpy(mappedVb, vertices.data(), vbBytes);
	coneMesh_.vertexBuffer->Unmap(0, nullptr);

	void* mappedIb = nullptr;
	coneMesh_.indexBuffer->Map(0, nullptr, &mappedIb);
	std::memcpy(mappedIb, indices.data(), ibBytes);
	coneMesh_.indexBuffer->Unmap(0, nullptr);

	coneMesh_.vbView.BufferLocation = coneMesh_.vertexBuffer->GetGPUVirtualAddress();
	coneMesh_.vbView.SizeInBytes = static_cast<UINT>(vbBytes);
	coneMesh_.vbView.StrideInBytes = sizeof(VertexData);

	coneMesh_.ibView.BufferLocation = coneMesh_.indexBuffer->GetGPUVirtualAddress();
	coneMesh_.ibView.SizeInBytes = static_cast<UINT>(ibBytes);
	coneMesh_.ibView.Format = DXGI_FORMAT_R32_UINT;
	coneMesh_.indexCount = static_cast<uint32_t>(indices.size());
}

void AeroVaporMeshRenderer::DrawWingVapor(
	const Matrix4x4& aircraftWorldMatrix,
	float intensity,
	float timeSec,
	Camera* camera,
	const AeroFlowParams& flowParams) {

	const float effectiveIntensity = tuning_.forcePreview ? tuning_.previewWingIntensity : intensity;
	if (effectiveIntensity <= 0.01f) {
		return;
	}
	DrawMesh(
		wingMesh_,
		aircraftWorldMatrix,
		std::clamp(effectiveIntensity, 0.0f, 1.0f),
		timeSec,
		0u,
		1.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		flowParams,
		camera);
}

void AeroVaporMeshRenderer::DrawTransonicCone(
	const Matrix4x4& aircraftWorldMatrix,
	float intensity,
	float timeSec,
	Camera* camera) {

	const float effectiveIntensity = tuning_.forcePreview ? tuning_.previewConeIntensity : intensity;
	if (effectiveIntensity <= 0.01f) {
		return;
	}
	AeroFlowParams defaultFlow{};
	DrawMesh(
		coneMesh_,
		aircraftWorldMatrix,
		std::clamp(effectiveIntensity, 0.0f, 1.0f),
		timeSec,
		1u,
		0.88f,
		{ 0.96f, 0.98f, 1.0f, 1.0f },
		defaultFlow,
		camera);
}

void AeroVaporMeshRenderer::DrawMesh(
	const MeshBuffers& mesh,
	const Matrix4x4& worldMatrix,
	float intensity,
	float timeSec,
	uint32_t mode,
	float opacityBoost,
	const Vector4& baseColor,
	const AeroFlowParams& flowParams,
	Camera* camera) {

	if (!isInitialized_ || !camera || mesh.indexCount == 0u || currentDrawCount_ >= kMaxDrawCount) {
		return;
	}

	EnsureNoiseTextureLoaded();
	if (!noiseTextureLoaded_) {
		return;
	}

	const uint32_t slot = currentDrawCount_++;
	const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex() % kFrameCount;
	const size_t slotOffset = (static_cast<size_t>(frameIndex) * kMaxDrawCount + slot) * kCbAlignment;

	VaporTransformCB* xf = reinterpret_cast<VaporTransformCB*>(transformMapped_ + slotOffset);
	xf->World = worldMatrix;
	xf->WVP = worldMatrix * camera->GetViewProjectionMatrix();
	xf->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

	// Object3dCommon の太陽光設定と同期（未設定時はデフォルト斜光を使用）
	Vector3 sunDir = { 0.45f, 0.80f, 0.35f };
	Vector3 sunColor = { 1.0f, 0.98f, 0.95f };
	float sunIntensity = 1.0f;
	if (Object3dCommon* objCommon = Object3dCommon::GetInstance()) {
		if (const DirectionalLight* dl = objCommon->GetDirectionalLightData()) {
			const Vector3 lightDir = { -dl->direction.x, -dl->direction.y, -dl->direction.z };
			if (Length(lightDir) > 1e-4f) {
				sunDir = Normalize(lightDir);
			}
			sunColor = { dl->color.x, dl->color.y, dl->color.z };
			sunIntensity = dl->intensity;
		}
	}

	const uint32_t depthSrvIndex = dxCommon_->GetDepthSrvIndex();
	const bool useDepthSoft = (depthSrvIndex != 0u && dxCommon_->GetDepthStencilBuffer() != nullptr);

	VaporParamsCB* prm = reinterpret_cast<VaporParamsCB*>(paramsMapped_ + slotOffset);
	prm->time = timeSec;
	prm->intensity = intensity;
	prm->flowSpeed = tuning_.flowSpeed;
	prm->softDistance = tuning_.softDistance;
	prm->leadingEdgeSharpness = tuning_.leadingEdgeSharpness;
	prm->trailingErosion = tuning_.trailingErosion;
	prm->densityScale = tuning_.densityScale;
	prm->fresnelPower = tuning_.fresnelPower;
	prm->sunDirection = sunDir;
	prm->sunIntensity = sunIntensity;
	prm->sunColor = sunColor;
	prm->domeThickness = tuning_.domeThickness;
	prm->shadowTint = tuning_.shadowTint;
	prm->mode = mode;
	prm->cameraPosition = camera->GetTranslate();
	prm->nearClip = camera->GetNearClip();
	prm->farClip = camera->GetFarClip();
	prm->hasDepthTexture = useDepthSoft ? 1u : 0u;
	prm->wingSpanExpand = tuning_.wingSpanExpand;
	prm->opacityBoost = opacityBoost;
	prm->baseColor = baseColor;
	prm->aoaRad = flowParams.aoaRad;
	prm->sideslipRad = flowParams.sideslipRad;
	prm->rollAsymmetry = flowParams.rollAsymmetry;
	prm->airspeedScale = flowParams.airspeedScale;
	prm->vortexCoreStrength = tuning_.vortexCoreStrength;
	prm->streamlineTwist = tuning_.streamlineTwist;
	prm->pad0 = 0.0f;
	prm->pad1 = 0.0f;

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRenderTextureRTVHandle(0);
	if (useDepthSoft) {
		D3D12_RESOURCE_BARRIER depthBarrier{};
		depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthBarrier.Transition.pResource = dxCommon_->GetDepthStencilBuffer();
		depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &depthBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsvHandle = dxCommon_->GetReadOnlyDSVHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &readOnlyDsvHandle);
	}

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &mesh.vbView);
	commandList->IASetIndexBuffer(&mesh.ibView);

	commandList->SetGraphicsRootConstantBufferView(0, transformBuffer_->GetGPUVirtualAddress() + slotOffset);
	commandList->SetGraphicsRootConstantBufferView(1, paramsBuffer_->GetGPUVirtualAddress() + slotOffset);
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(noiseTextureIndex_));
	srvManager_->SetGraphicsRootDescriptorTable(3, useDepthSoft ? depthSrvIndex : nullDepthSrvIndex_);

	commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

	if (useDepthSoft) {
		D3D12_RESOURCE_BARRIER depthBarrier{};
		depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthBarrier.Transition.pResource = dxCommon_->GetDepthStencilBuffer();
		depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		commandList->ResourceBarrier(1, &depthBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE normalDsvHandle = dxCommon_->GetDSVHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &normalDsvHandle);
	}
}

void AeroVaporMeshRenderer::DrawDebugGui() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode("3D Aero Vapor Shell Tuning")) {
		ImGui::Checkbox("Force Preview Intensity", &tuning_.forcePreview);
		if (tuning_.forcePreview) {
			ImGui::SliderFloat("Preview Wing Intensity", &tuning_.previewWingIntensity, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Preview Cone Intensity", &tuning_.previewConeIntensity, 0.0f, 1.0f, "%.2f");
		}
		ImGui::SliderFloat("Leading Edge Sharpness", &tuning_.leadingEdgeSharpness, 0.02f, 0.25f, "%.3f");
		ImGui::SliderFloat("Trailing Erosion", &tuning_.trailingErosion, 0.10f, 1.20f, "%.2f");
		ImGui::SliderFloat("Airflow Scroll Speed", &tuning_.flowSpeed, 0.4f, 8.0f, "%.2f");
		ImGui::SliderFloat("Density Scale", &tuning_.densityScale, 0.15f, 1.40f, "%.2f");
		ImGui::SliderFloat("Dome Thickness", &tuning_.domeThickness, 0.25f, 2.00f, "%.2f");
		ImGui::SliderFloat("View Angle Fade", &tuning_.fresnelPower, 0.15f, 2.00f, "%.2f");
		ImGui::SliderFloat("Soft Depth Fade (m)", &tuning_.softDistance, 0.05f, 1.20f, "%.2f m");
		ImGui::SliderFloat("Span Expand Factor", &tuning_.wingSpanExpand, 0.50f, 1.40f, "%.2f");
		ImGui::SliderFloat("LEV Vortex Core Strength", &tuning_.vortexCoreStrength, 0.0f, 1.80f, "%.2f");
		ImGui::SliderFloat("Streamline Twist", &tuning_.streamlineTwist, 0.0f, 2.00f, "%.2f");
		ImGui::ColorEdit3("Cloud Shadow Tint", &tuning_.shadowTint.x);
		ImGui::TreePop();
	}
#endif
}
