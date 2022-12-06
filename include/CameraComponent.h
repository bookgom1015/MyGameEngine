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
	virtual bool OnInitialzing() override;

	virtual bool ProcessInput(const InputState& input) override;
	virtual bool Update(float delta) override;
	virtual bool OnUpdateWorldTransform() override;

	void Pitch(float rad);
	void Yaw(float rad);
	void Roll(float rad);

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

	float mPitch;
	float mYaw;
	float mRoll;

	bool bLimitPitch;
	bool bLimitYaw;
	bool bLimitRoll;

	float mPitchLimit;
	float mYawLimit;
	float mRollLimit;
};