#pragma once
#include"../base/Math/MyMath.h"
using namespace MyMath;
#include <d3d12.h>
#include <cstdint>
#include <wrl/client.h>
#include<string>

#include "../System/BlendMode.h"
class SpriteCommon;
class DirectXCommon;

/// <summary>
/// 2Dスプライト(画像)の描画を行うクラス
/// </summary>
class Sprite
{
public:
	/// <summary>
	/// スプライトの初期化
	/// </summary>
	/// <param name="spriteCommon">SpriteCommonポインタ</param>
	/// <param name="textureFilePath">テクスチャのファイルパス</param>
	void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath = "");

	/// <summary>
	/// 毎フレームの更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// 描画コマンドの発行
	/// </summary>
	void Draw();

	/// <summary>
	/// テクスチャの変更
	/// </summary>
	/// <param name="textureFilePath">新しいテクスチャパス</param>
	void ChangeTexture(std::string textureFilePath);

	//--アクセッサ--//
	//---セッター---//
	void SetPosition(const Vector2& pos) { transform_.translate = { pos.x, pos.y, 0.0f }; }
	void SetRotation(float rot) { transform_.rotate.z = rot; }
	void SetScale(const Vector2& scale) { transform_.scale = { scale.x, scale.y, 1.0f }; }
	void SetSize(const Vector2& size) { size_ = size; }
	void SetColor(const Vector4& color) { materialData_->color = color; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
	void SetFlipX(bool isFlip) { isFlipX_ = isFlip; }
	void SetFlipY(bool isFlip) { isFlipY_ = isFlip; }
	void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }
	void SetTextureSize(const Vector2& size) { textureSize_ = size; }

	// テクスチャインデックス設定時にサイズも合わせるように変更
	void SetTextureIndex(uint32_t textureIndex) {
		textureIndex_ = textureIndex;
		AdjustTextureSize();
	}

	void SetBlendMode(BlendMode mode) { blendMode_ = mode; }

	//---ゲッター---//
	Vector2 GetPosition() const { return { transform_.translate.x, transform_.translate.y }; }
	float GetRotation() const { return transform_.rotate.z; }
	Vector2 GetSize() const { return size_; }
	Vector4 GetColor() const { return materialData_->color; }
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	bool GetFlipX() const { return isFlipX_; }
	bool GetFlipY() const { return isFlipY_; }
	Vector2 GetTextureLeftTop() const { return textureLeftTop_; }
	Vector2 GetTextureSize() const { return textureSize_; }

private:
	// テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();

	SpriteCommon* spriteCommon_ = nullptr;

	//---頂点データ---//
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
	};

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_ = nullptr;

	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};


	//---マテリアルデータ---//
	struct Material {
		Matrix4x4 WVP;
		Vector4 color;
	};

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
	Material* materialData_ = nullptr;


	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	uint32_t textureIndex_ = 0;

	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	bool isFlipX_ = false;
	bool isFlipY_ = false;
	//テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	//テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.0f,100.0f };
	Vector2 size_ = { 100.0f,100.0f };
	BlendMode blendMode_ = BlendMode::kNormal;
};