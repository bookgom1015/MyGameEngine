#include "Common/Component/Component.h"
#include "Common/Actor/Actor.h"

Component::Component(Actor* const owner) {
	mOwner = owner;
	owner->AddComponent(this);
}

Component::~Component() {}

BOOL Component::OnInitialzing() { return TRUE; }

const Transform& Component::GetActorTransform() {
	return mOwner->GetTransform();
}