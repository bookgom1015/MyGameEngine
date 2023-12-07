#include "RotatingMonkey.h"
#include "Logger.h"
#include "MeshComponent.h"

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

	return true;
}

BOOL RotatingMonkey::ProcessActorInput(const InputState& input) {

	return true;
}

BOOL RotatingMonkey::UpdateActor(FLOAT delta) {
	AddRotationYaw(delta * mSpeed);

	return true;
}