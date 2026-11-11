#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <random>

#include "../base/Math/MyMath.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "../System/BlendMode.h"
#include <array>

using namespace MyMath;

namespace ParticleHeightCapDefaults {
	inline constexpr float kNoHeightLimit = 0.0f;
	inline constexpr float kSoftBrakeStartRatio = 0.45f;
	inline constexpr float kSoftBrakeSpanRatio = 0.55f;
	inline constexpr float kUnitRatio = 1.0f;
	inline constexpr uint32_t kZeroCount = 0u;
}

// パーティクル1粒の情報（CPU側での計算用）
struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector3 acceleration; // 加速度
	Vector3 angularVelocity; // 回転速度
	Vector4 color;
	float lifeTime;
	float currentTime;

	// アニメーション用パラメータ
	float startScale = 1.0f;   // 初期スケール
	float endScale = 0.0f;     // 終了時スケール
	float startAlpha = 1.0f;   // 初期アルファ（フェードアウト用）
	float scaleEasing = 1.0f;  // スケールイージング指数

	// 上昇上限高さ制御（0.0f以下で無制限）
	float spawnY = ParticleHeightCapDefaults::kNoHeightLimit;
	float maxRiseHeight = ParticleHeightCapDefaults::kNoHeightLimit;

	// ストレッチビルボード
	bool isStretched = false;
	float stretchFactor = 1.0f;
	Vector3 stretchDir = { 0.0f, 0.0f, 0.0f };
};

// パーティクル発生パラメータ
struct ParticleParameters {
	Vector3 minVelocity = { -1.0f, -1.0f, -1.0f };
	Vector3 maxVelocity = { 1.0f, 1.0f, 1.0f };
	Vector4 minColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float minLifeTime = 1.0f;
	float maxLifeTime = 3.0f;
	Vector3 acceleration = { 0.0f, 0.0f, 0.0f }; // 重力など
	float randomPositionRange = 1.0f;            // 発生位置のランダムばらつき範囲（0.0で完全に点から発生）

	// 上昇上限高さ（発生Y座標からの最大上昇距離[m]：0.0f以下で無制限）
	float maxRiseHeight = ParticleHeightCapDefaults::kNoHeightLimit;

	// 回転
	float minRotation = 0.0f;
	float maxRotation = 0.0f;
	float minRotationSpeed = 0.0f;
	float maxRotationSpeed = 0.0f;

	// スケール・フェードアニメーション
	float minScale = 1.0f;     // 初期スケールの範囲（最小）
	float maxScale = 1.0f;     // 初期スケールの範囲（最大）
	float endScale = 0.0f;     // 寿命終了時のスケール
	bool fadeOut = true;       // アルファフェードアウトの有効化
	float scaleEasing = 1.0f;  // スケール変化のイージング指数（1.0=リニア）

	// ストレッチ（引き伸ばし）ビルボード用
	bool isStretched = false;
	float stretchFactor = 1.0f;
	Vector3 stretchDir = { 0.0f, 0.0f, 0.0f };
};

struct ParticleInstancingData {
	Matrix4x4 WVP;
	Vector4 color;
};

struct AABB {
	Vector3 min; // 最小点
	Vector3 max; // 最大点
};

// 加速度場
struct AccelerationField {
	Vector3 acceleration; // 加速度
	AABB area;            // 適用範囲
};

// パーティクルグループ
struct ParticleGroup {
	// マテリアルデータ
	std::string textureFilePath;
	uint32_t textureSrvIndex;

	// パーティクルのリスト
	std::list<Particle> particles;

	// インスタンシングデータ用
	uint32_t instancingSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
	UINT instanceCount; // 描画するインスタンス数
	ParticleInstancingData* instancingDataPtr = nullptr; // 書き込み用ポインタ
	BlendMode blendMode = BlendMode::kNormal;
};

#include <memory> 

/// <summary>
/// CPU駆動型パーティクルシステム管理クラス
/// </summary>
class ParticleManager
{
public: // シングルトンパターン
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static ParticleManager* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	ParticleManager() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update();
	void Draw();
	void Finalize();

	// グループの作成
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

	// パーティクルの発生
	void Emit(const std::string& name, const Vector3& position, uint32_t count);
	void Emit(const std::string& name, const Vector3& position, const ParticleParameters& params, uint32_t count);

	// ブレンドモード設定
	void SetBlendMode(const std::string& name, BlendMode mode);

	// 加速度場の追加
	void AddAccelerationField(const std::string& name, const Vector3& acceleration, const AABB& area);

	// 稼働中パーティクルの全消去・統計取得
	void ClearAllParticles();
	uint32_t GetTotalActiveParticleCount() const;
	uint32_t GetGroupActiveParticleCount(const std::string& name) const;

private:
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

	static std::unique_ptr<ParticleManager> instance_;

	// メンバ変数
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups_;

	// 加速度場コンテナ
	std::unordered_map<std::string, AccelerationField> accelerationFields_;

	// 共通リソース
	// 頂点データ（板ポリゴン）
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// カメラ・スクリーン定数バッファ
	struct CameraData {
		float nearClip;
		float farClip;
		float screenWidth;
		float screenHeight;
	};
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraBuffer_;
	CameraData* cameraDataPtr_ = nullptr;

	// パイプライン関連
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> graphicsPipelineStates_;

	// ランダムエンジン
	std::mt19937 randomEngine_;

	// 最大インスタンス数（1グループあたり）
	const uint32_t kMaxInstanceCount_ = 1024;

private: // 内部関数
	void CreateRootSignature();
	void CreateGraphicsPipeline();
	void CreateModel(); // 板ポリゴンの作成
};
