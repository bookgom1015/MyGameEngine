#include "Common/Input/InputManager.h"
#include "Common/Debug/Logger.h"

using namespace DirectX;

namespace {
	BOOL GetKeyButtonValue(INT key) {
		SHORT status = GetAsyncKeyState(key);
		if (status & 0x8000 || status & 0x8001) return TRUE;
		else return FALSE;
	}

	EButtonStates GetKeyButtonState(INT key) {
		SHORT status = GetAsyncKeyState(key);
		if (status & 0x0000) return EButtonStates::ENone;
		else if (status & 0x8000) return EButtonStates::EPressed;
		else if (status & 0x0001) return EButtonStates::EReleased;
		else return EButtonStates::EHeld;
	}
}

BOOL KeyboardState::GetKeyValue(INT key) const {
	return GetKeyButtonValue(key);
}

EButtonStates KeyboardState::GetKeyState(INT key) const {
	return GetKeyButtonState(key);
}

void MouseState::WheelUp() {
	mScrollWheelAccum += 1.f;
}

void MouseState::WheelDown() {
	mScrollWheelAccum -= 1.f;
}

XMFLOAT2 MouseState::GetMousePosition() const {
	return mMousePos;
}

XMFLOAT2 MouseState::GetMouseDelta() const {
	return mMouseDelta;
}

FLOAT MouseState::GetScrollWheel() const {
	return mScrollWheel;
}

BOOL MouseState::GetButtonValue(INT button) const {
	return GetKeyButtonValue(button);
}

EButtonStates MouseState::GetButtonState(INT button) const {
	return GetKeyButtonState(button);
}

InputManager::InputManager() {}

InputManager::~InputManager() {
	if (!bIsCleanedUp) CleanUp();
}

BOOL InputManager::Initialize(HWND hwnd) {
	mhMainWnd = hwnd;

	mInputState.Mouse.mMousePos.x = mInputState.Mouse.mMousePos.y = 0.f;
	mInputState.Mouse.mMouseDelta.x = mInputState.Mouse.mMouseDelta.y = 0.f;
	mInputState.Mouse.mScrollWheel = mInputState.Mouse.mScrollWheelAccum = 0.f;
	mInputState.Mouse.mIsRelative = FALSE;
	mInputState.Mouse.mIsIgnored = TRUE;

	return TRUE;
}

void InputManager::CleanUp() {
	bIsCleanedUp = TRUE;
}

void InputManager::Update() {
	FLOAT prevPosX = mInputState.Mouse.mMousePos.x;
	FLOAT prevPosY = mInputState.Mouse.mMousePos.y;

	RECT wndRect;
	GetWindowRect(mhMainWnd, &wndRect);

	POINT cursorPos;
	GetCursorPos(&cursorPos);

	mInputState.Mouse.mMousePos.x = static_cast<FLOAT>(cursorPos.x) - wndRect.left;
	mInputState.Mouse.mMousePos.y = static_cast<FLOAT>(cursorPos.y) - wndRect.top;

	if (mInputState.Mouse.mIsRelative) {

		const INT centerX = static_cast<INT>((wndRect.left + wndRect.right) * 0.5f);
		const INT centerY = static_cast<INT>((wndRect.top + wndRect.bottom) * 0.5f);

		SetCursorPos(centerX, centerY);

		prevPosX = static_cast<FLOAT>(centerX) - wndRect.left;
		prevPosY = static_cast<FLOAT>(centerY) - wndRect.top;
	}

	mInputState.Mouse.mMouseDelta.x = mInputState.Mouse.mMousePos.x - prevPosX;
	mInputState.Mouse.mMouseDelta.y = mInputState.Mouse.mMousePos.y - prevPosY;

	if (mInputState.Mouse.mIsIgnored) {
		mInputState.Mouse.mIsIgnored = FALSE;
		mInputState.Mouse.mMouseDelta.x = 0.f;
		mInputState.Mouse.mMouseDelta.y = 0.f;
	}
}

void InputManager::SetCursorVisibility(BOOL visible) {
	ShowCursor(visible);
}

void InputManager::IgnoreMouseInput() {
	mInputState.Mouse.mIsIgnored = TRUE;
}

void InputManager::SetMouseRelative(BOOL state) {
	mInputState.Mouse.mIsRelative = state;
}

const InputState& InputManager::GetInputState() const {
	return mInputState;
}