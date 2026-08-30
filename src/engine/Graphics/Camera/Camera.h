#pragma once
#include "../base/Math/MyMath.h"
using namespace MyMath;

/// <summary>
/// 仮想カメラクラス
/// 空間のビュー行列・プロジェクション行列の計算と保持を行う
/// </summary>
class Camera
{
public:
	/// <summary>コンストラクタ</summary>
	Camera();

	/// <summary>
	/// カメラ情報の更新
	/// 行列の再計算を行う
	/// </summary>
	void Update();

	//アクセッサ
	//--セッター--//
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(const float fovY) { fovY_ = fovY; }
	void SetAspectRatio(const float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(const float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(const float farClip) { farClip_ = farClip; }

	//--ゲッター--//
	const Matrix4x4& GetWorldMatrix()const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const{ return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const{ return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const{ return viewProjectionMatrix_; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	float GetFovY() const { return fovY_; }
	float GetAspectRatio() const { return aspectRatio_; }
	float GetNearClip() const { return nearClip_; }
	float GetFarClip() const { return farClip_; }

	/// <summary>
	/// カメラを揺らす（カメラシェイク）
	/// </summary>
	/// <param name="duration">揺れる時間（秒）</param>
	/// <param name="magnitude">揺れの大きさ</param>
	void Shake(float duration, float magnitude);

private:
	Transform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	//水平方向視野角
	float fovY_;
	//アスペクト比
	float aspectRatio_;
	//ニアクリップ距離
	float nearClip_;
	//ファークリップ
	float farClip_;

	// カメラシェイク用
	float shakeTimer_ = 0.0f;
	float shakeMagnitude_ = 0.0f;
};

