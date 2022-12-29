#include "RotatingMonkey.h"
#include "Logger.h"
#include "MeshComponent.h"

using namespace DirectX;

RotatingMonkey::RotatingMonkey(const std::string& name, float speed, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);

	mSpeed = speed;
}

RotatingMonkey::RotatingMonkey(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

RotatingMonkey::~RotatingMonkey() {};

bool RotatingMonkey::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("monkey.obj"));

	return true;
}

bool RotatingMonkey::ProcessActorInput(const InputState& input) {

	return true;
}

bool RotatingMonkey::UpdateActor(float delta) {
	AddRotationYaw(delta * mSpeed);

	return true;
}