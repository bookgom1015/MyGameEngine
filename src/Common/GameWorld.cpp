#include "Common/GameWorld.h"
#include "Common/Debug/Logger.h"
#include "Common/GameTimer.h"
#include "Common/Input/InputManager.h"
#include "Common/Actor/ActorManager.h"
#include "Common/Camera/Camera.h"

#include "Prefab/FreeLookActor.h"
#include "Prefab/SphereActor.h"
#include "Prefab/PlaneActor.h"
#include "Prefab/RotatingMonkey.h"
#include "Prefab/CastleActor.h"
#include "Prefab/BoxActor.h"

#ifdef _DirectX
#include "DirectX/Render/DxRenderer.h"
#else
#include "Vulkan/Render/VkRenderer.h"
#endif

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <exception>

#undef min
#undef max

using namespace DirectX;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, INT showCmd) {
	try {
		GameWorld game;

		if (!game.Initialize()) return -1;
		if (!game.RunLoop()) {
			WLogln(L"Error occured");
			return -1;
		}
		game.CleanUp();

		WLogln(L"The game has been successfully finished");
		return 0;
	}
	catch (const std::exception& e) {
		Logln("Exception catched: ", e.what());
		return -1;
	}
}

namespace {
	const UINT InitClientWidth = 1280;
	const UINT InitClientHeight = 720;

#ifdef _DirectX
	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid
		return GameWorld::GetWorld()->MsgProc(hwnd, msg, wParam, lParam);
	}
#else
	void GLFWKeyCallback(GLFWwindow* wnd, INT key, INT code, INT action, INT mods) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(wnd));
		game->MsgProc(wnd, key, code, action, mods);
	}

	void GLFWResizeCallback(GLFWwindow* wnd, INT width, INT height) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(wnd));
		game->OnResize(width, height);
	}

	void GLFWFocusCallback(GLFWwindow* pWnd, INT inState) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		game->OnFocusChanged(inState);
	}
#endif
}

GameWorld* GameWorld::sGameWorld = nullptr;

GameWorld::GameWorld() {
	sGameWorld = this;

	mTimer = std::make_unique<GameTimer>();
	mInputManager = std::make_unique<InputManager>();
#ifdef _DirectX
	mRenderer = std::make_unique<DxRenderer>();
#else
	mRenderer = std::make_unique<VkRenderer>();
#endif
	mActorManager = std::make_unique<ActorManager>();
}

GameWorld::~GameWorld() {
	if (!bIsCleanedUp) CleanUp();
}

BOOL GameWorld::Initialize() {
	Logger::LogHelper::StaticInit();

	CheckReturn(InitMainWindow());
	CheckReturn(mInputManager->Initialize(mhMainWnd));
	CheckReturn(mRenderer->Initialize(mhMainWnd, mGlfwWnd, InitClientWidth, InitClientHeight));

	mInputManager->SetMouseRelative(TRUE);
	mInputManager->SetCursorVisibility(FALSE);

	mTimer->SetLimitFrameRate(GameTimer::E_LimitFrameRate60f);

	return TRUE;
}

BOOL GameWorld::RunLoop() {
	MSG msg = { 0 };

	FLOAT beginTime = 0.f;
	FLOAT endTime = 0.f;
	FLOAT elapsedTime = 0.f;

	mTimer->Reset();

	if (!LoadData()) return FALSE;

#ifdef _DirectX
	CheckReturn(PrepareUpdate());

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
	CheckReturn(PrepareUpdate());

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

	return TRUE;
}

void GameWorld::CleanUp() {
	if (mInputManager != nullptr) mInputManager->CleanUp();
	if (mRenderer != nullptr) mRenderer->CleanUp();

#ifdef _Vulkan
	glfwDestroyWindow(mGlfwWnd);
	glfwTerminate();
#endif

	bIsCleanedUp = TRUE;
}

GameWorld* GameWorld::GetWorld() { return sGameWorld; }

Renderer* GameWorld::GetRenderer() const { return mRenderer.get(); }

ActorManager* GameWorld::GetActorManager() const { return mActorManager.get(); }

#ifdef _DirectX
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
			bAppPaused = TRUE;
		}
		else {
			mInputManager->IgnoreMouseInput();
			bAppPaused = FALSE;
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
				bAppPaused = TRUE;
				bMinimized = TRUE;
				bMaximized = FALSE;
			}
			else if (wParam == SIZE_MAXIMIZED) {
				bAppPaused = FALSE;
				bMinimized = FALSE;
				bMaximized = TRUE;
				OnResize(width, height);
			}
			else if (wParam == SIZE_RESTORED) {
				// Restoring from minimized state?
				if (bMinimized) {
					bAppPaused = FALSE;
					bMinimized = FALSE;
					OnResize(width, height);
				}

				// Restoring from maximized state?
				else if (bMaximized) {
					bAppPaused = FALSE;
					bMaximized = FALSE;
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
		bAppPaused = TRUE;
		bResizing = TRUE;
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mTimer->Start();
		bAppPaused = FALSE;
		bResizing = FALSE;
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
#else
LRESULT GameWorld::MsgProc(GLFWwindow* wnd, INT key, INT code, INT action, INT mods) {
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS) {
			glfwSetWindowShouldClose(mGlfwWnd, GLFW_TRUE);
		}
		return 0;
	}

	return 0;
}
#endif

void GameWorld::OnResize(UINT width, UINT height) {
	if (!mRenderer->OnResize(width, height)) {
		PostQuitMessage(0);
	}
}

#ifdef _Vulkan
void GameWorld::OnFocusChanged(INT focused) {
	bAppPaused = (focused == GLFW_TRUE ? FALSE : TRUE);
	mInputManager->IgnoreMouseInput();
}
#endif

BOOL GameWorld::InitMainWindow() {
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
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, FALSE);
	INT width = R.right - R.left;
	INT height = R.bottom - R.top;

	INT outputWidth = GetSystemMetrics(SM_CXSCREEN);
	INT outputHeight = GetSystemMetrics(SM_CYSCREEN);

	INT clientPosX = static_cast<INT >((outputWidth - InitClientWidth) * 0.5f);
	INT clientPosY = static_cast<INT >((outputHeight - InitClientHeight) * 0.5f);

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

	INT clientPosX = (videoMode->width - InitClientWidth) >> 1;
	INT clientPosY = (videoMode->height - InitClientHeight) >> 1;

	glfwSetWindowPos(mGlfwWnd, clientPosX, clientPosY);

	glfwSetWindowUserPointer(mGlfwWnd, this);
		
	glfwSetKeyCallback(mGlfwWnd, GLFWKeyCallback);
	glfwSetFramebufferSizeCallback(mGlfwWnd, GLFWResizeCallback);
	glfwSetWindowFocusCallback(mGlfwWnd, GLFWFocusCallback);
	//glfwSetWindowPosCallback(mGlfwWnd, GLFWWindowPosCallback);
#endif

	return TRUE;
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
			mTimeSlowDown = std::min(1.f, mTimeSlowDown + 0.1f);
			return;
		case VK_DOWN:
			mTimeSlowDown = std::max(0.f, mTimeSlowDown - 0.1f);
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
		case VK_3: {
			auto state = mRenderer->BloomEnabled();
			mRenderer->EnableBloom(!state);
			return;
		}
		}
	}
}

BOOL GameWorld::ProcessInput() {
	mInputManager->Update();
	if (mGameState == EGameStates::EGS_Play) CheckReturn(mActorManager->ProcessInput(mInputManager->GetInputState()));

	return TRUE;
}

BOOL GameWorld::PrepareUpdate() {
	CheckReturn(mRenderer->PrepareUpdate());

	return TRUE;
}

BOOL GameWorld::Update() {
	FLOAT dt = mTimer->DeltaTime();
	CheckReturn(mActorManager->Update(dt * mTimeSlowDown));
	CheckReturn(mRenderer->Update(dt * mTimeSlowDown));

	return TRUE;
}

BOOL GameWorld::Draw() {
	CheckReturn(mRenderer->Draw());

	return TRUE;
}

BOOL GameWorld::LoadData() {
	CheckReturn(mRenderer->SetEquirectangularMap("./../../assets/textures/forest_hdr.dds"));

	XMFLOAT4 rot;
	XMStoreFloat4(&rot, XMQuaternionRotationAxis(UnitVector::UpVector, XM_PI));

	new FreeLookActor("free_look_actor", DirectX::XMFLOAT3(0.f, 0.f, -5.f));
	new RotatingMonkey("monkey_L1", XM_PIDIV2, DirectX::XMFLOAT3(0.f, 0.5f, 0.5f), rot);
	new RotatingMonkey("monkey_L2", 2.f * XM_2PI, DirectX::XMFLOAT3(-2.f, -1.f, -0.5f), rot);
	new RotatingMonkey("monkey_L3", XM_PI, DirectX::XMFLOAT3(2.5f, -1.f, -1.f), rot);
	new RotatingMonkey("monkey_L4", XM_PIDIV4, DirectX::XMFLOAT3(0.f, 0.f, -20.f), rot);
	new RotatingMonkey("monkey_U1", XM_PIDIV2, DirectX::XMFLOAT3(0.f, 6.5f, 0.5f), rot);
	new RotatingMonkey("monkey_U2", 2.f * XM_2PI, DirectX::XMFLOAT3(-2.f, 5.f, -0.5f), rot);
	new RotatingMonkey("monkey_U3", XM_PI, DirectX::XMFLOAT3(2.5f, 5.f, -1.f), rot);
	new PlaneActor("plane_actor", DirectX::XMFLOAT3(0.f, -2.f, 0.f), DirectX::XMFLOAT4(0.f, 0.f, 0.f, 1.f), DirectX::XMFLOAT3(30.f, 1.f, 30.f));
	new SphereActor("sphere_actor", DirectX::XMFLOAT3(0.f, 0.f, -2.f));
	new BoxActor("box_actor", DirectX::XMFLOAT3(0.f, 9.f, 0.f), DirectX::XMFLOAT4(0.f, 0.f, 0.f, 1.f), DirectX::XMFLOAT3(6.f, 6.f, 6.f));

	return TRUE;
}