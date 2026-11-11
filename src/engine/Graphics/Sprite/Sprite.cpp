#include "Sprite.h"
#include "SpriteCommon.h"
#include "../../base/WinApp.h"
#include "../System/DirectXCommon.h"
#include "../base/Math/MyMath.h"
#include "../System/TextureManager.h"
#include "../../../external/DirectXTex/DirectXTex.h"

// 引数の dxCommon を削除 (spriteCommonから取得できるため)
void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath)
{
	spriteCommon_ = spriteCommon;
	DirectXCommon* dxCommon = spriteCommon_->GetDirectXCommon();
	assert(dxCommon != nullptr && "dxCommon is NULL! SpriteCommon::Initialize was not called.");
	assert(dxCommon->GetDevice() != nullptr && "D3D12Device is NULL!");
	vertexBuffer_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
	indexBuffer_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

	vertexBufferView.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	indexBufferView.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	uint32_t* indexDataLocal = nullptr;
	indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexDataLocal));
	indexDataLocal[0] = 0; indexDataLocal[1] = 1; indexDataLocal[2] = 2;
	indexDataLocal[3] = 1; indexDataLocal[4] = 3; indexDataLocal[5] = 2;
	indexBuffer_->Unmap(0, nullptr);

	//マテリアルリソースの作成
	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->WVP = Identity4x4();

	// テクスチャのロード処理
	if (textureFilePath.empty()) {
		// パスが空の場合は一旦仮のインデックスまたは0にしておく
		textureIndex_ = 0;
		size_ = { 100.0f, 100.0f }; // 仮サイズ
	}
	else {
		textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
		AdjustTextureSize();
	}

	anchorPoint_ = { 0.5f, 0.5f };
}

void Sprite::Update()
{
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// テクスチャ情報の取得
	const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(textureIndex_);

	// テクスチャサイズが0除算にならないようガード（念のため）
	float texWidth = static_cast<float>(metaData.width);
	float texHeight = static_cast<float>(metaData.height);
	if (texWidth == 0) { texWidth = 1.0f; }
	if (texHeight == 0) { texHeight = 1.0f; }

	float texLeft = textureLeftTop_.x / texWidth;
	float texRight = (textureLeftTop_.x + textureSize_.x) / texWidth;
	float texTop = textureLeftTop_.y / texHeight;
	float texBottom = (textureLeftTop_.y + textureSize_.y) / texHeight;

	//左右反転
	if (isFlipX_) { left = -left; right = -right; }
	//上下反転
	if (isFlipY_) { top = -top; bottom = -bottom; }

	vertexData_[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
	vertexData_[0].texcoord = { texLeft,texBottom };
	vertexData_[1].position = { left, top, 0.0f, 1.0f };   // 左上
	vertexData_[1].texcoord = { texLeft, texTop };
	vertexData_[2].position = { right, bottom, 0.0f, 1.0f };// 右下
	vertexData_[2].texcoord = { texRight, texBottom };
	vertexData_[3].position = { right, top, 0.0f, 1.0f };  // 右上
	vertexData_[3].texcoord = { texRight, texTop };

	Vector3 finalScale = {
		size_.x * transform_.scale.x,
		size_.y * transform_.scale.y,
		1.0f
	};

	// 正射影行列の静的キャッシュ（毎フレーム・毎スプライトの再計算を完全撤廃）
	static const Matrix4x4 s_proj = makeOrthographicmMatrix(
		0.0f, 0.0f,
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
		-10.0f, 100.0f
	);

	// 回転0のスプライト（文字やUIパネル等99%以上）は4x4行列積算を行わず直接代入する高速パス
	if (transform_.rotate.z == 0.0f && transform_.rotate.x == 0.0f && transform_.rotate.y == 0.0f) {
		Matrix4x4 wvp = s_proj;
		wvp.m[0][0] = finalScale.x * s_proj.m[0][0];
		wvp.m[1][1] = finalScale.y * s_proj.m[1][1];
		wvp.m[3][0] = transform_.translate.x * s_proj.m[0][0] + s_proj.m[3][0];
		wvp.m[3][1] = transform_.translate.y * s_proj.m[1][1] + s_proj.m[3][1];
		wvp.m[3][2] = transform_.translate.z * s_proj.m[2][2] + s_proj.m[3][2];
		materialData_->WVP = wvp;
	} else {
		Matrix4x4 worldMatrix = MakeAffineMatrix(finalScale, transform_.rotate, transform_.translate);
		materialData_->WVP = Multiply(worldMatrix, s_proj);
	}
}

void Sprite::Draw()
{
	DirectXCommon* dxCommon = spriteCommon_->GetDirectXCommon();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
	
	spriteCommon_->SetBlendMode(blendMode_);

	// 1. 頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	// 2. インデックスバッファの設定
	commandList->IASetIndexBuffer(&indexBufferView);
	// 3. マテリアル(定数バッファ)の設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// 4. テクスチャの設定 (内部の textureIndex_ を使用)
	commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	// 5. 描画コマンド
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::ChangeTexture(std::string textureFilePath)
{
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	AdjustTextureSize();
}

void Sprite::AdjustTextureSize()
{
	//テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);
	//画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}
