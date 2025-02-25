#pragma once

#include "Common/Mesh/Transform.h"
#include "Common/Input/InputManager.h"

#include <Windows.h>

class Actor;

class Component {
public:
	Component(Actor* const owner);
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