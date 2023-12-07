#pragma once

#include <Windows.h>
#include <wrl.h>
#include <memory>

class GameTimer;
class InputManager;
class Renderer;
class ActorManager;
class Camera;

struct GLFWwindow;

class GameWorld {
public:
	enum EGameStates {
		EGS_Play,
		EGS_UI
	};

public:
	GameWorld();
	virtual ~GameWorld();

public:
	BOOL Initialize();
	BOOL RunLoop();
	void CleanUp();

	static GameWorld* GetWorld();

	Renderer* GetRenderer() const;
	ActorManager* GetActorManager() const;

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT MsgProc(GLFWwindow* wnd, INT key, INT code, INT action, INT mods);

	void OnResize(UINT width, UINT height);

	void OnFocusChanged(INT focused);

private:
	BOOL InitMainWindow();

	void OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);

	BOOL ProcessInput();
	BOOL Update();
	BOOL Draw();

	BOOL LoadData();

private:
	static GameWorld* sGameWorld;

	BOOL bIsCleanedUp;

	HINSTANCE mhInst;			// Application instance handle
	HWND mhMainWnd;				// Main window handle
	GLFWwindow* mGlfwWnd;
	BOOL bAppPaused;			// Is the application paused?
	BOOL bMinimized;			// Is the application minimized?
	BOOL bMaximized;			// Is the application maximized?
	BOOL bResizing;				// Are the resize bars being dragged?
	BOOL bFullscreenState;		// Fullscreen enabled 

	std::unique_ptr<GameTimer> mTimer;
	std::unique_ptr<InputManager> mInputManager;
	std::unique_ptr<Renderer> mRenderer;

	std::unique_ptr<ActorManager> mActorManager;

	EGameStates mGameState;

	FLOAT mTimeSlowDown;
};