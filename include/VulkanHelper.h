#pragma once

#include <vector>
#include <optional>
#include <string>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct QueueFamilyIndices {
	std::optional<std::uint32_t> GraphicsFamily;
	std::optional<std::uint32_t> PresentFamily;

	std::uint32_t GetGraphicsFamilyIndex();
	std::uint32_t GetPresentFamilyIndex();
	bool IsComplete();

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

	bool CheckValidationLayersSupport();

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

	bool SetUpDebugMessenger(const VkInstance& inInstance, VkDebugUtilsMessengerEXT& inDebugMessenger);

	bool CheckDeviceExtensionsSupport(const VkPhysicalDevice& inPhysicalDevice);

	SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	bool IsDeviceSuitable(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	int RateDeviceSuitability(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

	VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice inPhysicalDevice);

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats);

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentMods);

	VkExtent2D ChooseSwapExtent(GLFWwindow* pWnd, const VkSurfaceCapabilitiesKHR& inCapabilities);
	
	VkCommandBuffer BeginSingleTimeCommands(const VkDevice& inDevice, const VkCommandPool& inCommandPool);

	void EndSingleTimeCommands(const VkDevice& inDevice, const VkQueue& inQueue, const VkCommandPool& inCommandPool, VkCommandBuffer& ioCommandBuffer);

	std::uint32_t FindMemoryType(const VkPhysicalDevice& inPhysicalDevice, std::uint32_t inTypeFilter, const VkMemoryPropertyFlags& inProperties);

	bool CreateBuffer(
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
		std::uint32_t inWidth,
		std::uint32_t inHeight);

	bool GenerateMipmaps(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		std::int32_t inTexWidth,
		std::int32_t inTexHeight,
		std::uint32_t inMipLevles);

	bool CreateImage(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		std::uint32_t inWidth,
		std::uint32_t inHeight,
		std::uint32_t inMipLevels,
		const VkSampleCountFlagBits& inNumSamples,
		const VkFormat& inFormat,
		const VkImageTiling& inTiling,
		const VkImageUsageFlags& inUsage,
		const VkMemoryPropertyFlags& inProperties,
		VkImage& outImage,
		VkDeviceMemory& outImageMemory);

	bool CreateImageView(
		const VkDevice& inDevice,
		const VkImage& inImage,
		const VkFormat& inFormat,
		std::uint32_t inMipLevles,
		const VkImageAspectFlags& inAspectFlags,
		VkImageView& outImageView);

	VkFormat FindSupportedFormat(
		const VkPhysicalDevice& inPhysicalDevice,
		const std::vector<VkFormat>& inCandidates,
		const VkImageTiling& inTiling,
		const VkFormatFeatureFlags inFeatures);

	VkFormat FindDepthFormat(const VkPhysicalDevice& inPhysicalDevice);

	bool HasStencilComponent(const VkFormat& inFormat);

	bool TransitionImageLayout(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		const VkImageLayout& inOldLayout,
		const VkImageLayout& inNewLayout,
		std::uint32_t inMipLevels);

	bool ReadFile(const std::string& inFilePath, std::vector<char>& outData);

	bool CreateShaderModule(const VkDevice& inDevice, const std::vector<char>& inCode, VkShaderModule& outModule);
}