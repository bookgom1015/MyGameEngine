#pragma once

#include "Transform.h"
#include "InputManager.h"

class Actor;

class Component {
public:
	Component(Actor* owner);
	virtual ~Component();

public:
	virtual bool OnInitialzing();

	virtual bool ProcessInput(const InputState& input) = 0;
	virtual bool Update(float delta) = 0;
	virtual bool OnUpdateWorldTransform() = 0;

protected:
	const Transform& GetActorTransform();

private:
	Actor* mOwner;
};