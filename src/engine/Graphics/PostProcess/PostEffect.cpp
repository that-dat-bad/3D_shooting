#include "PostEffect.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "../System/TextureManager.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include "../../io/json.hpp"

using json = nlohmann::json;

void to_json(json& j, const ActivePostEffect& effect) {
	j = json{
		{"type", static_cast<int>(effect.type)},
		{"kernelSize", effect.kernelSize},
		{"intensity", effect.intensity},
		{"dirX", effect.dirX},
		{"dirY", effect.dirY},
		{"dissolveThreshold", effect.dissolveThreshold},
		{"dissolveEdgeWidth", effect.dissolveEdgeWidth},
		{"dissolveMaskIndex", effect.dissolveMaskIndex},
		{"colorR", effect.colorR},
		{"colorG", effect.colorG},
		{"colorB", effect.colorB}
	};
}

void from_json(const json& j, ActivePostEffect& effect) {
	effect.type = static_cast<PostEffectType>(j.value("type", 0));
	effect.kernelSize = j.value("kernelSize", 3);
	effect.intensity = j.value("intensity", 1.0f);
	effect.dirX = j.value("dirX", 0.0f);
	effect.dirY = j.value("dirY", 0.0f);
	effect.dissolveThreshold = j.value("dissolveThreshold", 0.5f);
	effect.dissolveEdgeWidth = j.value("dissolveEdgeWidth", 0.05f);
	effect.dissolveMaskIndex = j.value("dissolveMaskIndex", 0);
	effect.colorR = j.value("colorR", 1.0f);
	effect.colorG = j.value("colorG", 1.0f);
	effect.colorB = j.value("colorB", 1.0f);
}

std::unique_ptr<PostEffect> PostEffect::instance_ = nullptr;

PostEffect* PostEffect::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<PostEffect>();
	}
	return instance_.get();
}

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateRootSignature();
	CreateDissolveRootSignature();
	LoadDissolveMasks();
	CreateGraphicsPipelines();

	size_t cbAlignedSize = (sizeof(PostEffectParams) + 0xff) & ~0xff;
	postEffectParamsBuffer_ = dxCommon_->CreateBufferResource(cbAlignedSize * 64);
	HRESULT hr = postEffectParamsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPostEffectParams_));
	assert(SUCCEEDED(hr));
}

void PostEffect::Finalize() {
	instance_.reset();
}

void PostEffect::CreateRootSignature() {
	// ルートシグネチャ: t0 にテクスチャ、s0 にサンプラー
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);
	desc.pStaticSamplers = staticSamplers;
	desc.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void PostEffect::CreateDissolveRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRange0[1] = {};
	descriptorRange0[0].BaseShaderRegister = 0;
	descriptorRange0[0].NumDescriptors = 1;
	descriptorRange0[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange0[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRange1[1] = {};
	descriptorRange1[0].BaseShaderRegister = 1;
	descriptorRange1[0].NumDescriptors = 1;
	descriptorRange1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange0;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange1;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);
	desc.pStaticSamplers = staticSamplers;
	desc.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&dissolveRootSignature_));
	assert(SUCCEEDED(hr));
}

void PostEffect::LoadDissolveMasks() {
	// ノイズマスクテクスチャを読み込み
	const std::string maskPaths[] = {
		"assets/masks/noise0.png",
		"assets/masks/noise1.png",
	};
	for (const auto& path : maskPaths) {
		TextureManager::GetInstance()->LoadTexture(path);
		uint32_t srvIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);
		dissolveMaskSrvIndices_.push_back(srvIndex);
	}
}

void PostEffect::CreateGraphicsPipelines() {
	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"./assets/shaders/FullScreen.VS.hlsl", L"vs_6_0");

	// ピクセルシェーダーのコンパイル（各種エフェクト）
	Microsoft::WRL::ComPtr<IDxcBlob> psNone = dxCommon_->CompileShader(L"./assets/shaders/CopyImage.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psColorTint = dxCommon_->CompileShader(L"./assets/shaders/ColorTint.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psVignette = dxCommon_->CompileShader(L"./assets/shaders/Vignette.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBoxFilter = dxCommon_->CompileShader(L"./assets/shaders/BoxFilter.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psGaussBlur = dxCommon_->CompileShader(L"./assets/shaders/GaussBlur.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psKawaseBlur = dxCommon_->CompileShader(L"./assets/shaders/KawaseBlur.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psRadialBlur = dxCommon_->CompileShader(L"./assets/shaders/RadialBlur.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psDissolve = dxCommon_->CompileShader(L"./assets/shaders/Dissolve.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psRandom = dxCommon_->CompileShader(L"./assets/shaders/Random.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psScanLine = dxCommon_->CompileShader(L"./assets/shaders/ScanLine.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psLightAmp = dxCommon_->CompileShader(L"./assets/shaders/LightAmp.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psLensDistortion = dxCommon_->CompileShader(L"./assets/shaders/LensDistortion.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psChromaticAberration = dxCommon_->CompileShader(L"./assets/shaders/ChromaticAberration.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBloom = dxCommon_->CompileShader(L"./assets/shaders/Bloom.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psColorIsolation = dxCommon_->CompileShader(L"./assets/shaders/ColorIsolation.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psLuminanceOutline = dxCommon_->CompileShader(L"./assets/shaders/LuminanceBasedOutline.PS.hlsl", L"ps_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psDepthOutline = dxCommon_->CompileShader(L"./assets/shaders/DepthBasedOutline.PS.hlsl", L"ps_6_0");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaders[static_cast<size_t>(PostEffectType::kCountOfPostEffects)];
	pixelShaders[static_cast<size_t>(PostEffectType::kNone)] = psNone;
	pixelShaders[static_cast<size_t>(PostEffectType::kColorTint)] = psColorTint;
	pixelShaders[static_cast<size_t>(PostEffectType::kVignette)] = psVignette;
	pixelShaders[static_cast<size_t>(PostEffectType::kBoxFilter)] = psBoxFilter;
	pixelShaders[static_cast<size_t>(PostEffectType::kGaussBlur)] = psGaussBlur;
	pixelShaders[static_cast<size_t>(PostEffectType::kKawaseBlur)] = psKawaseBlur;
	pixelShaders[static_cast<size_t>(PostEffectType::kRadialBlur)] = psRadialBlur;
	pixelShaders[static_cast<size_t>(PostEffectType::kDissolve)] = psDissolve;
	pixelShaders[static_cast<size_t>(PostEffectType::kRandom)] = psRandom;
	pixelShaders[static_cast<size_t>(PostEffectType::kScanLine)] = psScanLine;
	pixelShaders[static_cast<size_t>(PostEffectType::kLightAmp)] = psLightAmp;
	pixelShaders[static_cast<size_t>(PostEffectType::kLensDistortion)] = psLensDistortion;
	pixelShaders[static_cast<size_t>(PostEffectType::kChromaticAberration)] = psChromaticAberration;
	pixelShaders[static_cast<size_t>(PostEffectType::kBloom)] = psBloom;
	pixelShaders[static_cast<size_t>(PostEffectType::kColorIsolation)] = psColorIsolation;
	pixelShaders[static_cast<size_t>(PostEffectType::kLuminanceOutline)] = psLuminanceOutline;
	pixelShaders[static_cast<size_t>(PostEffectType::kDepthOutline)] = psDepthOutline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();

	// フルスクリーン三角形: 頂点入力なし（SV_VertexID で生成）
	psoDesc.InputLayout = { nullptr, 0 };

	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };

	// ラスタライザ
	D3D12_RASTERIZER_DESC rasterizer{};
	rasterizer.CullMode = D3D12_CULL_MODE_NONE;
	rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState = rasterizer;

	// ブレンド（不透明）
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.BlendState = blendDesc;

	// 深度なし
	D3D12_DEPTH_STENCIL_DESC depthStencil{};
	depthStencil.DepthEnable = false;
	psoDesc.DepthStencilState = depthStencil;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 各エフェクト用の PSO を生成 (Dissolveは別root signatureなのでスキップ)
	for (size_t i = 0; i < static_cast<size_t>(PostEffectType::kCountOfPostEffects); ++i) {
		if (i == static_cast<size_t>(PostEffectType::kDissolve)) { continue; }
		psoDesc.PS = { pixelShaders[i]->GetBufferPointer(), pixelShaders[i]->GetBufferSize() };
		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates_[i]));
		assert(SUCCEEDED(hr));
	}

	psoDesc.pRootSignature = dissolveRootSignature_.Get();
	psoDesc.PS = { psDissolve->GetBufferPointer(), psDissolve->GetBufferSize() };
	HRESULT hrDissolve = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dissolvePSO_));
	assert(SUCCEEDED(hrDissolve));
}

void PostEffect::Draw(ID3D12Resource* renderTextureResource, uint32_t renderTextureSrvIndex) {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	time_ += 0.016f;

	D3D12_VIEWPORT viewport = dxCommon_->GetViewport();
	commandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissor = dxCommon_->GetScissorRect();
	commandList->RSSetScissorRects(1, &scissor);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[] = { srvManager_->descriptorHeap_.Get() };
	commandList->SetDescriptorHeaps(1, heaps);

	if (activeEffects_.empty()) {
		// 後方互換モード
		ActivePostEffect legacyEffect;
		legacyEffect.type = currentEffect_;
		legacyEffect.kernelSize = kernelSize_;
		legacyEffect.intensity = intensity_;
		legacyEffect.dirX = dirX_;
		legacyEffect.dirY = dirY_;
		legacyEffect.colorR = colorR_;
		legacyEffect.colorG = colorG_;
		legacyEffect.colorB = colorB_;
		
		if (legacyEffect.type == PostEffectType::kGaussBlur) {
			DrawSinglePass(commandList, legacyEffect, 0, 1, false, 0, 1.0f, 0.0f);
			DrawSinglePass(commandList, legacyEffect, 1, 0, true, 1, 0.0f, 1.0f);
		} else if (legacyEffect.type == PostEffectType::kKawaseBlur) {
			int passes = legacyEffect.kernelSize;
			if (passes < 1) passes = 1;
			if (passes > 10) passes = 10;
			uint32_t tempSrc = 0;
			uint32_t tempDst = 1;
			for (int j = 0; j < passes; ++j) {
				bool isFinalOfKawase = (j == passes - 1);
				float kawaseOffset = (float)j + 0.5f;
				DrawSinglePass(commandList, legacyEffect, tempSrc, tempDst, isFinalOfKawase, j, 0.0f, 0.0f, kawaseOffset);
				if (!isFinalOfKawase) {
					std::swap(tempSrc, tempDst);
				}
			}
		} else {
			DrawSinglePass(commandList, legacyEffect, 0, 0, true, 0);
		}
		return;
	}

	uint32_t currentSrcIndex = 0;
	uint32_t passIndex = 0;

	for (size_t i = 0; i < activeEffects_.size(); ++i) {
		const ActivePostEffect& effect = activeEffects_[i];
		bool isLastEffect = (i == activeEffects_.size() - 1);
		uint32_t dstIndex = 1 - currentSrcIndex;

		if (effect.type == PostEffectType::kGaussBlur) {
			DrawSinglePass(commandList, effect, currentSrcIndex, dstIndex, false, passIndex++, 1.0f, 0.0f);
			DrawSinglePass(commandList, effect, dstIndex, currentSrcIndex, isLastEffect, passIndex++, 0.0f, 1.0f);
			if (!isLastEffect) currentSrcIndex = currentSrcIndex;
		} else if (effect.type == PostEffectType::kKawaseBlur) {
			int passes = effect.kernelSize;
			if (passes < 1) passes = 1;
			if (passes > 10) passes = 10;
			uint32_t tempSrc = currentSrcIndex;
			uint32_t tempDst = dstIndex;
			for (int j = 0; j < passes; ++j) {
				bool isFinalOfKawase = (j == passes - 1);
				float kawaseOffset = (float)j + 0.5f;
				DrawSinglePass(commandList, effect, tempSrc, tempDst, isLastEffect && isFinalOfKawase, passIndex++, 0.0f, 0.0f, kawaseOffset);
				if (!isLastEffect || !isFinalOfKawase) {
					std::swap(tempSrc, tempDst);
				}
			}
			if (!isLastEffect) currentSrcIndex = tempSrc;
		} else {
			DrawSinglePass(commandList, effect, currentSrcIndex, dstIndex, isLastEffect, passIndex++);
			if (!isLastEffect) currentSrcIndex = dstIndex;
		}
	}
}

void PostEffect::DrawSinglePass(ID3D12GraphicsCommandList* commandList, const ActivePostEffect& effect, uint32_t srcIndex, uint32_t dstIndex, bool isOutputToBackBuffer, uint32_t passIndex, float customDirX, float customDirY, float customIntensity) {
	size_t cbAlignedSize = (sizeof(PostEffectParams) + 0xff) & ~0xff;
	size_t effectIndex = static_cast<size_t>(effect.type);

	if (effect.type == PostEffectType::kDissolve) {
		commandList->SetGraphicsRootSignature(dissolveRootSignature_.Get());
		commandList->SetPipelineState(dissolvePSO_.Get());
	} else {
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(pipelineStates_[effectIndex].Get());
	}

	// Set Render Target
	if (!isOutputToBackBuffer) {
		D3D12_RESOURCE_BARRIER barrierRT = {};
		barrierRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierRT.Transition.pResource = dxCommon_->GetRenderTexture(dstIndex);
		barrierRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrierRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commandList->ResourceBarrier(1, &barrierRT);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = dxCommon_->GetRenderTextureRTVHandle(dstIndex);
		commandList->OMSetRenderTargets(1, &rtv, false, nullptr);
	} else {
		D3D12_CPU_DESCRIPTOR_HANDLE destRtv = dxCommon_->GetCurrentRTVHandle();
		commandList->OMSetRenderTargets(1, &destRtv, false, nullptr);
	}

	// Set SRV Input
	uint32_t srvIndex = dxCommon_->GetRenderTextureSrvIndex(srcIndex);
	commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(srvIndex));

	// Set Parameters
	if (passIndex >= 64) passIndex = 63; // Limit to buffer boundary
	size_t offset = cbAlignedSize * passIndex;
	PostEffectParams* param = reinterpret_cast<PostEffectParams*>(reinterpret_cast<uint8_t*>(mappedPostEffectParams_) + offset);
	
	param->time = time_;
	param->colorR = effect.colorR;
	param->colorG = effect.colorG;
	param->colorB = effect.colorB;

	if (effect.type == PostEffectType::kDissolve) {
		param->kernelSize = 0;
		param->intensity = effect.dissolveThreshold;
		param->dirX = effect.dissolveEdgeWidth;
		param->dirY = 0.0f;
		commandList->SetGraphicsRootConstantBufferView(1, postEffectParamsBuffer_->GetGPUVirtualAddress() + offset);

		int maskIdx = effect.dissolveMaskIndex;
		if (maskIdx < 0 || maskIdx >= static_cast<int>(dissolveMaskSrvIndices_.size())) maskIdx = 0;
		if (dissolveMaskSrvIndices_.size() > 0) {
			commandList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(dissolveMaskSrvIndices_[maskIdx]));
		}
	} else {
		param->kernelSize = effect.kernelSize;
		param->intensity = (customIntensity >= 0.0f) ? customIntensity : effect.intensity;
		param->dirX = (customDirX != -999999.0f) ? customDirX : effect.dirX;
		param->dirY = (customDirY != -999999.0f) ? customDirY : effect.dirY;
		commandList->SetGraphicsRootConstantBufferView(1, postEffectParamsBuffer_->GetGPUVirtualAddress() + offset);
	}

	commandList->DrawInstanced(3, 1, 0, 0);

	// Revert Render Target back to SRV if not last pass
	if (!isOutputToBackBuffer) {
		D3D12_RESOURCE_BARRIER barrierSRV = {};
		barrierSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierSRV.Transition.pResource = dxCommon_->GetRenderTexture(dstIndex);
		barrierSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrierSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &barrierSRV);
	}
}

void PostEffect::ApplyBuiltInPreset(const std::string& presetName) {
	activeEffects_.clear();
	if (presetName == "NVD (Night Vision)") {
		ActivePostEffect amp;
		amp.type = PostEffectType::kLightAmp;
		amp.intensity = 4.0f;
		amp.colorR = 0.15f;
		amp.colorG = 1.0f;
		amp.colorB = 0.3f;
		activeEffects_.push_back(amp);

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 4.5f;
		bloom.dirX = 2.0f;
		bloom.dirY = 0.5f; // 輝度閾値を低めにして空などの広範囲を鮮烈に発光させる
		activeEffects_.push_back(bloom);

		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.10f;
		activeEffects_.push_back(noise);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.15f;
		scan.dirX = 600.0f;
		scan.dirY = 5.0f;
		activeEffects_.push_back(scan);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.015f;
		activeEffects_.push_back(ab);

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.18f;
		activeEffects_.push_back(dist);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 3.5f;
		activeEffects_.push_back(vig);
	}
	else if (presetName == "VHS Retro") {
		ActivePostEffect tint;
		tint.type = PostEffectType::kColorTint;
		tint.colorR = 1.0f;
		tint.colorG = 0.92f;
		tint.colorB = 0.82f;
		activeEffects_.push_back(tint);

		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.1f;
		activeEffects_.push_back(noise);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.12f;
		scan.dirX = 400.0f;
		scan.dirY = 2.0f;
		activeEffects_.push_back(scan);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.02f;
		activeEffects_.push_back(ab);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.2f;
		activeEffects_.push_back(vig);
	}
	else if (presetName == "Cinematic Bloom") {
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 4.0f;
		bloom.dirX = 2.0f;
		bloom.dirY = 0.7f;
		activeEffects_.push_back(bloom);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.0f;
		activeEffects_.push_back(vig);
	}
	else if (presetName == "Digital Glitch") {
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.25f;
		activeEffects_.push_back(noise);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.04f;
		activeEffects_.push_back(ab);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.25f;
		scan.dirX = 800.0f;
		scan.dirY = -10.0f;
		activeEffects_.push_back(scan);

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.1f;
		activeEffects_.push_back(dist);
	}
	else if (presetName == "Color Isolation (Red Focus)") {
		ActivePostEffect iso;
		iso.type = PostEffectType::kColorIsolation;
		iso.colorR = 0.9f;
		iso.colorG = 0.1f;
		iso.colorB = 0.1f;
		iso.intensity = 0.45f; // Tolerance
		iso.dirX = 0.15f;      // Smoothness
		iso.dirY = 1.0f;       // Desaturation (Full monochrome for non-red)
		activeEffects_.push_back(iso);
	}
}

bool PostEffect::SavePreset(const std::string& presetName) {
	try {
		std::filesystem::create_directories("assets/presets");
		std::string filepath = "assets/presets/" + presetName + ".json";
		std::ofstream file(filepath);
		if (!file.is_open()) return false;

		json j = activeEffects_;
		file << j.dump(4);
		return true;
	}
	catch (...) {
		return false;
	}
}

bool PostEffect::LoadPreset(const std::string& presetName) {
	try {
		std::string filepath = "assets/presets/" + presetName + ".json";
		std::ifstream file(filepath);
		if (!file.is_open()) return false;

		json j;
		file >> j;
		activeEffects_ = j.get<std::vector<ActivePostEffect>>();
		return true;
	}
	catch (...) {
		return false;
	}
}
