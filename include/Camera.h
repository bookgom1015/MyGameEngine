#pragma once

#include <wrl.h>

#include "MathHelper.h"

class Camera {
public:
	Camera(FLOAT nearZ = 0.1f, FLOAT farZ = 100.0f, FLOAT fovY = 90.0f);
	virtual ~Camera() = default;

public:
	__forceinline FLOAT FovY() const;
	__forceinline FLOAT NearZ() const;
	__forceinline FLOAT FarZ() const;

	const DirectX::XMFLOAT4X4& View() const;
	const DirectX::XMFLOAT4X4& Proj() const;

	const DirectX::XMVECTOR& Position() const;
	DirectX::XMVECTOR Rotation() const;

	const DirectX::XMVECTOR& RightVector() const;
	const DirectX::XMVECTOR& UpVector() const;
	const DirectX::XMVECTOR& ForwardVector() const;

public:
	void UpdateViewMatrix();

	void Pitch(FLOAT rad);
	void Yaw(FLOAT rad);
	void Roll(FLOAT rad);

	void AddPosition(const DirectX::XMVECTOR& pos);
	void SetPosition(const DirectX::XMVECTOR& pos);

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

#include "Camera.inl"