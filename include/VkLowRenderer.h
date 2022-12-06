#pragma once

#include "Renderer.h"

class VkLowRenderer{
public:
	VkLowRenderer();
	virtual ~VkLowRenderer();

public:
	bool LowInitialize(GLFWwindow* glfwWnd, UINT width, UINT height);
	void LowCleanUp();

protected:
	virtual bool RecreateSwapChain();
	virtual void CleanUpSwapChain();

	__forceinline constexpr VkSampleCountFlagBits GetMSAASamples() const;

private:
	bool CreateInstance();
	bool CreateSurface();
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapChain();
	bool CreateImageViews();

protected:
	static const std::uint32_t SwapChainImageCount = 2;

	VkInstance mInstance;
	VkSurfaceKHR mSurface;
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mDevice;
	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;
	VkSwapchainKHR mSwapChain;
	std::vector<VkImage> mSwapChainImages;
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;
	std::vector<VkImageView> mSwapChainImageViews;

private:
	bool bIsCleanedUp;

	GLFWwindow* mGlfwWnd;

	VkDebugUtilsMessengerEXT mDebugMessenger;

	VkSampleCountFlagBits mMSAASamples;
};

#include "VkLowRenderer.inl"