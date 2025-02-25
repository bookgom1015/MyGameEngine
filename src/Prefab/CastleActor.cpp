#include "Prefab/CastleActor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/MeshComponent.h"

using namespace DirectX;

CastleActor::CastleActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mMeshComp = new MeshComponent(this);
}

CastleActor::CastleActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mMeshComp = new MeshComponent(this);
}

CastleActor::~CastleActor() {}

BOOL CastleActor::OnInitialzing() {
	CheckReturn(mMeshComp->LoadMesh("castle.obj"));

	return TRUE;
}

BOOL CastleActor::ProcessActorInput(const InputState& input) {
	return TRUE;
}

BOOL CastleActor::UpdateActor(FLOAT delta) {
	return TRUE;
}