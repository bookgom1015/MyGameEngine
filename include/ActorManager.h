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
	bool ProcessInput(const InputState& input);
	bool Update(float delta);

	void AddActor(Actor* actor);
	void RemoveActor(Actor* actor);
	Actor* GetActor(const std::string& name);

private:
	bool bUpdating;

	std::vector<std::unique_ptr<Actor>> mActors;
	std::vector<std::unique_ptr<Actor>> mPendingActors;

	std::vector<Actor*> mDeadActors;
	std::unordered_map<std::string, Actor*> mRefActors;
};