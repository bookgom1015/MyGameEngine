#pragma once

#include <wrl.h>

#include "MathHelper.h"

class Camera {
public:
	Camera(float nearZ = 0.1f, float farZ = 1000.0f, float fovY = 90.0f);
	virtual ~Camera() = default;

public:
	void UpdateViewMatrix();

	void Pitch(float rad);
	void Yaw(float rad);
	void Roll(float rad);

	void AddPosition(const DirectX::XMVECTOR& pos);
	const DirectX::XMVECTOR& GetPosition() const;
	void SetPosition(const DirectX::XMVECTOR& pos);

	const DirectX::XMFLOAT4X4& GetView() const;
	const DirectX::XMFLOAT4X4& GetProj() const;

	const DirectX::XMVECTOR& GetRightVector() const;
	const DirectX::XMVECTOR& GetUpVector() const;
	const DirectX::XMVECTOR& GetForwardVector() const;

	DirectX::XMVECTOR GetRotation() const;

private:
	DirectX::XMVECTOR mPosition;
	DirectX::XMVECTOR mRight;
	DirectX::XMVECTOR mUp;
	DirectX::XMVECTOR mForward;

	float mNearZ;
	float mFarZ;
	float mFovY;

	bool bViewDirty;

	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
};