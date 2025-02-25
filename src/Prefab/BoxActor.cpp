#include "Prefab/BoxActor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/MeshComponent.h"

using namespace DirectX;

BoxActor::BoxActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);
}

BoxActor::BoxActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

BoxActor::~BoxActor() {}

BOOL BoxActor::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("box.obj"));

	return TRUE;
}

BOOL BoxActor::ProcessActorInput(const InputState& input) {
	return TRUE;
}

BOOL BoxActor::UpdateActor(FLOAT delta) {
	return TRUE;
}