#pragma once

#include <wrl.h>

#include "MathHelper.h"

class Camera {
public:
	Camera(FLOAT nearZ = 0.1f, FLOAT farZ = 1000.0f, FLOAT fovY = 90.0f);
	virtual ~Camera() = default;

public:
	__forceinline FLOAT FovY() const;

public:
	void UpdateViewMatrix();

	void Pitch(FLOAT rad);
	void Yaw(FLOAT rad);
	void Roll(FLOAT rad);

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

	FLOAT mNearZ;
	FLOAT mFarZ;
	FLOAT mFovY;

	BOOL bViewDirty;

	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
};

FLOAT Camera::FovY() const {
	return mFovY;
}