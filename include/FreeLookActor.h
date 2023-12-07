#pragma once

#include "Actor.h"

class CameraComponent;

class FreeLookActor : public Actor {
public:
	FreeLookActor(const std::string& name,
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT4 rot = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });
	FreeLookActor(const std::string& name, const Transform& trans);
	virtual ~FreeLookActor();

public:
	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

	const DirectX::XMVECTOR& GetCameraForwardVector() const;
	const DirectX::XMVECTOR& GetCameraRightVector() const;
	const DirectX::XMVECTOR& GetCameraUpVector() const;

	DirectX::XMVECTOR GetCameraRotation() const;

public:
	CameraComponent* mCameraComp;

	FLOAT mForwardSpeed;
	FLOAT mStrapeSpeed;

	FLOAT mWalkSpeed;

	FLOAT mLookUpSpeed;
	FLOAT mTurnSpeed;

	FLOAT mLookSensitivity;
	FLOAT mTurnSensitivity;
};