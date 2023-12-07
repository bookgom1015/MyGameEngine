#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <wrl.h>

#include "InputManager.h"

class Actor;

class ActorManager {
public:
	ActorManager();
	virtual ~ActorManager();

public:
	BOOL ProcessInput(const InputState& input);
	BOOL Update(FLOAT delta);

	void AddActor(Actor* actor);
	void RemoveActor(Actor* actor);
	Actor* GetActor(const std::string& name);

private:
	BOOL bUpdating;

	std::vector<std::unique_ptr<Actor>> mActors;
	std::vector<std::unique_ptr<Actor>> mPendingActors;

	std::vector<Actor*> mDeadActors;
	std::unordered_map<std::string, Actor*> mRefActors;
};