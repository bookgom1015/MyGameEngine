#include "Prefab/FreeLookActor.h"
#include "Common/Debug/Logger.h"
#include "Common/Component/CameraComponent.h"

using namespace DirectX;

FreeLookActor::FreeLookActor(const std::string& name, const XMFLOAT3& pos, const XMFLOAT4& rot, const XMFLOAT3& scale) : Actor(name, pos, rot, scale) {
	mCameraComp = new CameraComponent(this);
}

FreeLookActor::FreeLookActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mCameraComp = new CameraComponent(this);

	mForwardSpeed = 0.f;
	mStrapeSpeed = 0.f;

	mWalkSpeed = 2.f;

	mLookUpSpeed = 0.f;
	mTurnSpeed = 0.f;

	mLookSensitivity = 0.02f;
	mTurnSensitivity = 0.02f;
}

FreeLookActor::~FreeLookActor() {}

const XMVECTOR& FreeLookActor::CameraForwardVector() const {
	return mCameraComp->ForwardVector();
}

const XMVECTOR& FreeLookActor::CameraRightVector() const {
	return mCameraComp->RightVector();
}

const XMVECTOR& FreeLookActor::CameraUpVector() const {
	return mCameraComp->UpVector();
}

XMVECTOR FreeLookActor::CameraRotation() const {
	return mCameraComp->Rotation();
}

BOOL FreeLookActor::ProcessActorInput(const InputState& input) {
	mForwardSpeed = 0.f;
	mStrapeSpeed = 0.f;
	
	float speed = 1.f;
	if (input.Keyboard.GetKeyValue(VK_LSHIFT)) speed *= 0.1f;

	if (input.Keyboard.GetKeyValue(VK_W)) mForwardSpeed	+= speed;
	if (input.Keyboard.GetKeyValue(VK_S)) mForwardSpeed	+= -speed;
	if (input.Keyboard.GetKeyValue(VK_A)) mStrapeSpeed	+= -speed;
	if (input.Keyboard.GetKeyValue(VK_D)) mStrapeSpeed	+= speed;
	
	mLookUpSpeed = input.Mouse.GetMouseDelta().y;
	mTurnSpeed = input.Mouse.GetMouseDelta().x;

	return TRUE;
}

BOOL FreeLookActor::UpdateActor(FLOAT delta) {
	XMVECTOR strape = mCameraComp->RightVector() * mStrapeSpeed;
	XMVECTOR forward = mCameraComp->ForwardVector() * mForwardSpeed;
	XMVECTOR disp = strape + forward;
	
	FLOAT yaw = mTurnSpeed * mLookSensitivity;
	FLOAT pitch = mLookUpSpeed * mTurnSensitivity;
	
	AddPosition(disp * mWalkSpeed * delta);
	AddRotationYaw(yaw);
	
	mCameraComp->Yaw(yaw);
	mCameraComp->Pitch(pitch);

	return TRUE;
}