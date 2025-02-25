#include "Common/Actor/ActorManager.h"
#include "Common/Debug/Logger.h"
#include "Common/Actor/Actor.h"

#include <algorithm>

ActorManager::ActorManager() {}

ActorManager::~ActorManager() {
	mDeadActors.clear();
	mRefActors.clear();
}

BOOL ActorManager::ProcessInput(const InputState& input) {
	for (size_t i = 0, end = mActors.size(); i < end; ++i)
		CheckReturn(mActors[i]->ProcessInput(input));
	return TRUE;
}

BOOL ActorManager::Update(FLOAT delta) {
	for (auto& actor : mPendingActors)
		mActors.push_back(std::move(actor));
	mPendingActors.clear();

	bUpdating = TRUE;
	for (size_t i = 0, end = mActors.size(); i < end; ++i) {
		if (!mActors[i]->IsInitialized())
			CheckReturn(mActors[i]->Initialize());
	}
	for (size_t i = 0, end = mActors.size(); i < end; ++i) {
		CheckReturn(mActors[i]->Update(delta));
		if (mActors[i]->IsDead()) mDeadActors.push_back(mActors[i].get());
	}
	bUpdating = FALSE;

	for (const auto actor : mDeadActors) {
		const auto begin = mActors.begin();
		const auto end = mActors.end();
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
	return TRUE;
}

void ActorManager::AddActor(Actor* const actor) {
	if (bUpdating) {
		const auto begin = mPendingActors.begin();
		const auto end = mPendingActors.end();
		const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Actor>& p) {
			return p.get() == actor;
			});
		if (iter != end) return;
		mPendingActors.push_back(std::unique_ptr<Actor>(actor));
	}
	else {
		const auto begin = mActors.begin();
		const auto end = mActors.end();
		const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Actor>& p) {
			return p.get() == actor;
			});
		if (iter != end) return;
		mActors.push_back(std::unique_ptr<Actor>(actor));
	}
	mRefActors[actor->GetName()] = actor;
}

void ActorManager::RemoveActor(Actor* const actor) {
	const auto begin = mDeadActors.begin();
	const auto end = mDeadActors.end();
	const auto iter = std::find(begin, end, actor);
	if (iter != end) return;
	mDeadActors.push_back(actor);
}

Actor* ActorManager::GetActor(const std::string& name) {
	return mRefActors[name];
}