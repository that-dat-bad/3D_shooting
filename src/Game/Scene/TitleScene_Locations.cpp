#include "TitleSceneInternal.h"

using namespace TitleSceneInternal;

void TitleScene::InitializeLocations3D(Camera* titleCamera, uint32_t duskSky, uint32_t whiteSky) {
	ModelManager::GetInstance()->LoadModel("Resources/models/m21_wg.gltf");
	ModelManager::GetInstance()->LoadModel("Resources/models/m21.gltf");
	ModelManager::GetInstance()->LoadModel("Resources/models/m21_crashed.gltf");

	locations_.clear();
	locations_.resize(static_cast<size_t>(TitleLocation::Count));

	// ------------------------------------------------------------
	// 1. 地下トンネル基地: トンネル + 道路 + 誘導灯（自発光） + 天井回転灯（飛行中M-21）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::Tunnel)];
		loc.id = TitleLocation::Tunnel;
		loc.name = "Tunnel";
		loc.aircraftModelPath = "Resources/models/m21.gltf"; // 車輪なし・ギア格納飛行モデル
		loc.origin = { 0.0f, 0.0f, 0.0f };
		loc.envModelPath = "Resources/models/title/Tunnel.obj";
		loc.aircraftOffset = { 0.0f, 2.0f, 0.0f };          // トンネル中央の空中に浮上（高度2.0m）
		loc.aircraftRotation = { 0.0f, 3.14159265f, 0.0f }; // 機首を奥（進行方向 +Z）に向けて前進飛行
		loc.propellerRpm = 2500.0f;                          // 巡航飛行推力
		loc.skyboxTexIndex = whiteSky;
		loc.skyboxColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // 天球を完全な黒にして暗黒空間にする

		// 追加パーツ：道路、誘導灯、回転灯
		loc.extraEnvParts.clear();
		{
			// 中央の道路と誘導灯、回転灯
			ExtraEnvPart road;
			road.modelPath = "Resources/models/title/Tunnel_road.obj";
			loc.extraEnvParts.push_back(std::move(road));

			ExtraEnvPart guide;
			guide.modelPath = "Resources/models/title/Tunnel_guide.obj";
			loc.extraEnvParts.push_back(std::move(guide));

			ExtraEnvPart rotateLight;
			rotateLight.modelPath = "Resources/models/title/Tunnel_rotate.obj";
			rotateLight.rotateY = true;
			rotateLight.rotateSpeed = 4.0f; // 回転灯のY軸自転 (rad/sec)
			loc.extraEnvParts.push_back(std::move(rotateLight));

			// トンネルを前後に大幅に連結（端に15個ずつ、中央と合わせて計31個、全長約1.25km）
			constexpr float kTunnelLength = 40.383094f;
			constexpr int kTunnelSegmentsPerSide = 15;
			for (int i = 1; i <= kTunnelSegmentsPerSide; ++i) {
				for (float dir : { -1.0f, 1.0f }) {
					float zOffset = dir * kTunnelLength * static_cast<float>(i);

					ExtraEnvPart tunnelPart;
					tunnelPart.modelPath = "Resources/models/title/Tunnel.obj";
					tunnelPart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(tunnelPart));

					ExtraEnvPart roadPart;
					roadPart.modelPath = "Resources/models/title/Tunnel_road.obj";
					roadPart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(roadPart));

					ExtraEnvPart guidePart;
					guidePart.modelPath = "Resources/models/title/Tunnel_guide.obj";
					guidePart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(guidePart));

					// 2セグメントごと（約80m間隔）に天井警告回転灯を追加配置
					if (i % 2 == 0) {
						ExtraEnvPart rotatePart;
						rotatePart.modelPath = "Resources/models/title/Tunnel_rotate.obj";
						rotatePart.offset = { 0.0f, 0.0f, zOffset };
						rotatePart.rotateY = true;
						rotatePart.rotateSpeed = 4.0f;
						loc.extraEnvParts.push_back(std::move(rotatePart));
					}
				}
			}
		}

		LocationLighting& l = loc.lighting;
		l.lightType = 2 | 4;                              // ポイントライト＋後方投光スポットライトでトンネル壁面・路面を照らす
		l.dirIntensity = 0.0f;
		// スラスターノズル位置（Z=-3.8m）からのエレクトリックブルー照り返し（白飛び抑制：項目1）
		l.pointColor = { 0.18f, 0.60f, 0.95f, 1.0f };
		l.pointOffset = { 0.0f, 1.95f, -3.8f };
		l.pointIntensity = 1.8f;                          // 過剰発光を抑え、機体ディテールを保つ
		l.pointRadius = 22.0f;
		l.pointDecay = 1.2f;
		l.pointFlicker = 0.15f;                           // ジェット燃焼の動的フリッカー
		// 後方および壁面・床面へのスラスター照射
		l.spotColor = { 0.25f, 0.68f, 0.95f, 1.0f };
		l.spotOffset = { 0.0f, 2.0f, -2.5f };
		l.spotDirection = MyMath::Normalize({ 0.0f, -0.35f, -0.93f });
		l.spotIntensity = 1.5f;                           // 過剰発光を抑制
		l.spotDistance = 30.0f;
		l.spotDecay = 1.2f;
		l.spotAngleDeg = 48.0f;
		l.spotFalloffStartDeg = 24.0f;

		// トンネル後方チェイスカメラ（機体テールの単発アフターバーナー＆ショックダイヤモンドを克明に捉える）
		loc.camera = { 8.2f, 0.7f, 1.05f, 0.20f, 0.15f, 0.22f, 0.45f, true };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.50f };
	}

	// ------------------------------------------------------------
	// 2. 格納庫: 元のハンガーモデル + 作業灯 + 天井投光器（金属質感とスペキュラ反射を適正化：項目4, 5, 9）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::Hangar)];
		loc.id = TitleLocation::Hangar;
		loc.name = "Hangar";
		loc.origin = { 0.0f, 0.0f, 0.0f };
		loc.envModelPath = "Resources/models/title/hangar.obj";
		loc.aircraftOffset = { 0.0f, 2.02f, 0.0f };       // 車輪底面（Y=-2.02m）が床面（Y=0）にピタリ接地
		loc.aircraftRotation = { 0.0f, 0.5f, 0.0f };
		loc.skyboxTexIndex = duskSky;                    // 建造物トラスのないピュアスカイでキャノピーへの白い斜線映り込みを根絶
		loc.skyboxColor = { 0.10f, 0.10f, 0.12f, 1.0f }; // 屋内なので空は暗く抑える

		LocationLighting& l = loc.lighting;
		l.lightType = 1 | 2 | 4;
		l.dirColor = { 0.60f, 0.70f, 0.88f, 1.0f };      // シャッター隙間からの青白い外光
		l.dirDirection = MyMath::Normalize({ 0.45f, -0.65f, 0.60f });
		l.dirIntensity = 0.55f;
		l.pointColor = { 1.0f, 0.85f, 0.65f, 1.0f };     // 作業灯（暖色投光）
		l.pointOffset = { -4.5f, 2.5f, -3.5f };
		l.pointIntensity = 1.6f;
		l.pointRadius = 22.0f;
		l.pointDecay = 1.2f;
		l.spotColor = { 0.95f, 0.98f, 1.0f, 1.0f };       // 天井投光器（キャノピー白飛び明滅バグ防止：強度抑制：項目4）
		l.spotOffset = { -2.0f, 10.5f, 2.5f };
		l.spotDirection = MyMath::Normalize({ 0.18f, -0.95f, -0.22f });
		l.spotIntensity = 1.4f;                           // 3.4fから1.4fに抑制！これでキャノピーに白いポリゴンが焼き付かない
		l.spotDistance = 28.0f;
		l.spotDecay = 1.2f;
		l.spotAngleDeg = 42.0f;
		l.spotFalloffStartDeg = 24.0f;

		// 安定した見下ろし・見上げの周回カメラ（注視点高さを持ち上げ、下部バナーとの主翼被りを物理的に解消：項目5）
		loc.camera = { 11.5f, 0.0f, 2.4f, 0.0f, 1.55f, 0.05f, 0.0f, false };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.55f };
	}

	// ------------------------------------------------------------
	// 3. 墜落現場: 広大な荒野地形モデル + 夕暮れ・黄昏天球 + 激突焦げ跡（項目2, 8）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::CrashedForest)];
		loc.id = TitleLocation::CrashedForest;
		loc.name = "CrashedForest";
		loc.origin = { 5000.0f, 0.0f, 5000.0f };
		loc.envModelPath = "assets/models/terrain.obj"; // 起伏のある荒野・地面テクスチャモデル（背景の虚無を解消：項目2）
		loc.envScale = { 15.0f, 2.5f, 15.0f };
		loc.envRotation = { 0.0f, 0.0f, 0.0f };
		loc.aircraftModelPath = "Resources/models/m21_crashed.gltf"; // 激突・焦げ跡テクスチャ適用モデル（項目8）
		loc.aircraftOffset = { 0.0f, 0.32f, 0.0f };         // 機体が地面に激突し食い込む
		loc.aircraftRotation = { 0.22f, -0.85f, 0.38f };    // 機首突っ込み + 左に激しく傾斜
		loc.hiddenParts = {
			DamagePart::Wing_L, DamagePart::Wing1_L, DamagePart::Wing2_L,
			DamagePart::Aileron_L, DamagePart::Tank1,
			DamagePart::Elevator0, DamagePart::Rudder,      // 左水平尾翼・方向舵も千切れ飛ぶ激突ダメージ
		};
		loc.debrisOffset = { -6.5f, 0.38f, 4.2f };          // 千切れた左翼が激突痕に突き刺さる
		loc.debrisRotation = { 0.95f, 0.70f, -0.45f };
		loc.skyboxTexIndex = duskSky;                       // 暗黒の虚無を解消し、夕暮れ・黄昏の空を配置（項目2）
		loc.skyboxColor = { 0.38f, 0.32f, 0.36f, 1.0f };   // 哀愁のある夕暮れトーン
		loc.extraEnvParts.clear();

		LocationLighting& l = loc.lighting;
		l.lightType = 1 | 2;
		l.dirColor = { 0.55f, 0.65f, 0.75f, 1.0f };      // 木漏れの冷たい光
		l.dirDirection = MyMath::Normalize({ 0.3f, -0.8f, -0.4f });
		l.dirIntensity = 0.5f;
		l.pointColor = { 1.0f, 0.45f, 0.15f, 1.0f };     // 炎の照り返し
		l.pointOffset = { 1.5f, 1.2f, -1.0f };
		l.pointIntensity = 1.6f;
		l.pointRadius = 14.0f;
		l.pointDecay = 1.3f;
		l.pointFlicker = 0.35f;

		// めり込み防止：十分な距離（15.5m）と俯瞰高さ（3.2m）で全景を見渡すシネマティックカメラ
		loc.camera = { 15.5f, 0.0f, 3.2f, 0.0f, 0.8f, 0.05f, 0.0f, false };
		loc.grade = { { 0.04f, 0.05f, 0.07f }, 0.04f, 0.5f };
	}

	for (auto& loc : locations_) {
		SetupLocation(loc, titleCamera);
	}
}

void TitleScene::SetupLocation(LocationData& loc, Camera* camera) {
	Object3dCommon* common = Object3dCommon::GetInstance();

	// 背景セットモデル（存在しなければ描画しない＝モデル制作前でも動作する）
	if (!loc.envModelPath.empty()) {
		if (TitleAssetExists(loc.envModelPath)) {
			ModelManager::GetInstance()->LoadModel(loc.envModelPath);
			loc.envObject = std::make_unique<Object3d>();
			loc.envObject->Initialize(common);
			loc.envObject->SetModel(loc.envModelPath);
			loc.envObject->SetCamera(camera);
			loc.envObject->SetBoundingRadius(200.0f); // 大きなセットがLODカリングされないように
		} else {
			OutputDebugStringA(("[TitleScene] Env model not found (skip): " + loc.envModelPath + "\n").c_str());
		}
	}

	// 追加の環境パーツ（道路、誘導灯、回転灯など）
	for (auto& extra : loc.extraEnvParts) {
		if (!extra.modelPath.empty()) {
			if (TitleAssetExists(extra.modelPath)) {
				ModelManager::GetInstance()->LoadModel(extra.modelPath);
				extra.object = std::make_unique<Object3d>();
				extra.object->Initialize(common);
				extra.object->SetModel(extra.modelPath);
				extra.object->SetCamera(camera);
				extra.object->SetBoundingRadius(200.0f);
			} else {
				OutputDebugStringA(("[TitleScene] Extra env model not found (skip): " + extra.modelPath + "\n").c_str());
			}
		}
	}

	// 機体
	loc.visualModel = std::make_unique<AircraftVisualModel>();
	loc.visualModel->Initialize(common, camera);
	loc.visualModel->SetupFromSingleModel(loc.aircraftModelPath);
	loc.visualModel->SetPropellerRpm(loc.propellerRpm);
	if (loc.id == TitleLocation::Tunnel) {
		// トンネル内では前進（+Z方向）に向けるため、モデルの自動180度回転をキャンセル
		loc.visualModel->SetBaseRotation({ 0.0f, 0.0f, 0.0f });
	}
	for (DamagePart part : loc.hiddenParts) {
		loc.visualModel->SetPartVisible(part, false);
	}

	// 千切れたパーツ：本体で消したパーツ「だけ」を表示する別インスタンス
	if (!loc.hiddenParts.empty()) {
		loc.debrisModel = std::make_unique<AircraftVisualModel>();
		loc.debrisModel->Initialize(common, camera);
		loc.debrisModel->SetupFromSingleModel(loc.aircraftModelPath);
		loc.debrisModel->SetPropellerRpm(0.0f);
		for (int i = 0; i < static_cast<int>(DamagePart::Count); ++i) {
			loc.debrisModel->SetPartVisible(static_cast<DamagePart>(i), false);
		}
		for (DamagePart part : loc.hiddenParts) {
			loc.debrisModel->SetPartVisible(part, true);
		}
	}

	// キャノピー（ガラス部分）の半透明・高環境反射・鋭角スペキュラ設定（項目4）
	loc.visualModel->SetupCanopyMaterial();

	// ロケーション別マテリアルの質感設定（項目8: 墜落現場ダメージ、項目9: 重厚なジュラルミン金属感とパネルライン強調）
	if (loc.id == TitleLocation::Hangar) {
		// 格納庫: 軍用機らしいジュラルミン・メタリック質感（モデル本来のカラーを保護）
		loc.visualModel->SetMaterialProperties(48.0f, 1.35f, 0.45f);
	} else if (loc.id == TitleLocation::Tunnel) {
		// トンネル: スラスター光を反射する金属肌（モデル本来のカラーを保護）
		loc.visualModel->SetMaterialProperties(40.0f, 1.20f, 0.40f);
	} else if (loc.id == TitleLocation::CrashedForest) {
		// 墜落現場: 激突・炎上による黒焦げ・煤煙テクスチャを鮮明に映し出すダークトーン（項目8）
		loc.visualModel->SetMaterialProperties(16.0f, 0.25f, 0.05f);
		loc.visualModel->SetMaterialColor({ 0.70f, 0.68f, 0.68f, 1.0f });
		// 残存した右主翼や昇降舵を激突の衝撃力で歪ませる（激突の説得力向上：項目8）
		loc.visualModel->SetPartLocalTransform(DamagePart::Wing_R, { 1.0f, 1.0f, 1.0f }, { 0.08f, -0.05f, 0.12f }, { 0.0f, 0.04f, -0.02f });
		loc.visualModel->SetPartLocalTransform(DamagePart::Wing1_R, { 1.0f, 1.0f, 1.0f }, { 0.10f, -0.06f, 0.15f }, { 0.0f, 0.06f, -0.03f });
		loc.visualModel->SetPartLocalTransform(DamagePart::Elevator1, { 1.0f, 1.0f, 1.0f }, { -0.22f, 0.0f, 0.08f }, { 0.0f, -0.03f, 0.0f });
		if (loc.debrisModel) {
			loc.debrisModel->SetMaterialProperties(14.0f, 0.22f, 0.05f);
			loc.debrisModel->SetMaterialColor({ 0.65f, 0.63f, 0.63f, 1.0f });
		}
	}
}

void TitleScene::ApplyLocationLighting(const LocationData& loc) {
	Object3dCommon* common = Object3dCommon::GetInstance();
	const LocationLighting& l = loc.lighting;
	common->SetLightType(l.lightType);

	if (DirectionalLight* dir = common->GetDirectionalLightData()) {
		dir->color = l.dirColor;
		dir->direction = MyMath::Normalize(l.dirDirection);
		dir->intensity = l.dirIntensity;
	}

	if (PointLight* point = common->GetPointLightData()) {
		float intensity = l.pointIntensity;
		if (l.pointFlicker > 0.0f) {
			// 複数の正弦波を重ねて不規則な炎のゆらぎを作る（0〜1）
			float n = 0.5f
				+ 0.25f * std::sin(animTimer_ * 13.0f)
				+ 0.15f * std::sin(animTimer_ * 29.0f + 1.3f)
				+ 0.10f * std::sin(animTimer_ * 47.0f + 2.1f);
			intensity *= (1.0f - l.pointFlicker) + l.pointFlicker * n * 2.0f;
		}
		point->color = l.pointColor;
		point->position = MyMath::Add(loc.origin, l.pointOffset);
		point->intensity = intensity;
		point->radius = l.pointRadius;
		point->decay = l.pointDecay;
	}

	if (SpotLight* spot = common->GetSpotLightData()) {
		spot->color = l.spotColor;
		spot->position = MyMath::Add(loc.origin, l.spotOffset);
		spot->direction = MyMath::Normalize(l.spotDirection);
		spot->intensity = l.spotIntensity;
		spot->distance = l.spotDistance;
		spot->decay = l.spotDecay;
		spot->cosAngle = std::cos(l.spotAngleDeg * kDegToRad);
		spot->cosFalloffStart = std::cos(l.spotFalloffStartDeg * kDegToRad);
	}
}

void TitleScene::UpdateLocationCamera(const LocationData& loc, float dt, bool menuMode, Camera* targetCamera) {
	Camera* titleCamera = targetCamera ? targetCamera : CameraManager::GetInstance()->GetActiveCamera();
	if (!titleCamera) { return; }

	const LocationCamera& c = loc.camera;

	// メニュー中は少し引いてゆっくり、傾きも穏やかに
	float orbitSpeed = menuMode ? c.orbitSpeed * 1.5f : c.orbitSpeed;
	float distance = menuMode ? c.distance * 1.15f : c.distance + std::sin(animTimer_ * 1.5f) * c.distanceSway;
	float height = c.height + std::sin(animTimer_ * (menuMode ? 0.5f : 2.0f)) * c.heightSway;
	float bankTilt = menuMode ? c.bankTilt * 0.4f : c.bankTilt;

	cameraTheta_ += orbitSpeed * dt;

	if (c.isTunnelCamera) {
		// トンネル専用チェイスカメラワーク：
		// 機体後方（-Z側）から単発アフターバーナーノズル・ショックダイヤモンド・推進炎を正面に捉える
		float swayX = std::sin(cameraTheta_) * 1.3f;
		float currentDist = menuMode ? c.distance * 1.15f : (c.distance + std::sin(animTimer_ * 0.8f) * c.distanceSway);
		float currentH = c.height + std::sin(animTimer_ * 1.1f) * c.heightSway;

		// 機体およびノズルのワールド位置
		// m21.gltf のローカルノズル位置は Z = +14.95m。aircraftRotation (0, π, 0) によりワールドでは Z = -14.95m
		MyMath::Vector3 acPos = MyMath::Add(loc.origin, loc.aircraftOffset);
		float nozzleOffsetZ = afterburner_ ? afterburner_->GetSingleNozzleOffset().z : 14.95f;
		float nozzleOffsetY = afterburner_ ? afterburner_->GetSingleNozzleOffset().y : 0.011f;
		float nozzleOffsetX = afterburner_ ? afterburner_->GetSingleNozzleOffset().x : 0.0f;
		// 機体が 180度回転 (aircraftRotation.y = π) しているため、ローカル+Zはワールド-Z
		MyMath::Vector3 nozzleCenter = { acPos.x - nozzleOffsetX, acPos.y + nozzleOffsetY, acPos.z - nozzleOffsetZ };

		// 注視点：ノズル出口から少し機体側（+Z方向）に入った尾部〜主翼根元付近を注視
		MyMath::Vector3 target;
		target.x = nozzleCenter.x;
		target.y = nozzleCenter.y + c.lookAtHeight;
		target.z = nozzleCenter.z + 2.8f; // ノズルの少し前方（機体内部寄り）を見通すことで自然な構図に

		// カメラ位置：ノズルおよび噴射炎のさらに後方（-Z側）に配置し、少し斜め上方から見下ろす
		MyMath::Vector3 camPos;
		camPos.x = target.x + 0.35f + swayX; // わずかに斜め後方にオフセットして立体感を強調
		camPos.y = nozzleCenter.y + currentH;
		camPos.z = nozzleCenter.z - currentDist; // ノズルから currentDist (約8.2m) 後方

		float dx = target.x - camPos.x;
		float dy = target.y - camPos.y;
		float dz = target.z - camPos.z;
		float yaw = std::atan2(dx, dz);
		float pitch = std::atan2(-dy, std::sqrt(dx * dx + dz * dz));

		// ドローン挙動：左右移動速度（cos）に合わせてカメラを自然にバンク傾き
		float swayVx = std::cos(cameraTheta_) * 1.3f;
		float droneRoll = -swayVx * bankTilt * 0.05f;

		titleCamera->SetTranslate(camPos);
		titleCamera->SetRotate({ pitch, yaw, droneRoll });
		titleCamera->Update();
		return;
	}

	MyMath::Vector3 target = MyMath::Add(loc.origin, loc.aircraftOffset);
	target.y += c.lookAtHeight;

	MyMath::Vector3 camPos;
	camPos.x = target.x + std::cos(cameraTheta_) * distance;
	camPos.y = target.y + height;
	camPos.z = target.z + std::sin(cameraTheta_) * distance;

	// 方向ベクトルの正規化
	MyMath::Vector3 toTarget = MyMath::Subtract(target, camPos);
	MyMath::Vector3 dir = MyMath::Normalize(toTarget);

	// MakeAffineMatrix (Rx * Ry * Rz) に厳密に一致するオイラー角の幾何学解
	// これにより周回時のYaw/Pitchクロスカップリングによる不自然な前後ジッター・ガタつきを根本根絶
	float yaw = std::atan2(dir.x, dir.z);
	float clampedY = std::clamp(dir.y, -0.999f, 0.999f);
	float pitch = -std::asin(clampedY);

	// ドローン挙動：旋回移動による向心バンク傾き（手ぶれ・振動はゼロ、滑らかな旋回傾き）
	float baseRoll = -orbitSpeed * bankTilt * 1.2f;
	float dynamicRoll = baseRoll + std::cos(cameraTheta_) * 0.015f * bankTilt;

	// 墜落現場専用：衝撃で故障しかけた監視カメラの電気的スタッター・微小ブレ
	if (loc.id == TitleLocation::CrashedForest) {
		if (rand() % 100 < 8) { // 8%の確率でカクッとノイズブレ
			float shakeAmt = (static_cast<float>(rand() % 100) - 50.0f) * 0.003f;
			camPos.x += shakeAmt;
			camPos.y += shakeAmt * 0.7f;
			dynamicRoll += shakeAmt * 2.2f;
		}
		float subtleVibe = std::sin(animTimer_ * 28.0f) * 0.006f;
		camPos.y += subtleVibe;
	}

	titleCamera->SetTranslate(camPos);
	titleCamera->SetRotate({ pitch, yaw, dynamicRoll });
	titleCamera->Update();
}

void TitleScene::UpdateLocations3D(float dt) {
	LocationData& loc = CurrentLocation();

	// 現在ロケーションの機体・残骸・背景を更新
	{
		MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
		MyMath::Vector3 rot = loc.aircraftRotation;

		// トンネル内の飛行モーション（低空巡航飛行の浮遊動揺・バンク・推力微振動）
		if (loc.id == TitleLocation::Tunnel && enableTunnelMotion_) {
			float swayY = std::sin(animTimer_ * 2.2f) * 0.08f + std::sin(animTimer_ * 4.5f) * 0.02f;
			float swayX = std::sin(animTimer_ * 1.3f) * 0.18f;
			float engineVibe = std::sin(animTimer_ * 48.0f) * 0.003f;
			pos.x += swayX;
			pos.y += swayY + engineVibe;

			// 機体の姿勢（バンク傾き ＆ 迎え角ピッチ）
			float roll = -std::cos(animTimer_ * 1.3f) * 0.045f;
			float pitch = 0.025f + std::sin(animTimer_ * 2.2f) * 0.015f;
			rot.x += pitch;
			rot.z += roll;
		}

		currentAircraftMatrix_ = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rot, pos);
		loc.visualModel->Update(currentAircraftMatrix_, dt);
		if (loc.debrisModel) {
			MyMath::Vector3 debrisPos = MyMath::Add(loc.origin, loc.debrisOffset);
			loc.debrisModel->Update(MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.debrisRotation, debrisPos), dt);
		}

		// トンネル専用：手前方向へ超高速スクロールして猛烈な飛行前進感を演出（時速約470km/h）
		float tunnelScrollZ = 0.0f;
		if (loc.id == TitleLocation::Tunnel) {
			constexpr float kTunnelLength = 40.383094f;
			constexpr float kFlightSpeed = 130.0f; // 飛行速度 (m/s) 約468km/h
			tunnelScrollZ = std::fmod(animTimer_ * kFlightSpeed, kTunnelLength);
		}

		// 墜落現場専用：折れた左翼付け根・破損エンジンから立ち上るリアルな濃煙と微小火の粉（項目4）
		if (loc.id == TitleLocation::CrashedForest) {
			crashSmokeTimer_ += dt;
			if (crashSmokeTimer_ >= 0.035f) {
				crashSmokeTimer_ = 0.0f;
				MyMath::Vector3 smokePos = MyMath::Add(loc.origin, loc.aircraftOffset);
				smokePos.x -= 1.6f;
				smokePos.y += 0.7f;
				smokePos.z += 0.2f;

				ParticleParameters smokeParams;
				smokeParams.minVelocity = { -0.012f, 0.028f, -0.012f };
				smokeParams.maxVelocity = {  0.012f, 0.065f,  0.012f };
				// R=G=Bを厳格に完全同値化し、ピンクや緑の偏色を100%根絶（項目6: 漆黒・チャコールグレー重煙）
				float g = 0.08f + static_cast<float>(rand() % 100) / 100.0f * 0.10f; // 0.08～0.18
				smokeParams.minColor = { g, g, g, 0.88f };
				smokeParams.maxColor = { g, g, g, 0.94f };
				smokeParams.minLifeTime = 3.5f;
				smokeParams.maxLifeTime = 5.2f;
				smokeParams.minScale = 1.2f;
				smokeParams.maxScale = 4.8f;
				smokeParams.scaleEasing = 0.40f;
				smokeParams.randomPositionRange = 0.35f;
				smokeParams.acceleration = { 0.0004f, 0.0001f, 0.0002f };
				ParticleManager::GetInstance()->Emit("TitleSmoke", smokePos, smokeParams, 2);
			}

			crashSparkTimer_ += dt;
			if (crashSparkTimer_ >= 0.12f) { // 火花は控えめに、極小の赤熱火の粉（ember）のみ
				crashSparkTimer_ = 0.0f;
				MyMath::Vector3 sparkPos = MyMath::Add(loc.origin, loc.aircraftOffset);
				sparkPos.x -= 1.5f;
				sparkPos.y += 0.5f;
				sparkPos.z += 0.1f;

				ParticleParameters sparkParams;
				sparkParams.minVelocity = { -0.025f, 0.020f, -0.025f };
				sparkParams.maxVelocity = {  0.025f, 0.055f,  0.025f };
				sparkParams.minColor = { 1.0f, 0.35f, 0.05f, 0.90f };
				sparkParams.maxColor = { 1.0f, 0.80f, 0.20f, 0.95f };
				sparkParams.minLifeTime = 0.4f;
				sparkParams.maxLifeTime = 0.85f;
				sparkParams.minScale = 0.03f;
				sparkParams.maxScale = 0.07f; // 微小な火の粉
				sparkParams.randomPositionRange = 0.15f;
				sparkParams.acceleration = { 0.0f, -0.0020f, 0.0f };
				ParticleManager::GetInstance()->Emit("TitleSpark", sparkPos, sparkParams, 1);
			}
		}

		if (loc.envObject) {
			MyMath::Vector3 envPos = loc.origin;
			envPos.z -= tunnelScrollZ; // 機体前進に伴い背景は手前（-Z方向）へスクロール
			loc.envObject->SetTranslate(envPos);
			loc.envObject->SetRotate(loc.envRotation);
			loc.envObject->SetScale(loc.envScale);
			loc.envObject->Update();
		}
		for (auto& extra : loc.extraEnvParts) {
			if (extra.object) {
				if (extra.rotateY) {
					extra.currentAngle += extra.rotateSpeed * dt;
					if (extra.currentAngle > 6.2831853f) {
						extra.currentAngle -= 6.2831853f;
					}
				}
				MyMath::Vector3 partRot = MyMath::Add(extra.rotation, { 0.0f, extra.currentAngle, 0.0f });
				MyMath::Vector3 partPos = MyMath::Add(loc.origin, extra.offset);
				partPos.z -= tunnelScrollZ; // 追加パーツも手前（-Z方向）へスクロール
				extra.object->SetTranslate(partPos);
				extra.object->SetRotate(partRot);
				extra.object->SetScale(extra.scale);
				extra.object->Update();
			}
		}

		if (loc.id == TitleLocation::Tunnel) {
			Model* guideModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_guide.obj");
			if (guideModel) {
				guideModel->SetMaterialEnableLighting(0, false);
				guideModel->SetMaterialColor(0, { guideEmissiveColor_.x, guideEmissiveColor_.y, guideEmissiveColor_.z, 1.0f });
				guideModel->SetMaterialEmissive(0, guideEmissiveColor_, guideEmissiveIntensity_);
			}
		}
	}

	ApplyLocationLighting(loc);
}

void TitleScene::PrewarmAssets() {
	PrewarmAllTitleCamerasAndLocations(0.016f);
}

void TitleScene::PrewarmAllTitleCamerasAndLocations(float dt) {
	CameraManager* cm = CameraManager::GetInstance();
	if (!cm) { return; }

	// 1. メインの TitleCamera を取得または作成
	if (!cm->HasCamera("TitleCamera")) {
		cm->CreateCamera("TitleCamera");
	}
	Camera* titleCamera = cm->GetCamera("TitleCamera");
	if (!titleCamera) {
		titleCamera = cm->GetActiveCamera();
	}

	// 2. タイトルの全ロケーション専用カメラ（Hangar, Tunnel, CrashedForest）を作成・登録
	const char* camNames[] = {
		"TitleCamera_Hangar",
		"TitleCamera_Tunnel",
		"TitleCamera_CrashedForest"
	};

	for (int i = 0; i < 3; ++i) {
		if (!cm->HasCamera(camNames[i])) {
			cm->CreateCamera(camNames[i]);
		}
	}

	if (prewarmPhase_ == 0) {
		// ============================================================
		// Phase 0: タイトルの全カメラ（周回・定点・Live Feed用）のパラメータ計算・更新
		// ============================================================
		for (size_t i = 0; i < locations_.size() && i < 3; ++i) {
			const LocationData& loc = locations_[i];
			Camera* cam = cm->GetCamera(camNames[i]);
			if (cam) {
				UpdateLocationCamera(loc, 0.0f, false, cam);
			}
		}

		// メインカメラ (TitleCamera) をメニュー遷移時ロケーションに合わせて先行更新
		if (!locations_.empty()) {
			int targetLocIdx = (CurrentLocation().id == TitleLocation::CrashedForest) ? 0 : currentLocationIndex_;
			if (targetLocIdx >= 0 && targetLocIdx < static_cast<int>(locations_.size())) {
				UpdateLocationCamera(locations_[targetLocIdx], 0.0f, true, titleCamera);
			}
		}

		// Skybox と Afterburner を TitleCamera に接続・更新
		if (skybox_) {
			skybox_->SetCamera(titleCamera);
			skybox_->Update();
		}
		if (afterburner_) {
			afterburner_->Update(dt, afterburner_->GetIntensity());
		}

		loadingPhaseText_ = "PREWARMING HANGAR & CRASHED FOREST ASSETS...";
		prewarmPhase_ = 1;
		OutputDebugStringA("[TitleScene] Loading Phase 0 complete: All Title Cameras & Optical Links initialized.\n");
	} else if (prewarmPhase_ == 1) {
		// ============================================================
		// Phase 1: Hangar (CH 01) & CrashedForest (CH 03) の全オブジェクト行列更新
		// ============================================================
		for (size_t i : { static_cast<size_t>(TitleLocation::Hangar), static_cast<size_t>(TitleLocation::CrashedForest) }) {
			if (i < locations_.size()) {
				LocationData& loc = locations_[i];
				if (titleCamera) {
					if (loc.envObject) loc.envObject->SetCamera(titleCamera);
					for (auto& extra : loc.extraEnvParts) {
						if (extra.object) extra.object->SetCamera(titleCamera);
					}
					if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
					if (loc.debrisModel) loc.debrisModel->SetCamera(titleCamera);
				}

				// 機体モデルの行列計算
				MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
				Matrix4x4 acMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.aircraftRotation, pos);
				if (loc.visualModel) {
					loc.visualModel->Update(acMat, dt);
				}

				// 残骸モデルの行列計算
				if (loc.debrisModel) {
					MyMath::Vector3 debrisPos = MyMath::Add(loc.origin, loc.debrisOffset);
					Matrix4x4 debMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.debrisRotation, debrisPos);
					loc.debrisModel->Update(debMat, dt);
				}

				// 背景セットモデル
				if (loc.envObject) {
					loc.envObject->SetScale(loc.envScale);
					loc.envObject->SetRotate(loc.envRotation);
					loc.envObject->SetTranslate(loc.origin);
					loc.envObject->Update();
				}

				// 追加パーツ群
				for (auto& extra : loc.extraEnvParts) {
					if (extra.object) {
						MyMath::Vector3 ep = MyMath::Add(loc.origin, extra.offset);
						extra.object->SetScale(extra.scale);
						extra.object->SetRotate(extra.rotation);
						extra.object->SetTranslate(ep);
						extra.object->Update();
					}
				}
			}
		}

		loadingPhaseText_ = "SYNCHRONIZING TUNNEL PIPELINES & GPU BUFFERS...";
		prewarmPhase_ = 2;
		OutputDebugStringA("[TitleScene] Loading Phase 1 complete: Hangar & Crashed Forest 3D assets prewarmed.\n");
	} else if (prewarmPhase_ == 2) {
		// ============================================================
		// Phase 2: Tunnel (CH 02) の全オブジェクト（31セグメント約100パーツ）更新 & テクスチャフラッシュ
		// ============================================================
		if (static_cast<size_t>(TitleLocation::Tunnel) < locations_.size()) {
			LocationData& loc = locations_[static_cast<size_t>(TitleLocation::Tunnel)];
			if (titleCamera) {
				if (loc.envObject) loc.envObject->SetCamera(titleCamera);
				for (auto& extra : loc.extraEnvParts) {
					if (extra.object) extra.object->SetCamera(titleCamera);
				}
				if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
			}

			// 機体モデルの行列計算
			MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
			Matrix4x4 acMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.aircraftRotation, pos);
			if (loc.visualModel) {
				loc.visualModel->Update(acMat, dt);
			}

			// トンネル中央背景
			if (loc.envObject) {
				loc.envObject->SetScale(loc.envScale);
				loc.envObject->SetRotate(loc.envRotation);
				loc.envObject->SetTranslate(loc.origin);
				loc.envObject->Update();
			}

			// トンネルの全セグメントパーツ（約100パーツ）
			for (auto& extra : loc.extraEnvParts) {
				if (extra.object) {
					MyMath::Vector3 ep = MyMath::Add(loc.origin, extra.offset);
					extra.object->SetScale(extra.scale);
					extra.object->SetRotate(extra.rotation);
					extra.object->SetTranslate(ep);
					extra.object->Update();
				}
			}
		}

		// GPUテクスチャアップロードの同期完了
		DirectXCommon::GetInstance()->FlushTextureUploads();

		loadingPhaseText_ = "FINALIZING ALL TITLE CAMERAS & DESKTOP ROOT...";
		prewarmPhase_ = 3;
		OutputDebugStringA("[TitleScene] Loading Phase 2 complete: Tunnel 100+ segments & GPU textures flushed.\n");
	} else if (prewarmPhase_ == 3) {
		// ============================================================
		// Phase 3: 全カメラ・UIの最終確定
		// ============================================================
		menuCardPanel_.Update();
		menuHeaderPanel_.Update();
		if (consoleBgSprite_) { consoleBgSprite_->Update(); }

		// 現在ロケーションのカメラを再確定
		int targetLocIdx = (CurrentLocation().id == TitleLocation::CrashedForest) ? 0 : currentLocationIndex_;
		if (targetLocIdx >= 0 && targetLocIdx < static_cast<int>(locations_.size())) {
			UpdateLocationCamera(locations_[targetLocIdx], 0.0f, true, titleCamera);
		}

		loadingPhaseText_ = "ALL TITLE CAMERAS SYNCHRONIZED";
		prewarmPhase_ = 4;
		isTitleCamerasPrewarmed_ = true;
		OutputDebugStringA("[TitleScene] Loading Phase 3 complete: ALL TITLE CAMERAS & LOCATIONS FULLY SYNCHRONIZED!\n");
	}
}

void TitleScene::ChangeLocation(int index) {
	if (locations_.empty()) { return; }
	currentLocationIndex_ = index % static_cast<int>(locations_.size());
	cutTimer_ = 0.0f;
	transitionTimer_ = kTransitionDuration; // カメラ切替砂嵐トランジションを発動（項目10）
	cameraTheta_ = static_cast<float>(rand() % 628) * 0.01f; // ランダムな角度から映す
	LocationData& loc = CurrentLocation();
	ApplyLocationLighting(loc);
	if (skybox_) {
		skybox_->SetTextureIndex(loc.skyboxTexIndex);
		skybox_->SetColor(loc.skyboxColor);
	}
}
