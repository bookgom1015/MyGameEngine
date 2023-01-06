#pragma once

#include <Windows.h>

#include "MathHelper.h"
#include "KeyCodes.h"

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
	bool GetKeyValue(int key) const;
	EButtonStates GetKeyState(int key) const;
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
	float GetScrollWheel() const;

	bool GetButtonValue(int button) const;
	EButtonStates GetButtonState(int button) const;

private:
	DirectX::XMFLOAT2 mMousePos;
	DirectX::XMFLOAT2 mMouseDelta;

	float mScrollWheel;
	float mScrollWheelAccum;

	bool mIsRelative;
	bool mIsIgnored;
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
	bool Initialize(HWND hwnd);
	void CleanUp();

	void Update();

	void SetCursorVisibility(BOOL visible);
	void IgnoreMouseInput();
	void SetMouseRelative(bool state);

	const InputState& GetInputState() const;

private:
	bool bIsCleanedUp;

	HWND mhMainWnd;

	InputState mInputState;
};