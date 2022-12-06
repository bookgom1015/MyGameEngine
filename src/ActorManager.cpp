#include "ActorManager.h"
#include "Logger.h"
#include "Actor.h"

#include <algorithm>

ActorManager::ActorManager() {
	bUpdating = false;
}

ActorManager::~ActorManager() {
	mDeadActors.clear();
	mRefActors.clear();
}

bool ActorManager::ProcessInput(const InputState& input) {
	for (size_t i = 0, end = mActors.size(); i < end; ++i)
		CheckReturn(mActors[i]->ProcessInput(input));
	return true;
}

bool ActorManager::Update(float delta) {
	for (auto& actor : mPendingActors)
		mActors.push_back(std::move(actor));
	mPendingActors.clear();

	bUpdating = true;
	for (size_t i = 0, end = mActors.size(); i < end; ++i) {
		if (!mActors[i]->IsInitialized())
			CheckReturn(mActors[i]->Initialize());
	}
	for (size_t i = 0, end = mActors.size(); i < end; ++i) {
		CheckReturn(mActors[i]->Update(delta));
		if (mActors[i]->IsDead()) mDeadActors.push_back(mActors[i].get());
	}
	bUpdating = false;

	for (const auto actor : mDeadActors) {
		auto begin = mActors.begin();
		auto end = mActors.end();
		const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Actor>& p) {
			return p.get() == actor;
			});
		if (iter != end) {
			mRefActors.erase(actor->GetName());
			std::iter_swap(iter, end - 1);
			mActors.pop_back();
		}
	}
	mDeadActors.clear();
	return true;
}

void ActorManager::AddActor(Actor* actor) {
	if (bUpdating) {
		auto begin = mPendingActors.begin();
		auto end = mPendingActors.end();
		const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Actor>& p) {
			return p.get() == actor;
			});
		if (iter != end) return;
		mPendingActors.push_back(std::unique_ptr<Actor>(actor));
	}
	else {
		auto begin = mActors.begin();
		auto end = mActors.end();
		const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Actor>& p) {
			return p.get() == actor;
			});
		if (iter != end) return;
		mActors.push_back(std::unique_ptr<Actor>(actor));
	}
	mRefActors[actor->GetName()] = actor;
}

void ActorManager::RemoveActor(Actor* actor) {
	auto begin = mDeadActors.begin();
	auto end = mDeadActors.end();
	auto iter = std::find(begin, end, actor);
	if (iter != end) return;
	mDeadActors.push_back(actor);
}

Actor* ActorManager::GetActor(const std::string& name) {
	return mRefActors[name];
}