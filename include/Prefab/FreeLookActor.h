#pragma once

#include "Common/Helper/MathHelper.h"
#include "Common/Actor/Actor.h"

class CameraComponent;

class FreeLookActor : public Actor {
public:
	FreeLookActor(const std::string& name,
		const DirectX::XMFLOAT3& pos   = { 0.f, 0.f, 0.f },
		const DirectX::XMFLOAT4& rot   = { 0.f, 0.f, 0.f, 1.f },
		const DirectX::XMFLOAT3& scale = { 1.f, 1.f, 1.f });
	FreeLookActor(const std::string& name, const Transform& trans);
	virtual ~FreeLookActor();

public:
	const DirectX::XMVECTOR& CameraForwardVector() const;
	const DirectX::XMVECTOR& CameraRightVector() const;
	const DirectX::XMVECTOR& CameraUpVector() const;

	DirectX::XMVECTOR CameraRotation() const;

public:
	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

public:
	CameraComponent* mCameraComp;

	FLOAT mForwardSpeed		= 0.f;
	FLOAT mStrapeSpeed		= 0.f;

	FLOAT mWalkSpeed		= 4.f;

	FLOAT mLookUpSpeed		= 0.f;
	FLOAT mTurnSpeed		= 0.f;

	FLOAT mLookSensitivity	= 0.02f;
	FLOAT mTurnSensitivity	= 0.02f;
};