#pragma once

#include <Windows.h>
#include <wrl.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <array>
#include <algorithm>

//
// Link necessary libraries.
//
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "vulkan-1.lib")

//
// DirectX header files.
//
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <D3Dcompiler.h>
#include <dxgi1_6.h>
#include "d3dx12.h"
#include <DirectXCollision.h>
//

//
// Vulkan header files.
//
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
//

#include "Transform.h"
#include "Mesh.h"
#include "Camera.h"

enum ERenderTypes {
	EOpaque = 0,
	EBlend,
	ESky,
	ENumTypes,
};

class Renderer {
public:
	virtual bool Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) = 0;
	virtual void CleanUp() = 0;

	virtual bool Update(float delta) = 0;
	virtual bool Draw() = 0;

	virtual bool OnResize(UINT width, UINT height) = 0;

	virtual void* AddModel(const std::string& file, const Transform& trans, ERenderTypes type = ERenderTypes::EOpaque) = 0;
	virtual void RemoveModel(void* model) = 0;
	virtual void UpdateModel(void* model, const Transform& trans) = 0;

	virtual void SetModelVisibility(void* model, bool visible) = 0;

	void SetCamera(Camera* cam);

	void EnableDebugging(bool state);
	__forceinline constexpr bool DebuggingEnabled() const;

	void EnableShadow(bool state);
	__forceinline constexpr bool ShadowEnabled() const;

	void EnableSsao(bool state);
	__forceinline constexpr bool SsaoEnabled() const;

	__forceinline constexpr bool IsInitialized() const;
	__forceinline constexpr float AspectRatio() const;

protected:
	bool bInitialized = false;

	UINT mClientWidth;
	UINT mClientHeight;

	Camera* mCamera;

	bool bDebuggingEnabled = true;
	bool bShadowEnabled = true;
	bool bSsaoEnabled = true;
};

constexpr bool Renderer::DebuggingEnabled() const {
	return bDebuggingEnabled;
}

constexpr bool Renderer::ShadowEnabled() const {
	return bShadowEnabled;
}

constexpr bool Renderer::SsaoEnabled() const {
	return bSsaoEnabled;
}

constexpr bool Renderer::IsInitialized() const {
	return bInitialized;
}

constexpr float Renderer::AspectRatio() const {
	return static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight);
}