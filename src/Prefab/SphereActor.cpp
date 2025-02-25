#include "Prefab/SphereActor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/MeshComponent.h"

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

	return TRUE;
}

BOOL SphereActor::ProcessActorInput(const InputState& input) {

	return TRUE;
}

BOOL SphereActor::UpdateActor(FLOAT delta) {

	return TRUE;
}