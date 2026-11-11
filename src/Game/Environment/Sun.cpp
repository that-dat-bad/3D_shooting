#include "Sun.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"

void Sun::Initialize() {
	UpdateLight();
}

void Sun::UpdateLight() {
	auto* dirLight = Object3dCommon::GetInstance()->GetDirectionalLightData();
	if (dirLight) {
		dirLight->direction = direction_;
		dirLight->color = color_;
		dirLight->intensity = intensity_;
	}
}

void Sun::UpdateBloom(Camera* camera) {
	if (!camera) return;

	auto* postEffect = PostEffect::GetInstance();
	const MyMath::Matrix4x4& mat = camera->GetWorldMatrix();
	MyMath::Vector3 camForward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

	// 太陽光線と逆の方向（太陽そのものを指すベクトル）
	MyMath::Vector3 sunDirNorm = MyMath::Normalize(MyMath::Vector3{ -direction_.x, -direction_.y, -direction_.z });
	float sunDot = MyMath::Dot(camForward, sunDirNorm);

	if (sunDot > 0.8f) {
		float factor = (sunDot - 0.8f) / 0.2f; // 0.8 〜 1.0 -> 0.0 〜 1.0
		ActivePostEffect sunBloom;
		sunBloom.type = PostEffectType::kBloom;
		sunBloom.intensity = 3.0f;
		sunBloom.dirX = factor * 1.5f; // 正対するほど自然に光があふれる
		sunBloom.dirY = 0.92f;         // 太陽円盤自体の超高輝度部のみにブルームを絞る
		postEffect->AddActiveEffect(sunBloom);
	}
}

void Sun::Draw(Camera* camera) {
	if (!camera) return;

	MyMath::Vector3 camPos = camera->GetTranslate();
	MyMath::Vector3 sunDirNorm = MyMath::Normalize(MyMath::Vector3{ -direction_.x, -direction_.y, -direction_.z });
	
	// カメラ位置 + 太陽方向 * 900m に配置して無限遠をシミュレート
	MyMath::Vector3 sunPos = MyMath::Add(camPos, MyMath::Multiply(900.0f, sunDirNorm));
	
	// 常にカメラを向くビルボード回転
	MyMath::Vector3 rotate = camera->GetRotate();
	
	// 太陽の描画サイズ
	MyMath::Vector3 scale = { 100.0f, 100.0f, 1.0f };
	
	// やや黄色みを帯びた白色光（加算合成）
	MyMath::Vector4 sunColor = { 1.0f, 1.0f, 0.85f, 1.0f };
	
	uint32_t sunTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
	PrimitiveModel::GetInstance()->DrawPlane(scale, rotate, sunPos, sunColor, sunTex, camera, BlendMode::kAdd);
}
