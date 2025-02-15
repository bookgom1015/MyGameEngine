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

#ifdef _DirectX
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#else
	LRESULT MsgProc(GLFWwindow* wnd, INT key, INT code, INT action, INT mods);
#endif

	void OnResize(UINT width, UINT height);

#ifdef _Vulkan
	void OnFocusChanged(INT focused);
#endif

private:
	BOOL InitMainWindow();

	void OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);

	BOOL ProcessInput();
	BOOL Update();
	BOOL Draw();

	BOOL LoadData();

private:
	static GameWorld* sGameWorld;

	HINSTANCE	mhInst				= NULL;		// Application instance handle
	HWND		mhMainWnd			= NULL;		// Main window handle
	GLFWwindow* mGlfwWnd			= nullptr;
	BOOL		bIsCleanedUp		= FALSE;
	BOOL		bAppPaused			= FALSE;	// Is the application paused?
	BOOL		bMinimized			= FALSE;	// Is the application minimized?
	BOOL		bMaximized			= FALSE;	// Is the application maximized?
	BOOL		bResizing			= FALSE;	// Are the resize bars being dragged?
	BOOL		bFullscreenState	= FALSE;	// Fullscreen enabled 

	std::unique_ptr<GameTimer> mTimer;
	std::unique_ptr<InputManager> mInputManager;
	std::unique_ptr<Renderer> mRenderer;

	std::unique_ptr<ActorManager> mActorManager;

	EGameStates mGameState = EGameStates::EGS_Play;

	FLOAT mTimeSlowDown = 1.0f;
};