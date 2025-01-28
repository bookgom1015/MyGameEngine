#pragma once

#include <memory>
#include <wrl.h>

#include "Component.h"

class Camera;

class CameraComponent : public Component {
public:
	CameraComponent(Actor* owner);
	virtual ~CameraComponent();

public:
	const DirectX::XMVECTOR& Position() const;
	DirectX::XMVECTOR Rotation() const;

	const DirectX::XMFLOAT4X4& View() const;

	const DirectX::XMVECTOR& RightVector() const;
	const DirectX::XMVECTOR& UpVector() const;
	const DirectX::XMVECTOR& ForwardVector() const;

public:
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessInput(const InputState& input) override;
	virtual BOOL Update(FLOAT delta) override;
	virtual BOOL OnUpdateWorldTransform() override;

	void Pitch(FLOAT rad);
	void Yaw(FLOAT rad);
	void Roll(FLOAT rad);

	void AddPosition(const DirectX::XMVECTOR& pos);
	void SetPosition(const DirectX::XMVECTOR& pos);

private:
	std::unique_ptr<Camera> mCamera;

	FLOAT mPitch;
	FLOAT mYaw;
	FLOAT mRoll;

	BOOL bLimitPitch;
	BOOL bLimitYaw;
	BOOL bLimitRoll;

	FLOAT mPitchLimit;
	FLOAT mYawLimit;
	FLOAT mRollLimit;
};