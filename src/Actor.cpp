#include "Actor.h"
#include "Logger.h"
#include "Component.h"
#include "GameWorld.h"
#include "ActorManager.h"

#include <algorithm>

using namespace DirectX;

Actor::Actor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) {
	bInitialized = bDead = false;
	bNeedToUpdate = true;

	mName = name;
	mTransform.Position = XMLoadFloat3(&pos);
	mTransform.Rotation = XMLoadFloat4(&rot);
	mTransform.Scale = XMLoadFloat3(&scale);

	GameWorld::GetWorld()->GetActorManager()->AddActor(this);
}

Actor::Actor(const std::string& name, const Transform& trans) {
	bInitialized = bDead = false;
	bNeedToUpdate = true;

	mName = name;
	mTransform = trans;

	GameWorld::GetWorld()->GetActorManager()->AddActor(this);
}

Actor::~Actor() {}

BOOL Actor::Initialize() {
	CheckReturn(OnInitialzing());

	for (const auto& comp : mComponents)
		CheckReturn(comp->OnInitialzing());

	bInitialized = true;
	return true;
}

BOOL Actor::ProcessInput(const InputState& input) {
	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		mComponents[i]->ProcessInput(input);

	ProcessActorInput(input);

	return true;
}

BOOL Actor::Update(FLOAT delta) {
	CheckReturn(OnUpdateWorldTransform());

	CheckReturn(UpdateComponents(delta));
	CheckReturn(UpdateActor(delta));

	CheckReturn(OnUpdateWorldTransform());

	return true;
}

void Actor::AddComponent(Component* comp) {
	auto begin = mComponents.begin();
	auto end = mComponents.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Component>& p) {
		return p.get() == comp;
		});
	if (iter != end) return;
	mComponents.push_back(std::unique_ptr<Component>(comp));
}

void Actor::RemoveComponent(Component* comp) {
	auto begin = mComponents.begin();
	auto end = mComponents.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Component>& p) {
		return p.get() == comp;
		});
	if (iter != end) {
		std::iter_swap(iter, end - 1);
		mComponents.pop_back();
	}
}

void Actor::SetPosition(const XMFLOAT3& pos) {
	bNeedToUpdate = true;
	mTransform.Position = XMLoadFloat3(&pos);
}

void Actor::SetPosition(const DirectX::XMVECTOR& pos) {
	bNeedToUpdate = true;
	mTransform.Position = pos;
}

void Actor::AddPosition(const XMFLOAT3& pos) {
	bNeedToUpdate = true;
	mTransform.Position += XMLoadFloat3(&pos);
}

void Actor::AddPosition(const DirectX::XMVECTOR& pos) {
	bNeedToUpdate = true;
	mTransform.Position += pos;
}

void Actor::SetRotation(const XMFLOAT4& rot) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMLoadFloat4(&rot);
}

void Actor::SetRotation(const DirectX::XMVECTOR& rot) {
	bNeedToUpdate = true;
	mTransform.Rotation = rot;
}

void Actor::AddRotation(const XMFLOAT4& rot) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMLoadFloat4(&rot));
}

void Actor::AddRotation(const DirectX::XMVECTOR& rot) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, rot);
}

void Actor::AddRotationPitch(FLOAT rad) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVectors::RightVector, rad));
}

void Actor::AddRotationYaw(FLOAT rad) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVectors::UpVector, rad));
}

void Actor::AddRotationRoll(FLOAT rad) {
	bNeedToUpdate = true;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVectors::ForwardVector, rad));
}

void Actor::SetScale(const XMFLOAT3& scale) {
	bNeedToUpdate = true;
	mTransform.Scale = XMLoadFloat3(&scale);
}

void Actor::SetScale(const DirectX::XMVECTOR& scale) {
	bNeedToUpdate = true;
	mTransform.Scale = scale;
}

const std::string& Actor::GetName() const {
	return mName;
}

const Transform& Actor::GetTransform() const {
	return mTransform;
}

void Actor::Die() {
	bDead = true;
}

BOOL Actor::OnInitialzing() { return true; }

BOOL Actor::ProcessActorInput(const InputState& input) { return true; }

BOOL Actor::UpdateActor(FLOAT delta) { return true; }

BOOL Actor::UpdateComponents(FLOAT delta) {
	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		CheckReturn(mComponents[i]->Update(delta));

	return true;
}

BOOL Actor::OnUpdateWorldTransform() {
	if (!bNeedToUpdate) return true;

	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		CheckReturn(mComponents[i]->OnUpdateWorldTransform());

	bNeedToUpdate = false;
	return true;
}