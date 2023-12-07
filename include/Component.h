#pragma once

#include "Transform.h"
#include "InputManager.h"

#include <Windows.h>

class Actor;

class Component {
public:
	Component(Actor* owner);
	virtual ~Component();

public:
	virtual BOOL OnInitialzing();

	virtual BOOL ProcessInput(const InputState& input) = 0;
	virtual BOOL Update(FLOAT delta) = 0;
	virtual BOOL OnUpdateWorldTransform() = 0;

protected:
	const Transform& GetActorTransform();

private:
	Actor* mOwner;
};