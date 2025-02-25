#include "Common/Actor/Actor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/Component.h"
#include "Common/Actor/ActorManager.h"
#include "Common/GameWorld.h"

#include <algorithm>

using namespace DirectX;

Actor::Actor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) {
	mName = name;
	mTransform.Position = XMLoadFloat3(&pos);
	mTransform.Rotation = XMLoadFloat4(&rot);
	mTransform.Scale = XMLoadFloat3(&scale);

	GameWorld::GetWorld()->GetActorManager()->AddActor(this);
}

Actor::Actor(const std::string& name, const Transform& trans) {
	mName = name;
	mTransform = trans;

	GameWorld::GetWorld()->GetActorManager()->AddActor(this);
}

Actor::~Actor() {}

BOOL Actor::Initialize() {
	CheckReturn(OnInitialzing());

	for (const auto& comp : mComponents)
		CheckReturn(comp->OnInitialzing());

	bInitialized = TRUE;
	return TRUE;
}

BOOL Actor::ProcessInput(const InputState& input) {
	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		mComponents[i]->ProcessInput(input);

	ProcessActorInput(input);

	return TRUE;
}

BOOL Actor::Update(FLOAT delta) {
	CheckReturn(OnUpdateWorldTransform());

	CheckReturn(UpdateComponents(delta));
	CheckReturn(UpdateActor(delta));

	CheckReturn(OnUpdateWorldTransform());

	return TRUE;
}

void Actor::AddComponent(Component* const comp) {
	const auto begin = mComponents.begin();
	const auto end = mComponents.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Component>& p) {
		return p.get() == comp;
		});
	if (iter != end) return;
	mComponents.push_back(std::unique_ptr<Component>(comp));
}

void Actor::RemoveComponent(Component* const comp) {
	const auto begin = mComponents.begin();
	const auto end = mComponents.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<Component>& p) {
		return p.get() == comp;
		});
	if (iter != end) {
		std::iter_swap(iter, end - 1);
		mComponents.pop_back();
	}
}

void Actor::SetPosition(const XMFLOAT3& pos) {
	bNeedToUpdate = TRUE;
	mTransform.Position = XMLoadFloat3(&pos);
}

void Actor::SetPosition(const DirectX::XMVECTOR& pos) {
	bNeedToUpdate = TRUE;
	mTransform.Position = pos;
}

void Actor::AddPosition(const XMFLOAT3& pos) {
	bNeedToUpdate = TRUE;
	mTransform.Position += XMLoadFloat3(&pos);
}

void Actor::AddPosition(const DirectX::XMVECTOR& pos) {
	bNeedToUpdate = TRUE;
	mTransform.Position += pos;
}

void Actor::SetRotation(const XMFLOAT4& rot) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMLoadFloat4(&rot);
}

void Actor::SetRotation(const DirectX::XMVECTOR& rot) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = rot;
}

void Actor::AddRotation(const XMFLOAT4& rot) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMLoadFloat4(&rot));
}

void Actor::AddRotation(const DirectX::XMVECTOR& rot) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, rot);
}

void Actor::AddRotationPitch(FLOAT rad) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVector::RightVector, rad));
}

void Actor::AddRotationYaw(FLOAT rad) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVector::UpVector, rad));
}

void Actor::AddRotationRoll(FLOAT rad) {
	bNeedToUpdate = TRUE;
	mTransform.Rotation = XMQuaternionMultiply(mTransform.Rotation, XMQuaternionRotationAxis(UnitVector::ForwardVector, rad));
}

void Actor::SetScale(const XMFLOAT3& scale) {
	bNeedToUpdate = TRUE;
	mTransform.Scale = XMLoadFloat3(&scale);
}

void Actor::SetScale(const DirectX::XMVECTOR& scale) {
	bNeedToUpdate = TRUE;
	mTransform.Scale = scale;
}

const std::string& Actor::GetName() const {
	return mName;
}

const Transform& Actor::GetTransform() const {
	return mTransform;
}

void Actor::Die() {
	bDead = TRUE;
}

BOOL Actor::OnInitialzing() { return TRUE; }

BOOL Actor::ProcessActorInput(const InputState& input) { return TRUE; }

BOOL Actor::UpdateActor(FLOAT delta) { return TRUE; }

BOOL Actor::UpdateComponents(FLOAT delta) {
	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		CheckReturn(mComponents[i]->Update(delta));

	return TRUE;
}

BOOL Actor::OnUpdateWorldTransform() {
	if (!bNeedToUpdate) return TRUE;

	for (size_t i = 0, end = mComponents.size(); i < end; ++i)
		CheckReturn(mComponents[i]->OnUpdateWorldTransform());

	bNeedToUpdate = FALSE;
	return TRUE;
}