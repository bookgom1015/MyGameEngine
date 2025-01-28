#include "CameraComponent.h"
#include "Logger.h"
#include "Camera.h"
#include "GameWorld.h"
#include "Renderer.h"

using namespace DirectX;

CameraComponent::CameraComponent(Actor* owner) : Component(owner) {
	mPitch = 0.0f;
	mYaw = 0.0f;
	mRoll = 0.0f;

	bLimitPitch = true;
	bLimitYaw = false;
	bLimitRoll = false;

	mPitchLimit = XM_PIDIV2 - 0.1f;
	mYawLimit = 0.0f;
	mRollLimit = 0.0f;

	mCamera = std::make_unique<Camera>();
}

CameraComponent::~CameraComponent() {}

const XMVECTOR& CameraComponent::Position() const {
	return mCamera->Position();
}

XMVECTOR CameraComponent::Rotation() const {
	return mCamera->Rotation();
}

const XMFLOAT4X4& CameraComponent::View() const {
	return mCamera->View();
}

const XMVECTOR& CameraComponent::RightVector() const {
	return mCamera->RightVector();
}

const XMVECTOR& CameraComponent::UpVector() const {
	return mCamera->UpVector();
}

const XMVECTOR& CameraComponent::ForwardVector() const {
	return mCamera->ForwardVector();
}

BOOL CameraComponent::OnInitialzing() {
	GameWorld::GetWorld()->GetRenderer()->SetCamera(mCamera.get());

	return true;
}

BOOL CameraComponent::ProcessInput(const InputState& input) { return true; }

BOOL CameraComponent::Update(FLOAT delta) {	return true; }

BOOL CameraComponent::OnUpdateWorldTransform() {
	mCamera->SetPosition(GetActorTransform().Position);
	mCamera->UpdateViewMatrix();

	return true;
}

void CameraComponent::Pitch(FLOAT rad) {
	if (bLimitPitch) {
		FLOAT newPitch = mPitch + rad;
		if (newPitch >= mPitchLimit) rad = mPitchLimit - mPitch;
		else if (newPitch <= -mPitchLimit) rad = -mPitchLimit - mPitch;
		mPitch += rad;
	}
	else {
		mPitch += rad;
		if (mPitch >= XM_2PI) mPitch = 0.0f;
		else if (mPitch <= -XM_2PI) mPitch = 0.0f;
	}
	//Logln(std::to_string(mPitch));
	mCamera->Pitch(rad);
}

void CameraComponent::Yaw(FLOAT rad) {
	mCamera->Yaw(rad);
}

void CameraComponent::Roll(FLOAT rad) {
	mCamera->Roll(rad);
}

void CameraComponent::AddPosition(const XMVECTOR& pos) {
	mCamera->AddPosition(pos);
}

void CameraComponent::SetPosition(const XMVECTOR& pos) {
	mCamera->SetPosition(pos);
}