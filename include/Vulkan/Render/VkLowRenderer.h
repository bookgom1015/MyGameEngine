#pragma once

#include "Common/Render/Renderer.h"

class VkLowRenderer{
public:
	VkLowRenderer();
	virtual ~VkLowRenderer();

public:
	BOOL LowInitialize(GLFWwindow* const glfwWnd, UINT width, UINT height);
	void LowCleanUp();

protected:
	virtual BOOL RecreateSwapChain();
	virtual void CleanUpSwapChain();

	__forceinline constexpr VkSampleCountFlagBits GetMSAASamples() const;

private:
	BOOL CreateInstance();
	BOOL CreateSurface();
	BOOL SelectPhysicalDevice();
	BOOL CreateLogicalDevice();
	BOOL CreateSwapChain();
	BOOL CreateImageViews();

protected:
	static const UINT SwapChainImageCount = 2;

	VkInstance mInstance;

	VkSurfaceKHR mSurface;

	VkPhysicalDevice mPhysicalDevice;
	VkDevice mDevice;

	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	VkSwapchainKHR mSwapChain;
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;

	std::array<VkImage, SwapChainImageCount> mSwapChainImages;
	std::array<VkImageView, SwapChainImageCount> mSwapChainImageViews;

private:
	BOOL bIsCleanedUp;

	GLFWwindow* mGlfwWnd;

	VkDebugUtilsMessengerEXT mDebugMessenger;

	VkSampleCountFlagBits mMSAASamples;
};

#include "VkLowRenderer.inl"