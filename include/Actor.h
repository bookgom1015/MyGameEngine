#pragma once

#include <string>
#include <vector>
#include <memory>
#include <wrl.h>
#include <Windows.h>

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
	BOOL Initialize();
	BOOL ProcessInput(const InputState& input);
	BOOL Update(FLOAT delta);

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

	void AddRotationPitch(FLOAT rad);
	void AddRotationYaw(FLOAT rad);
	void AddRotationRoll(FLOAT rad);

	void SetScale(const DirectX::XMFLOAT3& scale);
	void SetScale(const DirectX::XMVECTOR& scale);

	const std::string& GetName() const;
	const Transform& GetTransform() const;

	__forceinline constexpr BOOL IsInitialized() const;

	__forceinline constexpr BOOL IsDead() const;
	void Die();

protected:
	virtual BOOL OnInitialzing();

	virtual BOOL ProcessActorInput(const InputState& input);
	virtual BOOL UpdateActor(FLOAT delta);

private:
	BOOL UpdateComponents(FLOAT delta);
	BOOL OnUpdateWorldTransform();

private:
	BOOL bInitialized;
	BOOL bDead;
	BOOL bNeedToUpdate;

	std::string mName;
	Transform mTransform;

	std::vector<std::unique_ptr<Component>> mComponents;
};

#include "Actor.inl"