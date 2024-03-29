#include "Component.h"
#include "Actor.h"

Component::Component(Actor* owner) {
	mOwner = owner;
	owner->AddComponent(this);
}

Component::~Component() {}

BOOL Component::OnInitialzing() { return true; }

const Transform& Component::GetActorTransform() {
	return mOwner->GetTransform();
}