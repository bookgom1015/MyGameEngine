#include "Camera.h"
#include "Logger.h"
#include "GameWorld.h"
#include "Renderer.h"

using namespace DirectX;

Camera::Camera(FLOAT nearZ, FLOAT farZ, FLOAT fovY) : bViewDirty(true) {
	mNearZ = nearZ;
	mFarZ = farZ;
	mFovY = fovY;
	mPosition = XMVectorZero();
	mRight = UnitVector::RightVector;
	mUp = UnitVector::UpVector;
	mForward = UnitVector::ForwardVector;
}

void Camera::UpdateViewMatrix() {
	if (!bViewDirty) return;
	bViewDirty = false;
	
	XMStoreFloat4x4(
		&mView,
		XMMatrixLookAtLH(
			mPosition,
			mPosition + mForward,
			mUp
		)
	);

	XMStoreFloat4x4(
		&mProj,
		XMMatrixPerspectiveFovLH(
			XM_PIDIV2, 
			GameWorld::GetWorld()->GetRenderer()->AspectRatio(), 
			0.1f, 
			1000.0f
		)
	);
}

void Camera::Pitch(FLOAT rad) {
	auto quat = XMQuaternionRotationAxis(mRight, rad);
	
	mUp = XMVector3Rotate(mUp, quat);
	mForward = XMVector3Rotate(mForward, quat);
	
	bViewDirty = true;
}

void Camera::Yaw(FLOAT rad) {
	auto quat = XMQuaternionRotationAxis(UnitVector::UpVector, rad);
	
	mRight = XMVector3Rotate(mRight, quat);
	mUp = XMVector3Rotate(mUp, quat);
	mForward = XMVector3Rotate(mForward, quat);
	
	bViewDirty = true;
}

void Camera::Roll(FLOAT rad) {
	auto quat = XMQuaternionRotationAxis(mForward, rad);

	mRight = XMVector3Rotate(mRight, quat);
	mUp = XMVector3Rotate(mUp, quat);

	bViewDirty = true;
}

void Camera::AddPosition(const XMVECTOR& pos) {
	mPosition += pos;

	bViewDirty = true;
}

const XMVECTOR& Camera::GetPosition() const {
	return mPosition;
}

void Camera::SetPosition(const XMVECTOR& pos) {
	mPosition = pos;

	bViewDirty = true;
}

const XMFLOAT4X4& Camera::GetView() const {
	return mView;
}

const XMFLOAT4X4& Camera::GetProj() const {
	return mProj;
}

const XMVECTOR& Camera::GetRightVector() const {
	return mRight;
}

const XMVECTOR& Camera::GetUpVector() const {
	return mUp;
}

const XMVECTOR& Camera::GetForwardVector() const {
	return mForward;
}

XMVECTOR Camera::GetRotation() const {
	return XMQuaternionRotationMatrix(XMMatrixLookAtLH(
		XMVectorZero(),
		UnitVector::ForwardVector,
		mUp
	));
}