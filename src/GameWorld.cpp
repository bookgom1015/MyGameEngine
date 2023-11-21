#include "GameWorld.h"
#include "Logger.h"
#include "GameTimer.h"
#include "InputManager.h"
#include "ActorManager.h"
#include "Camera.h"

#include "FreeLookActor.h"
#include "SphereActor.h"
#include "PlaneActor.h"
#include "RotatingMonkey.h"

#ifdef _DirectX
#include "DxRenderer.h"
#else
#include "VkRenderer.h"
#endif

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <exception>

#undef min
#undef max

using namespace DirectX;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	try {
		GameWorld game;

		if (!game.Initialize()) return -1;
		if (!game.RunLoop()) {
			WLogln(L"Error occured");
			return -1;
		}
		game.CleanUp();

		Logln("The game has been successfully finished");
		return 0;
	}
	catch (const std::exception& e) {
		Logln("Exception catched: ", e.what());
		return -1;
	}
}

namespace {
	const UINT InitClientWidth = 800;
	const UINT InitClientHeight = 600;

	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid
		return GameWorld::GetWorld()->MsgProc(hwnd, msg, wParam, lParam);
	}

	void GLFWKeyCallback(GLFWwindow* wnd, int key, int code, int action, int mods) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(wnd));
		game->MsgProc(wnd, key, code, action, mods);
	}

	void GLFWResizeCallback(GLFWwindow* wnd, int width, int height) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(wnd));
		game->OnResize(width, height);
	}

	void GLFWFocusCallback(GLFWwindow* pWnd, int inState) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		game->OnFocusChanged(inState);
	}
}

GameWorld* GameWorld::sGameWorld = nullptr;

GameWorld::GameWorld() {
	sGameWorld = this;

	bIsCleanedUp = false;

	mhInst = NULL;
	mhMainWnd = NULL;
	mGlfwWnd = nullptr;
	bIsCleanedUp = false;
	bAppPaused = false;
	bMinimized = false;
	bMaximized = false;
	bResizing = false;
	bFullscreenState = false;

	mTimer = std::make_unique<GameTimer>();
	mInputManager = std::make_unique<InputManager>();
#ifdef _DirectX
	mRenderer = std::make_unique<DxRenderer>();
#else
	mRenderer = std::make_unique<VkRenderer>();
#endif
	mActorManager = std::make_unique<ActorManager>();

	mGameState = EGameStates::EGS_Play;

	mTimeSlowDown = 1.0f;
}

GameWorld::~GameWorld() {
	if (!bIsCleanedUp) CleanUp();
}

bool GameWorld::Initialize() {
	Logger::LogHelper::StaticInit();

	CheckReturn(InitMainWindow());
	CheckReturn(mInputManager->Initialize(mhMainWnd));
	CheckReturn(mRenderer->Initialize(mhMainWnd, mGlfwWnd, InitClientWidth, InitClientHeight));

	mInputManager->SetMouseRelative(true);
	mInputManager->SetCursorVisibility(false);

	mTimer->SetLimitFrameRate(GameTimer::E_LimitFrameRate60f);

	return true;
}

bool GameWorld::RunLoop() {
	MSG msg = { 0 };

	float beginTime = 0.0f;
	float endTime = 0.0f;
	float elapsedTime = 0.0f;

	mTimer->Reset();

	if (!LoadData()) return S_FALSE;

#ifdef _DirectX
	while (msg.message != WM_QUIT) {
		// If there are Window messages then process them
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff
		else {
			mTimer->Tick();

			endTime = mTimer->TotalTime();
			elapsedTime = endTime - beginTime;

			if (elapsedTime >= mTimer->GetLimitFrameRate()) {
				beginTime = endTime;

				if (!bAppPaused) CheckReturn(ProcessInput());
				CheckReturn(Update());
				if (!bAppPaused) CheckReturn(Draw());
			}

			if (bAppPaused) Sleep(33);
		}
	}
#else
	while (!glfwWindowShouldClose(mGlfwWnd)) {
		glfwPollEvents();

		mTimer->Tick();		

		endTime = mTimer->TotalTime();
		elapsedTime = endTime - beginTime;

		if (elapsedTime >= mTimer->GetLimitFrameRate()) {
			beginTime = endTime;

			if (!bAppPaused) CheckReturn(ProcessInput());
			CheckReturn(Update());
			CheckReturn(Draw());
		}

		if (bAppPaused) Sleep(33);
	}
#endif

	return true;
}

void GameWorld::CleanUp() {
	if (mInputManager != nullptr) mInputManager->CleanUp();
	if (mRenderer != nullptr) mRenderer->CleanUp();

#ifdef _Vulkan
	glfwDestroyWindow(mGlfwWnd);
	glfwTerminate();
#endif

	bIsCleanedUp = true;
}

GameWorld* GameWorld::GetWorld() { return sGameWorld; }

Renderer* GameWorld::GetRenderer() const { return mRenderer.get(); }

ActorManager* GameWorld::GetActorManager() const { return mActorManager.get(); }

LRESULT GameWorld::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static UINT width = InitClientWidth;
	static UINT height = InitClientHeight;

	if ((mGameState == EGameStates::EGS_UI) && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 0;

	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			bAppPaused = true;
		}
		else {
			mInputManager->IgnoreMouseInput();
			bAppPaused = false;
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
	{
		// Save the new client area dimensions.
		width = LOWORD(lParam);
		height = HIWORD(lParam);
		if (mRenderer->IsInitialized()) {
			if (wParam == SIZE_MINIMIZED) {
				bAppPaused = true;
				bMinimized = true;
				bMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED) {
				bAppPaused = false;
				bMinimized = false;
				bMaximized = true;
				OnResize(width, height);
			}
			else if (wParam == SIZE_RESTORED) {
				// Restoring from minimized state?
				if (bMinimized) {
					bAppPaused = false;
					bMinimized = false;
					OnResize(width, height);
				}

				// Restoring from maximized state?
				else if (bMaximized) {
					bAppPaused = false;
					bMaximized = false;
					OnResize(width, height);
				}
				// If user is dragging the resize bars, we do not resize 
				// the buffers here because as the user continuously 
				// drags the resize bars, a stream of WM_SIZE messages are
				// sent to the window, and it would be pointless (and slow)
				// to resize for each WM_SIZE message received from dragging
				// the resize bars.  So instead, we reset after the user is 
				// done resizing the window and releases the resize bars, which 
				// sends a WM_EXITSIZEMOVE message.
				else if (bResizing) {

				}
				// API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				else {
					OnResize(width, height);
				}
			}
		}
		return 0;
	}

	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mTimer->Stop();
		bAppPaused = true;
		bResizing = true;
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mTimer->Start();
		bAppPaused = false;
		bResizing = false;
		OnResize(width, height);
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		return 0;

	case WM_KEYUP:
		OnKeyboardInput(WM_KEYUP, wParam, lParam);
		return 0;

	case WM_KEYDOWN:
		OnKeyboardInput(WM_KEYDOWN, wParam, lParam);
		return 0;

	case WM_LBUTTONDOWN: {
		const auto pos = mInputManager->GetInputState().Mouse.GetMousePosition();
		if (mGameState == EGameStates::EGS_UI) mRenderer->Pick(pos.x, pos.y);
		return 0;
	}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT GameWorld::MsgProc(GLFWwindow* wnd, int key, int code, int action, int mods) {
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS) {
			glfwSetWindowShouldClose(mGlfwWnd, GLFW_TRUE);
		}
		return 0;
	}

	return 0;
}

void GameWorld::OnResize(UINT width, UINT height) {
	if (!mRenderer->OnResize(width, height)) {
		PostQuitMessage(0);
	}
}

void GameWorld::OnFocusChanged(int focused) {
	bAppPaused = (focused == GLFW_TRUE ? false : true);
	mInputManager->IgnoreMouseInput();
}

bool GameWorld::InitMainWindow() {
#ifdef _DirectX
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MyGameEngine";
	if (!RegisterClass(&wc))  ReturnFalse(L"Failed to register the window class");

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, static_cast<LONG>(InitClientWidth), static_cast<LONG>(InitClientHeight) };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	int outputWidth = GetSystemMetrics(SM_CXSCREEN);
	int outputHeight = GetSystemMetrics(SM_CYSCREEN);

	int clientPosX = static_cast<int>((outputWidth - InitClientWidth) * 0.5f);
	int clientPosY = static_cast<int>((outputHeight - InitClientHeight) * 0.5f);

	mhMainWnd = CreateWindow(
		L"MyGameEngine", L"MyGameEngine - DirectX",
		WS_OVERLAPPEDWINDOW,
		clientPosX, clientPosY,
		width, height,
		0, 0, mhInst, 0);
	if (!mhMainWnd) ReturnFalse(L"Failed to create the window");

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);
#else
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mGlfwWnd = glfwCreateWindow(InitClientWidth, InitClientHeight, "MyGameEngine - Vulkan", nullptr, nullptr);
	mhMainWnd = glfwGetWin32Window(mGlfwWnd);

	auto primaryMonitor = glfwGetPrimaryMonitor();
	const auto videoMode = glfwGetVideoMode(primaryMonitor);

	int clientPosX = (videoMode->width - InitClientWidth) >> 1;
	int clientPosY = (videoMode->height - InitClientHeight) >> 1;

	glfwSetWindowPos(mGlfwWnd, clientPosX, clientPosY);

	glfwSetWindowUserPointer(mGlfwWnd, this);
		
	glfwSetKeyCallback(mGlfwWnd, GLFWKeyCallback);
	glfwSetFramebufferSizeCallback(mGlfwWnd, GLFWResizeCallback);
	glfwSetWindowFocusCallback(mGlfwWnd, GLFWFocusCallback);
	//glfwSetWindowPosCallback(mGlfwWnd, GLFWWindowPosCallback);
#endif

	return true;
}

void GameWorld::OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN) {
		switch (wParam) {
		case VK_ESCAPE:	PostQuitMessage(0); return;
		case VK_T: {
			if (mGameState == EGameStates::EGS_Play) {
				mGameState = EGameStates::EGS_UI;
				mInputManager->SetMouseRelative(false);
				mInputManager->SetCursorVisibility(true);
				mRenderer->ShowImGui(true);
			}
			else {
				mGameState = EGameStates::EGS_Play;
				mInputManager->SetMouseRelative(true);
				mInputManager->SetCursorVisibility(false);
				mInputManager->IgnoreMouseInput();
				mRenderer->ShowImGui(false);
			}
			return;
		}
		case VK_UP:
			mTimeSlowDown = std::min(1.0f, mTimeSlowDown + 0.1f);
			return;
		case VK_DOWN:
			mTimeSlowDown = std::max(0.0f, mTimeSlowDown - 0.1f);
			return;
		case VK_SPACE: {
			auto state = mRenderer->RaytracingEnabled();
			mRenderer->EnableRaytracing(!state);
			return;
		}
		case VK_1: {
			auto state = mRenderer->TaaEnabled();
			mRenderer->EnableTaa(!state);
			return;
		}
		case VK_2: {
			auto state = mRenderer->DepthOfFieldEnabled();
			mRenderer->EnableDepthOfField(!state);
			return;
		}
		}
	}
}

bool GameWorld::ProcessInput() {
	mInputManager->Update();
	if (mGameState == EGameStates::EGS_Play) CheckReturn(mActorManager->ProcessInput(mInputManager->GetInputState()));

	return true;
}

bool GameWorld::Update() {
	float dt = mTimer->DeltaTime();
	CheckReturn(mActorManager->Update(dt * mTimeSlowDown));
	CheckReturn(mRenderer->Update(dt * mTimeSlowDown));

	return true;
}

bool GameWorld::Draw() {
	CheckReturn(mRenderer->Draw());

	return true;
}

bool GameWorld::LoadData() {
	CheckReturn(mRenderer->SetEquirectangularMap("./../../assets/textures/brown_photostudio.dds"));

	XMFLOAT4 rot;
	XMStoreFloat4(&rot, XMQuaternionRotationAxis(UnitVectors::UpVector, XM_PI));

	new FreeLookActor("free_look_actor", DirectX::XMFLOAT3(0.0f, 0.0f, -5.0f));
	new RotatingMonkey("monkey_1", XM_PIDIV2, DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f), rot);
	new RotatingMonkey("monkey_2", 2.0f * XM_2PI, DirectX::XMFLOAT3(-2.0f, -1.0f, -0.5f), rot);
	new RotatingMonkey("monkey_3", XM_PI, DirectX::XMFLOAT3(2.5f, -1.0f, -1.0f), rot);
	new RotatingMonkey("monkey_4", XM_PIDIV4, DirectX::XMFLOAT3(0.0f, 0.0f, -20.0f), rot);
	new PlaneActor("plane_actor", DirectX::XMFLOAT3(0.0f, -2.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(10.0f, 1.0f, 10.0f));
	new SphereActor("sphere_actor", DirectX::XMFLOAT3(0.0f, 0.0f, -2.0f));

	return true;
}