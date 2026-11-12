#include "PrimitiveModel.h"
#include <cassert>
#include <cmath>
#include "../System/TextureManager.h"
#include "PrimitiveGenerator.h"
#include "../base/logger.h"

using namespace logger;
using namespace MyMath;

std::unique_ptr<PrimitiveModel> PrimitiveModel::instance_ = nullptr;

PrimitiveModel* PrimitiveModel::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<PrimitiveModel>();
	}
	return instance_.get();
}

void PrimitiveModel::Finalize() {
	instance_.reset();
}

void PrimitiveModel::Initialize(DirectXCommon* dxCommon) {
	dxCommon_ = dxCommon;
	
	// ルートシグネチャとパイプラインの生成
	CreateRootSignature();
	CreateGraphicsPipeline();

	// リングやシリンダーの頂点バッファを生成
	CreatePrimitiveBuffers();

	// 最大描画回数分の定数バッファアロケーション (Transformation と Material)
	uint32_t transformBufferSize = kCbAlignment * kMaxDrawCount;
	uint32_t materialBufferSize = kCbAlignment * kMaxDrawCount;

	transformBuffer_ = dxCommon_->CreateBufferResource(transformBufferSize);
	materialBuffer_ = dxCommon_->CreateBufferResource(materialBufferSize);

	// マップしっぱなしにする
	transformBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformMappedData_));
	materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&materialMappedData_));
	
	TextureManager::GetInstance()->LoadTexture("assets/textures/uvChecker.png");

	Reset();
}

void PrimitiveModel::Reset() {
	currentDrawCount_ = 0;
}

void PrimitiveModel::DrawRing(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(ringVertexBufferView_, ringVertexCount_, scale, rotate, translate, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawRing(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(ringVertexBufferView_, ringVertexCount_, worldMatrix, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawCylinder(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(cylinderVertexBufferView_, cylinderVertexCount_, scale, rotate, translate, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawCylinder(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(cylinderVertexBufferView_, cylinderVertexCount_, worldMatrix, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawPlane(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(planeVertexBufferView_, planeVertexCount_, scale, rotate, translate, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawPlane(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(planeVertexBufferView_, planeVertexCount_, worldMatrix, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawCone(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(coneVertexBufferView_, coneVertexCount_, scale, rotate, translate, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawCone(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(coneVertexBufferView_, coneVertexCount_, worldMatrix, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawCone(const Vector3& scale, const Quaternion& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	CallDrawCommand(coneVertexBufferView_, coneVertexCount_, scale, rotate, translate, color, textureIndex, camera, blendMode);
}

void PrimitiveModel::DrawLine3D(const Vector3& p1, const Vector3& p2, const Vector4& color, Camera* camera) {
	if (currentDrawCount_ >= kMaxDrawCount) { return; }
	if (!camera) { return; }

	Matrix4x4 wMatrix = {
		p2.x - p1.x, p2.y - p1.y, p2.z - p1.z, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		p1.x, p1.y, p1.z, 1.0f
	};
	Matrix4x4 wvpMatrix = wMatrix * camera->GetViewProjectionMatrix();

	uint32_t index = currentDrawCount_;
	currentDrawCount_++;

	TransformationMatrix* transformData = reinterpret_cast<TransformationMatrix*>(&transformMappedData_[index * kCbAlignment]);
	transformData->WVP = wvpMatrix;
	transformData->World = wMatrix;
	transformData->WorldInverseTranspose = wMatrix; 

	MaterialData* materialData = reinterpret_cast<MaterialData*>(&materialMappedData_[index * kCbAlignment]);
	materialData->color = color;
	materialData->enableLighting = 0; 
	materialData->shininess = 50.0f;
	materialData->uvTransform = Identity4x4();

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(pipelineStatesLine_[static_cast<size_t>(BlendMode::kNormal)].Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &lineVertexBufferView_);

	commandList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));
	commandList->SetGraphicsRootConstantBufferView(1, transformBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));

	// ダミーテクスチャをセット
	uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/uvChecker.png");
	if (texIndex != 0) {
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));
	}

	commandList->DrawInstanced(lineVertexCount_, 1, 0, 0);
}

void PrimitiveModel::CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	if (currentDrawCount_ >= kMaxDrawCount) { return; }
	if (vertexCount == 0 || !camera) { return; }

	// 行列計算
	Matrix4x4 wMatrix = MakeAffineMatrix(scale, rotate, translate);
	Matrix4x4 wvpMatrix = wMatrix * camera->GetViewProjectionMatrix();

	// 定数バッファの書き込み先インデックス
	uint32_t index = currentDrawCount_;
	currentDrawCount_++;

	TransformationMatrix* transformData = reinterpret_cast<TransformationMatrix*>(&transformMappedData_[index * kCbAlignment]);
	transformData->WVP = wvpMatrix;
	transformData->World = wMatrix;
	transformData->WorldInverseTranspose = wMatrix; // PSで法線計算が不要なためWorldをそのまま入れる（簡易実装）

	MaterialData* materialData = reinterpret_cast<MaterialData*>(&materialMappedData_[index * kCbAlignment]);
	materialData->color = color;
	materialData->enableLighting = 0; // 今回は常に0（Unlit）
	materialData->shininess = 50.0f;
	materialData->uvTransform = Identity4x4();

	// --- 描画コマンドの発行 ---
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// パイプライン状態の設定
	commandList->SetPipelineState(pipelineStates_[static_cast<size_t>(blendMode)].Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// 頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vbView);

	commandList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));
	
	commandList->SetGraphicsRootConstantBufferView(1, transformBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));

	uint32_t texIndex = textureIndex;
	if (texIndex == 0) { texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/uvChecker.png"); }
	if (texIndex != 0) {
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));
	}

	// 描画
	commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void PrimitiveModel::CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Vector3& scale, const Quaternion& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	if (currentDrawCount_ >= kMaxDrawCount) { return; }
	if (vertexCount == 0 || !camera) { return; }

	// 行列計算 (クォータニオンを使用する MakeAffineMatrix)
	Matrix4x4 wMatrix = MakeAffineMatrix(scale, rotate, translate);
	Matrix4x4 wvpMatrix = wMatrix * camera->GetViewProjectionMatrix();

	// 定数バッファの書き込み先インデックス
	uint32_t index = currentDrawCount_;
	currentDrawCount_++;

	TransformationMatrix* transformData = reinterpret_cast<TransformationMatrix*>(&transformMappedData_[index * kCbAlignment]);
	transformData->WVP = wvpMatrix;
	transformData->World = wMatrix;
	transformData->WorldInverseTranspose = wMatrix; 

	MaterialData* materialData = reinterpret_cast<MaterialData*>(&materialMappedData_[index * kCbAlignment]);
	materialData->color = color;
	materialData->enableLighting = 0; // 常に0
	materialData->shininess = 50.0f;
	materialData->uvTransform = Identity4x4();

	// --- 描画コマンドの発行 ---
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// パイプライン状態の設定
	commandList->SetPipelineState(pipelineStates_[static_cast<size_t>(blendMode)].Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// 頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vbView);

	commandList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));
	
	commandList->SetGraphicsRootConstantBufferView(1, transformBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));

	uint32_t texIndex = textureIndex;
	if (texIndex == 0) { texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/uvChecker.png"); }
	if (texIndex != 0) {
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));
	}

	// 描画
	commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void PrimitiveModel::CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Matrix4x4& wMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode) {
	if (currentDrawCount_ >= kMaxDrawCount) { return; }
	if (vertexCount == 0 || !camera) { return; }

	Matrix4x4 wvpMatrix = wMatrix * camera->GetViewProjectionMatrix();

	uint32_t index = currentDrawCount_;
	currentDrawCount_++;

	TransformationMatrix* transformData = reinterpret_cast<TransformationMatrix*>(&transformMappedData_[index * kCbAlignment]);
	transformData->WVP = wvpMatrix;
	transformData->World = wMatrix;
	transformData->WorldInverseTranspose = wMatrix; 

	MaterialData* materialData = reinterpret_cast<MaterialData*>(&materialMappedData_[index * kCbAlignment]);
	materialData->color = color;
	materialData->enableLighting = 0; // 常に0
	materialData->shininess = 50.0f;
	materialData->uvTransform = Identity4x4();

	// --- 描画コマンドの発行 ---
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// パイプライン状態の設定
	commandList->SetPipelineState(pipelineStates_[static_cast<size_t>(blendMode)].Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// 頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vbView);

	commandList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));
	commandList->SetGraphicsRootConstantBufferView(1, transformBuffer_->GetGPUVirtualAddress() + (index * kCbAlignment));

	uint32_t texIndex = textureIndex;
	if (texIndex == 0) { texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/uvChecker.png"); }
	if (texIndex != 0) {
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));
	}

	// 描画
	commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void PrimitiveModel::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void PrimitiveModel::CreateGraphicsPipeline() {
	// 軽量版インプットレイアウト（WEIGHTやINDICESがない）
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"./assets/shaders/Primitive.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"./assets/shaders/Primitive.PS.hlsl", L"ps_6_0");
	
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // エフェクトなので裏面も描画する
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // エフェクトなのでZ書き込みは通常しない
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 全ブレンドモード生成
	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCountOf); ++i) {
		BlendMode mode = static_cast<BlendMode>(i);
		graphicsPipelineStateDesc.BlendState = GetBlendDesc(mode);
		
		if (mode == BlendMode::kOpaque) {
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		} else {
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineStates_[i]));
		assert(SUCCEEDED(hr));
	}

	// ライン描画用パイプラインの生成 (トポロジータイプをLINEに変更)
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCountOf); ++i) {
		BlendMode mode = static_cast<BlendMode>(i);
		graphicsPipelineStateDesc.BlendState = GetBlendDesc(mode);

		if (mode == BlendMode::kOpaque) {
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		} else {
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineStatesLine_[i]));
		assert(SUCCEEDED(hr));
	}
}

void PrimitiveModel::CreatePrimitiveBuffers() {
	// ===================================
	// リングの頂点データ生成
	// ===================================
	std::vector<VertexData> ringVertices = PrimitiveGenerator::CreateRing(32, 0.8f, 1.0f);

	ringVertexCount_ = static_cast<uint32_t>(ringVertices.size());
	ringVertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * ringVertexCount_);
	ringVertexBufferView_.BufferLocation = ringVertexBuffer_->GetGPUVirtualAddress();
	ringVertexBufferView_.SizeInBytes = sizeof(VertexData) * ringVertexCount_;
	ringVertexBufferView_.StrideInBytes = sizeof(VertexData);
	
	VertexData* ringMapped = nullptr;
	ringVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&ringMapped));
	std::memcpy(ringMapped, ringVertices.data(), ringVertexBufferView_.SizeInBytes);
	ringVertexBuffer_->Unmap(0, nullptr);

	// ===================================
	// シリンダーの頂点データ生成
	// ===================================
	std::vector<VertexData> cylinderVertices = PrimitiveGenerator::CreateCylinder(32, 1.0f, 1.0f);

	cylinderVertexCount_ = static_cast<uint32_t>(cylinderVertices.size());
	cylinderVertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * cylinderVertexCount_);
	cylinderVertexBufferView_.BufferLocation = cylinderVertexBuffer_->GetGPUVirtualAddress();
	cylinderVertexBufferView_.SizeInBytes = sizeof(VertexData) * cylinderVertexCount_;
	cylinderVertexBufferView_.StrideInBytes = sizeof(VertexData);
	
	VertexData* cylMapped = nullptr;
	cylinderVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cylMapped));
	std::memcpy(cylMapped, cylinderVertices.data(), cylinderVertexBufferView_.SizeInBytes);
	cylinderVertexBuffer_->Unmap(0, nullptr);

	// ===================================
	// プレーンの頂点データ生成
	// ===================================
	std::vector<VertexData> planeVertices = PrimitiveGenerator::CreatePlane(2.0f);

	planeVertexCount_ = static_cast<uint32_t>(planeVertices.size());
	planeVertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * planeVertexCount_);
	planeVertexBufferView_.BufferLocation = planeVertexBuffer_->GetGPUVirtualAddress();
	planeVertexBufferView_.SizeInBytes = sizeof(VertexData) * planeVertexCount_;
	planeVertexBufferView_.StrideInBytes = sizeof(VertexData);
	
	VertexData* planeMapped = nullptr;
	planeVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&planeMapped));
	std::memcpy(planeMapped, planeVertices.data(), planeVertexBufferView_.SizeInBytes);
	planeVertexBuffer_->Unmap(0, nullptr);

	// ===================================
	// コーンの頂点データ生成
	// ===================================
	std::vector<VertexData> coneVertices = PrimitiveGenerator::CreateCone(32, 1.0f, 1.0f);

	coneVertexCount_ = static_cast<uint32_t>(coneVertices.size());
	coneVertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * coneVertexCount_);
	coneVertexBufferView_.BufferLocation = coneVertexBuffer_->GetGPUVirtualAddress();
	coneVertexBufferView_.SizeInBytes = sizeof(VertexData) * coneVertexCount_;
	coneVertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* coneMapped = nullptr;
	coneVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&coneMapped));
	std::memcpy(coneMapped, coneVertices.data(), coneVertexBufferView_.SizeInBytes);
	coneVertexBuffer_->Unmap(0, nullptr);

	// ===================================
	// ラインの頂点データ生成 (0,0,0) -> (1,0,0)
	// ===================================
	lineVertexCount_ = 2;
	lineVertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * lineVertexCount_);
	lineVertexBufferView_.BufferLocation = lineVertexBuffer_->GetGPUVirtualAddress();
	lineVertexBufferView_.SizeInBytes = sizeof(VertexData) * lineVertexCount_;
	lineVertexBufferView_.StrideInBytes = sizeof(VertexData);
	
	VertexData* lineMapped = nullptr;
	lineVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lineMapped));
	lineMapped[0] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
	lineMapped[1] = { {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
	lineVertexBuffer_->Unmap(0, nullptr);
}
