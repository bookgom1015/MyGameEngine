#pragma once

#include <string>
#include <vector>
#include <memory>
#include <wrl.h>

#include "Transform.h"
#include "InputManager.h"

class Component;

class Actor {
public:
	Actor(const std::string& name,
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT4 rot = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });
	Actor(const std::string& name, const Transform& trans);
	virtual ~Actor();

public:
	bool Initialize();
	bool ProcessInput(const InputState& input);
	bool Update(float delta);

	void AddComponent(Component* comp);
	void RemoveComponent(Component* comp);

	void SetPosition(const DirectX::XMFLOAT3& pos);
	void SetPosition(const DirectX::XMVECTOR& pos);
	void AddPosition(const DirectX::XMFLOAT3& pos);
	void AddPosition(const DirectX::XMVECTOR& pos);

	void SetRotation(const DirectX::XMFLOAT4& rot);
	void SetRotation(const DirectX::XMVECTOR& rot);
	void AddRotation(const DirectX::XMFLOAT4& rot);
	void AddRotation(const DirectX::XMVECTOR& rot);

	void AddRotationPitch(float rad);
	void AddRotationYaw(float rad);
	void AddRotationRoll(float rad);

	void SetScale(const DirectX::XMFLOAT3& scale);
	void SetScale(const DirectX::XMVECTOR& scale);

	const std::string& GetName() const;
	const Transform& GetTransform() const;

	__forceinline constexpr bool IsInitialized() const;

	__forceinline constexpr bool IsDead() const;
	void Die();

protected:
	virtual bool OnInitialzing();

	virtual bool ProcessActorInput(const InputState& input);
	virtual bool UpdateActor(float delta);

private:
	bool UpdateComponents(float delta);
	bool OnUpdateWorldTransform();

private:
	bool bInitialized;
	bool bDead;
	bool bNeedToUpdate;

	std::string mName;
	Transform mTransform;

	std::vector<std::unique_ptr<Component>> mComponents;
};

#include "Actor.inl"