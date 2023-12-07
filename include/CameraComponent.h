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
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessInput(const InputState& input) override;
	virtual BOOL Update(FLOAT delta) override;
	virtual BOOL OnUpdateWorldTransform() override;

	void Pitch(FLOAT rad);
	void Yaw(FLOAT rad);
	void Roll(FLOAT rad);

	void AddPosition(const DirectX::XMVECTOR& pos);
	const DirectX::XMVECTOR& GetPosition() const;
	void SetPosition(const DirectX::XMVECTOR& pos);

	const DirectX::XMFLOAT4X4& GetView() const;

	const DirectX::XMVECTOR& GetRightVector() const;
	const DirectX::XMVECTOR& GetUpVector() const;
	const DirectX::XMVECTOR& GetForwardVector() const;

	DirectX::XMVECTOR GetRotation() const;

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