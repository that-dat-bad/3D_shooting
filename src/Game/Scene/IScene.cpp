#include "IScene.h"
#include"Input.h"
#include <cstring>

int IScene::sceneID = SCENE::TITLE;

IScene::~IScene() = default;

bool IScene::IsKeyTriggered(BYTE keyNumber) {
	return Input::GetInstance()->TriggerKey(keyNumber);
}

bool IScene::IsKeyPressed(BYTE keyNumber) {
	return Input::GetInstance()->PushKey(keyNumber);
}

int IScene::GetSceneID() {
	return sceneID;
}

void IScene::SetSceneID(int id) {
	sceneID = id;
}

