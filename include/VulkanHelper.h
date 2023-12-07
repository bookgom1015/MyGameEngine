#pragma once

#include <vector>
#include <optional>
#include <string>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct QueueFamilyIndices {
	std::optional<UINT> GraphicsFamily;
	std::optional<UINT> PresentFamily;

	UINT GetGraphicsFamilyIndex();
	UINT GetPresentFamilyIndex();
	BOOL IsComplete();

	static QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR Capabilities;
	std::vector<VkSurfaceFormatKHR> Formats;
	std::vector<VkPresentModeKHR> PresentModes;
};

namespace VulkanHelper {
	static const std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	static const std::vector<const char*> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	BOOL CheckValidationLayersSupport();

	void GetRequiredExtensions(std::vector<const char*>& outExtensions);

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	VkResult CreateDebugUtilsMessengerEXT(
		const VkInstance& instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(
		const VkInstance& instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator);

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo);

	BOOL SetUpDebugMessenger(const VkInstance& inInstance, VkDebugUtilsMessengerEXT& inDebugMessenger);

	BOOL CheckDeviceExtensionsSupport(const VkPhysicalDevice& inPhysicalDevice);

	SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	BOOL IsDeviceSuitable(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	INT RateDeviceSuitability(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice inPhysicalDevice);

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats);

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentMods);

	VkExtent2D ChooseSwapExtent(GLFWwindow* pWnd, const VkSurfaceCapabilitiesKHR& inCapabilities);
	
	VkCommandBuffer BeginSingleTimeCommands(const VkDevice& inDevice, const VkCommandPool& inCommandPool);

	void EndSingleTimeCommands(const VkDevice& inDevice, const VkQueue& inQueue, const VkCommandPool& inCommandPool, VkCommandBuffer& ioCommandBuffer);

	UINT FindMemoryType(const VkPhysicalDevice& inPhysicalDevice, UINT inTypeFilter, const VkMemoryPropertyFlags& inProperties);

	BOOL CreateBuffer(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		VkDeviceSize inSize,
		const VkBufferUsageFlags& inUsage,
		const VkMemoryPropertyFlags& inProperties,
		VkBuffer& outBuffer,
		VkDeviceMemory& outBufferMemory);

	void CopyBuffer(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkBuffer& inSrcBuffer,
		const VkBuffer& inDstBuffer,
		VkDeviceSize inSize);

	void CopyBufferToImage(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkBuffer& inBuffer,
		const VkImage& inImage,
		UINT inWidth,
		UINT inHeight);

	BOOL GenerateMipmaps(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		INT inTexWidth,
		INT inTexHeight,
		UINT inMipLevles); 

	BOOL CreateImage(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		UINT inWidth,
		UINT inHeight,
		UINT inMipLevels,
		const VkSampleCountFlagBits& inNumSamples,
		const VkFormat& inFormat,
		const VkImageTiling& inTiling,
		const VkImageUsageFlags& inUsage,
		const VkMemoryPropertyFlags& inProperties,
		VkImage& outImage,
		VkDeviceMemory& outImageMemory);

	BOOL CreateImageView(
		const VkDevice& inDevice,
		const VkImage& inImage,
		const VkFormat& inFormat,
		UINT inMipLevles,
		const VkImageAspectFlags& inAspectFlags,
		VkImageView& outImageView);

	VkFormat FindSupportedFormat(
		const VkPhysicalDevice& inPhysicalDevice,
		const std::vector<VkFormat>& inCandidates,
		const VkImageTiling& inTiling,
		const VkFormatFeatureFlags inFeatures);

	VkFormat FindDepthFormat(const VkPhysicalDevice& inPhysicalDevice);

	BOOL HasStencilComponent(const VkFormat& inFormat);

	BOOL TransitionImageLayout(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		const VkImageLayout& inOldLayout,
		const VkImageLayout& inNewLayout,
		UINT inMipLevels);

	BOOL ReadFile(const std::string& inFilePath, std::vector<char>& outData);

	BOOL CreateShaderModule(const VkDevice& inDevice, const std::vector<char>& inCode, VkShaderModule& outModule);
}