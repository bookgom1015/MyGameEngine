#pragma once

#include <Windows.h>

#include "Common/Helper/MathHelper.h"
#include "Common/KeyCodes.h"

enum class EButtonStates {
	ENone,
	EPressed,
	EReleased,
	EHeld
};

class KeyboardState {
	friend class InputManager;

public:
	KeyboardState() = default;
	virtual ~KeyboardState() = default;

public:
	BOOL GetKeyValue(INT key) const;
	EButtonStates GetKeyState(INT key) const;
};

class MouseState {
	friend class InputManager;

public:
	MouseState() = default;
	virtual ~MouseState() = default;

public:
	void WheelUp();
	void WheelDown();

	DirectX::XMFLOAT2 GetMousePosition() const;
	DirectX::XMFLOAT2 GetMouseDelta() const;
	FLOAT GetScrollWheel() const;

	BOOL GetButtonValue(INT button) const;
	EButtonStates GetButtonState(INT button) const;

private:
	DirectX::XMFLOAT2 mMousePos;
	DirectX::XMFLOAT2 mMouseDelta;

	FLOAT mScrollWheel;
	FLOAT mScrollWheelAccum;

	BOOL mIsRelative;
	BOOL mIsIgnored;
};

class ControllerState {
	friend class InputManager;

public:
	ControllerState() = default;
	virtual ~ControllerState() = default;
};

struct InputState {
public:
	KeyboardState Keyboard;
	MouseState Mouse;
	ControllerState Controller;
};

class InputManager {
public:
	InputManager();
	virtual ~InputManager();

private:
	InputManager(const InputManager& ref) = delete;
	InputManager(InputManager&& rval) = delete;
	InputManager& operator=(const InputManager& ref) = delete;
	InputManager& operator=(InputManager&& rval) = delete;

public:
	BOOL Initialize(HWND hwnd);
	void CleanUp();

	void Update();

	void SetCursorVisibility(BOOL visible);
	void IgnoreMouseInput();
	void SetMouseRelative(BOOL state);

	const InputState& GetInputState() const;

private:
	BOOL bIsCleanedUp = FALSE;

	HWND mhMainWnd;

	InputState mInputState;
};