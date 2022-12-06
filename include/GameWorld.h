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
private:

public:
	GameWorld();
	virtual ~GameWorld();

public:
	bool Initialize();
	bool RunLoop();
	void CleanUp();

	static GameWorld* GetWorld();

	Renderer* GetRenderer() const;
	ActorManager* GetActorManager() const;

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT MsgProc(GLFWwindow* wnd, int key, int code, int action, int mods);

	void OnResize(UINT width, UINT height);

	void OnFocusChanged(int focused);

private:
	bool InitMainWindow();

	void OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);

	bool ProcessInput();
	bool Update();
	bool Draw();

	bool LoadData();

private:
	static GameWorld* sGameWorld;

	bool bIsCleanedUp;

	HINSTANCE mhInst;			// Application instance handle
	HWND mhMainWnd;				// Main window handle
	GLFWwindow* mGlfwWnd;
	bool bAppPaused;			// Is the application paused?
	bool bMinimized;			// Is the application minimized?
	bool bMaximized;			// Is the application maximized?
	bool bResizing;				// Are the resize bars being dragged?
	bool bFullscreenState;		// Fullscreen enabled

	std::unique_ptr<GameTimer> mTimer;
	std::unique_ptr<InputManager> mInputManager;
	std::unique_ptr<Renderer> mRenderer;

	std::unique_ptr<ActorManager> mActorManager;
};