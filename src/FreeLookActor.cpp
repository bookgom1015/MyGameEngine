#include "FreeLookActor.h"
#include "Logger.h"
#include "CameraComponent.h"

using namespace DirectX;

FreeLookActor::FreeLookActor(const std::string& name, XMFLOAT3 pos, XMFLOAT4 rot, XMFLOAT3 scale) : Actor(name, pos, rot, scale) {
	mCameraComp = new CameraComponent(this);

	mForwardSpeed = 0.0f;
	mStrapeSpeed = 0.0f;

	mWalkSpeed = 4.0f;

	mLookUpSpeed = 0.0f;
	mTurnSpeed = 0.0f;

	mLookSensitivity = 0.02f;
	mTurnSensitivity = 0.02f;
}

FreeLookActor::FreeLookActor(const std::string& name, const Transform& trans) : Actor(name, trans) {
	mCameraComp = new CameraComponent(this);

	mForwardSpeed = 0.0f;
	mStrapeSpeed = 0.0f;

	mWalkSpeed = 2.0f;

	mLookUpSpeed = 0.0f;
	mTurnSpeed = 0.0f;

	mLookSensitivity = 0.02f;
	mTurnSensitivity = 0.02f;
}

FreeLookActor::~FreeLookActor() {}

BOOL FreeLookActor::ProcessActorInput(const InputState& input) {
	mForwardSpeed = 0.0f;
	mStrapeSpeed = 0.0f;
	
	float speed = 1.0f;
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
	XMVECTOR strape = mCameraComp->GetRightVector() * mStrapeSpeed;
	XMVECTOR forward = mCameraComp->GetForwardVector() * mForwardSpeed;
	XMVECTOR disp = strape + forward;
	
	FLOAT yaw = mTurnSpeed * mLookSensitivity;
	FLOAT pitch = mLookUpSpeed * mTurnSensitivity;
	
	AddPosition(disp * mWalkSpeed * delta);
	AddRotationYaw(yaw);
	
	mCameraComp->Yaw(yaw);
	mCameraComp->Pitch(pitch);

	return TRUE;
}

const XMVECTOR& FreeLookActor::GetCameraForwardVector() const {
	return mCameraComp->GetForwardVector();
}

const XMVECTOR& FreeLookActor::GetCameraRightVector() const {
	return mCameraComp->GetRightVector();
}

const XMVECTOR& FreeLookActor::GetCameraUpVector() const {
	return mCameraComp->GetUpVector();
}

XMVECTOR FreeLookActor::GetCameraRotation() const {
	return mCameraComp->GetRotation();
}
