#include "Common/Component/MeshComponent.h"
#include "Common/Debug/Logger.h"
#include "Common/Render/Renderer.h"
#include "Common/GameWorld.h"

MeshComponent::MeshComponent(Actor* const owner) : Component(owner) {}

MeshComponent::~MeshComponent() {
	GameWorld::GetWorld()->GetRenderer()->RemoveModel(mModel);
}

BOOL MeshComponent::ProcessInput(const InputState& input) { return TRUE; }

BOOL MeshComponent::Update(FLOAT delta) { return TRUE; }

BOOL MeshComponent::OnUpdateWorldTransform() {
	GameWorld::GetWorld()->GetRenderer()->UpdateModel(mModel, GetActorTransform());

	return TRUE;
}

BOOL MeshComponent::LoadMesh(const std::string& file) {
	mModel = GameWorld::GetWorld()->GetRenderer()->AddModel(file, GetActorTransform());
	if (mModel == nullptr) {
		std::wstringstream wsstream;
		wsstream << L"Failed to add the model; " << file.c_str();
		ReturnFalse(wsstream.str());
	}

	return TRUE;
}

void MeshComponent::SetVisibility(BOOL visible) {
	
}

void MeshComponent::SetPickable(BOOL pickable) {
	GameWorld::GetWorld()->GetRenderer()->SetModelPickable(mModel, FALSE);
}