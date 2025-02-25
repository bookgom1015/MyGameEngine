#include "Prefab/RotatingMonkey.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/MeshComponent.h"

using namespace DirectX;

RotatingMonkey::RotatingMonkey(const std::string& name, FLOAT speed, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);

	mSpeed = speed;
}

RotatingMonkey::RotatingMonkey(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

RotatingMonkey::~RotatingMonkey() {};

BOOL RotatingMonkey::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("monkey.obj"));

	return TRUE;
}

BOOL RotatingMonkey::ProcessActorInput(const InputState& input) {

	return TRUE;
}

BOOL RotatingMonkey::UpdateActor(FLOAT delta) {
	AddRotationYaw(delta * mSpeed);

	return TRUE;
}