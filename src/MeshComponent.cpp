#include "MeshComponent.h"
#include "Logger.h"
#include "GameWorld.h"
#include "Renderer.h"

MeshComponent::MeshComponent(Actor* owner) : Component(owner) {}

MeshComponent::~MeshComponent() {
	GameWorld::GetWorld()->GetRenderer()->RemoveModel(mModel);
}

BOOL MeshComponent::ProcessInput(const InputState& input) { return true; }

BOOL MeshComponent::Update(FLOAT delta) { return true; }

BOOL MeshComponent::OnUpdateWorldTransform() {
	GameWorld::GetWorld()->GetRenderer()->UpdateModel(mModel, GetActorTransform());

	return true;
}

BOOL MeshComponent::LoadMesh(const std::string& file) {
	mModel = GameWorld::GetWorld()->GetRenderer()->AddModel(file, GetActorTransform());
	if (mModel == nullptr) {
		std::wstringstream wsstream;
		wsstream << L"Failed to add the model; " << file.c_str();
		ReturnFalse(wsstream.str());
	}

	return true;
}

void MeshComponent::SetVisibility(BOOL visible) {
	
}

void MeshComponent::SetPickable(BOOL pickable) {
	GameWorld::GetWorld()->GetRenderer()->SetModelPickable(mModel, false);
}