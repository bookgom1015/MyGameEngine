#pragma once

//
// Common header files.
//
#include <algorithm>
#include <array>
#include <DirectXCollision.h>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <Windows.h>
#include <wrl.h>

#include "Camera.h"
#include "Transform.h"
#include "Mesh.h"
#include "RenderType.h"

//
// Link necessary libraries.
//
#ifdef _DirectX
	#pragma comment(lib, "dxguid.lib")
	#pragma comment(lib, "dxgi.lib")
	#pragma comment(lib, "d3d12.lib")
	#pragma comment(lib, "d3dcompiler.lib")
	#pragma comment(lib, "dxcompiler.lib")
	#pragma comment(lib, "DirectXTK12.lib")
#else 
	#pragma comment(lib, "glfw3.lib")
	#pragma comment(lib, "vulkan-1.lib")
#endif

#ifdef _DirectX
	//
	// DirectX header files.
	//
	#include <d3d12.h>
	#include <d3d12sdklayers.h>
	#include <D3Dcompiler.h>
	#include <dxgi1_6.h>
	#include <GraphicsMemory.h>
	#include <DDSTextureLoader.h>
	#include <ResourceUploadBatch.h>
	#include "d3dx12.h"
#else 
	//
	// Vulkan header files.
	//
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_INCLUDE_VULKAN
	#include <GLFW/glfw3.h>
	
	#define GLFW_EXPOSE_NATIVE_WIN32
	#include <GLFW/glfw3native.h>
#endif

class Renderer {
public:
	virtual BOOL Initialize(HWND hwnd, void* glfwWnd, UINT width, UINT height) = 0;
	virtual void CleanUp() = 0;

	virtual BOOL PrepareUpdate() = 0;
	virtual BOOL Update(FLOAT delta) = 0;
	virtual BOOL Draw() = 0;

	virtual BOOL OnResize(UINT width, UINT height) = 0;

	virtual void* AddModel(const std::string& file, const Transform& trans, RenderType::Type type = RenderType::E_Opaque) = 0;
	virtual void RemoveModel(void* model) = 0;
	virtual void UpdateModel(void* model, const Transform& trans) = 0;
	virtual void SetModelVisibility(void* model, BOOL visible) = 0;
	virtual void SetModelPickable(void* model, BOOL pickable) = 0;

	virtual BOOL SetCubeMap(const std::string& file) = 0;
	virtual BOOL SetEquirectangularMap(const std::string& file) = 0;

	virtual void Pick(FLOAT x, FLOAT y);

	void SetCamera(Camera* cam);

	void EnableDebugging(BOOL state);
	__forceinline constexpr BOOL DebuggingEnabled() const;

	void EnableShadow(BOOL state);
	__forceinline constexpr BOOL ShadowEnabled() const;

	void EnableSsao(BOOL state);
	__forceinline constexpr BOOL SsaoEnabled() const;

	void EnableTaa(BOOL state);
	__forceinline constexpr BOOL TaaEnabled() const;

	void EnableMotionBlur(BOOL state);
	__forceinline constexpr BOOL MotionBlurEnabled() const;

	void EnableDepthOfField(BOOL state);
	__forceinline constexpr BOOL DepthOfFieldEnabled() const;

	void EnableBloom(BOOL state);
	__forceinline constexpr BOOL BloomEnabled() const;

	void EnableSsr(BOOL state);
	__forceinline constexpr BOOL SsrEnabled() const;

	void EnableGammaCorrection(BOOL state);
	__forceinline constexpr BOOL GammaCorrectionEnabled() const;

	void EnableToneMapping(BOOL state);
	__forceinline constexpr BOOL ToneMappingEnabled() const;

	void EnablePixelation(BOOL state);
	__forceinline constexpr BOOL PixelationEnabled() const;

	void EnableSharpen(BOOL state);
	__forceinline constexpr BOOL SharpenEnabled() const;

	void EnableRaytracing(BOOL state);
	__forceinline constexpr BOOL RaytracingEnabled() const;

	void ShowImGui(BOOL state);

	__forceinline constexpr BOOL IsInitialized() const;
	__forceinline constexpr FLOAT AspectRatio() const;

protected:
	BOOL bInitialized = FALSE;

	UINT mClientWidth;
	UINT mClientHeight;

	Camera* mCamera;

	BOOL bShowImGui = FALSE;

	BOOL bDebuggingEnabled = FALSE;
	BOOL bShadowEnabled = TRUE;
	BOOL bSsaoEnabled = TRUE;
	BOOL bTaaEnabled = TRUE;
	BOOL bInitiatingTaa = TRUE;
	BOOL bMotionBlurEnabled = TRUE;
	BOOL bDepthOfFieldEnabled = TRUE;
	BOOL bBloomEnabled = TRUE;
	BOOL bSsrEnabled = TRUE;
	BOOL bGammaCorrectionEnabled = TRUE;
	BOOL bToneMappingEnabled = TRUE;
	BOOL bPixelationEnabled = FALSE;
	BOOL bSharpenEnabled = FALSE;

	BOOL bRaytracing = FALSE;
};

#include "Renderer.inl"