#include "PlaneActor.h"
#include "Logger.h"
#include "MeshComponent.h"

using namespace DirectX;

PlaneActor::PlaneActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);
}

PlaneActor::PlaneActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

PlaneActor::~PlaneActor() {};

bool PlaneActor::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("plane.obj"));

	return true;
}

bool PlaneActor::ProcessActorInput(const InputState& input) {

	return true;
}

bool PlaneActor::UpdateActor(float delta) {

	return true;
}