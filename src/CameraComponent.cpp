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

bool CameraComponent::OnInitialzing() {
	GameWorld::GetWorld()->GetRenderer()->SetCamera(mCamera.get());

	return true;
}

bool CameraComponent::ProcessInput(const InputState& input) { return true; }

bool CameraComponent::Update(float delta) {	return true; }

bool CameraComponent::OnUpdateWorldTransform() {
	mCamera->SetPosition(GetActorTransform().Position);
	mCamera->UpdateViewMatrix();

	return true;
}

void CameraComponent::Pitch(float rad) {
	if (bLimitPitch) {
		float newPitch = mPitch + rad;
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

void CameraComponent::Yaw(float rad) {
	mCamera->Yaw(rad);
}

void CameraComponent::Roll(float rad) {
	mCamera->Roll(rad);
}

void CameraComponent::AddPosition(const XMVECTOR& pos) {
	mCamera->AddPosition(pos);
}

const XMVECTOR& CameraComponent::GetPosition() const {
	return mCamera->GetPosition();
}

void CameraComponent::SetPosition(const XMVECTOR& pos) {
	mCamera->SetPosition(pos);
}

const XMFLOAT4X4& CameraComponent::GetView() const {
	return mCamera->GetView();
}

const XMVECTOR& CameraComponent::GetRightVector() const {
	return mCamera->GetRightVector();
}

const XMVECTOR& CameraComponent::GetUpVector() const {
	return mCamera->GetUpVector();
}

const XMVECTOR& CameraComponent::GetForwardVector() const {
	return mCamera->GetForwardVector();
}

XMVECTOR CameraComponent::GetRotation() const {
	return mCamera->GetRotation();
}