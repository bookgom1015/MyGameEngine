#include "Prefab/PlaneActor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/MeshComponent.h"

using namespace DirectX;

PlaneActor::PlaneActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);
}

PlaneActor::PlaneActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

PlaneActor::~PlaneActor() {};

BOOL PlaneActor::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("plane.obj"));
	mMeshComp->SetPickable(FALSE);

	return TRUE;
}

BOOL PlaneActor::ProcessActorInput(const InputState& input) {

	return TRUE;
}

BOOL PlaneActor::UpdateActor(FLOAT delta) {

	return TRUE;
}