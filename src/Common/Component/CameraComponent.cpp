#include "Common/Component/CameraComponent.h"
#include "Common/Debug/Logger.h"
#include "Common/Camera/Camera.h"
#include "Common/Render/Renderer.h"
#include "Common/GameWorld.h"

using namespace DirectX;

CameraComponent::CameraComponent(Actor* const owner) : Component(owner) {
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

	return TRUE;
}

BOOL CameraComponent::ProcessInput(const InputState& input) { return TRUE; }

BOOL CameraComponent::Update(FLOAT delta) {	return TRUE; }

BOOL CameraComponent::OnUpdateWorldTransform() {
	mCamera->SetPosition(GetActorTransform().Position);
	mCamera->UpdateViewMatrix();

	return TRUE;
}

void CameraComponent::Pitch(FLOAT rad) {
	if (bLimitPitch) {
		const FLOAT newPitch = mPitch + rad;
		if (newPitch >= mPitchLimit) rad = mPitchLimit - mPitch;
		else if (newPitch <= -mPitchLimit) rad = -mPitchLimit - mPitch;
		mPitch += rad;
	}
	else {
		mPitch += rad;
		if (mPitch >= XM_2PI) mPitch = 0.f;
		else if (mPitch <= -XM_2PI) mPitch = 0.f;
	}
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