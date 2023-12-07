#include "SphereActor.h"
#include "Logger.h"
#include "MeshComponent.h"

using namespace DirectX;

SphereActor::SphereActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);
}

SphereActor::SphereActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

SphereActor::~SphereActor() {};

BOOL SphereActor::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("sphere.obj"));

	return true;
}

BOOL SphereActor::ProcessActorInput(const InputState& input) {

	return true;
}

BOOL SphereActor::UpdateActor(FLOAT delta) {

	return true;
}