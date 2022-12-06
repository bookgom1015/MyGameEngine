#include "VulkanHelper.h"
#include "Logger.h"

#include <set>
#include <algorithm>
#include <fstream>

#undef min
#undef max

std::uint32_t QueueFamilyIndices::GetGraphicsFamilyIndex() { return GraphicsFamily.value(); }

std::uint32_t QueueFamilyIndices::GetPresentFamilyIndex() { return PresentFamily.value(); }

bool QueueFamilyIndices::IsComplete() { return GraphicsFamily.has_value() && PresentFamily.has_value(); }

QueueFamilyIndices QueueFamilyIndices::FindQueueFamilies(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
	QueueFamilyIndices indices = {};

	std::uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(inPhysicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(inPhysicalDevice, &queueFamilyCount, queueFamilies.data());

	std::uint32_t i = 0;
	for (const auto& queueFamiliy : queueFamilies) {
		if (queueFamiliy.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.GraphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(inPhysicalDevice, i, inSurface, &presentSupport);

		if (presentSupport) indices.PresentFamily = i;
		if (indices.IsComplete()) break;

		++i;
	}

	return indices;
}


bool VulkanHelper::CheckValidationLayersSupport() {
	std::uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

#ifdef _DEBUG
	WLogln(L"Available Layers:");
	for (const auto& layer : availableLayers) {
		Logln("\t ", layer.layerName);
	}
#endif

	std::vector<const char*> unsupportedLayers;
	bool status = true;

	for (auto layerName : ValidationLayers) {
		bool layerFound = false;

		for (const auto& layer : availableLayers) {
			if (std::strcmp(layerName, layer.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			unsupportedLayers.push_back(layerName);
			status = false;
		}
	}

	if (!status) {
		std::wstringstream wsstream;

		for (const auto& layerName : unsupportedLayers)
			wsstream << layerName << L' ';
		wsstream << L"is/are not supported";

		ReturnFalse(wsstream.str());
	}

	return true;
}

void VulkanHelper::GetRequiredExtensions(std::vector<const char*>& outExtensions) {
	std::uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

#ifdef _DEBUG
	WLogln(L"Required extensions by GLFW:");
	for (std::uint32_t i = 0; i < glfwExtensionCount; ++i) {
		Logln("\t ", glfwExtensions[i]);
	}
#endif

	outExtensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef _DEBUG
	outExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanHelper::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {
	Logln(pCallbackData->pMessage);

	return VK_FALSE;
}

VkResult VulkanHelper::CreateDebugUtilsMessengerEXT(
		const VkInstance& instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanHelper::DestroyDebugUtilsMessengerEXT(
		const VkInstance& instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

void VulkanHelper::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo) {
	inCreateInfo = {};
	inCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	inCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	inCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	inCreateInfo.pfnUserCallback = DebugCallback;
	inCreateInfo.pUserData = nullptr;
}

bool VulkanHelper::SetUpDebugMessenger(const VkInstance& inInstance, VkDebugUtilsMessengerEXT& inDebugMessenger) {
#ifndef _DEBUG
	return true;
#endif

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(inInstance, &createInfo, nullptr, &inDebugMessenger) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create debug messenger");
	}

	return true;
}

bool VulkanHelper::CheckDeviceExtensionsSupport(const VkPhysicalDevice& inPhysicalDevice) {
	std::uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(inPhysicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(inPhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

SwapChainSupportDetails VulkanHelper::QuerySwapChainSupport(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
	SwapChainSupportDetails details = {};

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(inPhysicalDevice, inSurface, &details.Capabilities);

	std::uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(inPhysicalDevice, inSurface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(inPhysicalDevice, inSurface, &formatCount, details.Formats.data());
	}

	std::uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(inPhysicalDevice, inSurface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.PresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(inPhysicalDevice, inSurface, &presentModeCount, details.PresentModes.data());
	}

	return details;
}

bool VulkanHelper::IsDeviceSuitable(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
	QueueFamilyIndices indices = QueueFamilyIndices::FindQueueFamilies(inPhysicalDevice, inSurface);

	bool extensionSupported = CheckDeviceExtensionsSupport(inPhysicalDevice);

	bool swapChainAdequate = false;
	if (extensionSupported) {
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(inPhysicalDevice, inSurface);

		swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(inPhysicalDevice, &supportedFeatures);

	return indices.IsComplete() && extensionSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

int VulkanHelper::RateDeviceSuitability(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
	int score = 0;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(inPhysicalDevice, &deviceProperties);

	WLogln(L"Physical device properties:");
	Logln("\t Device name: ", deviceProperties.deviceName);
	WLog(L"\t Device type: ");
	switch (deviceProperties.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		WLogln(L"Other type");
		score += 1000;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		WLogln(L"Integrated GPU");
		score += 500;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		WLogln(L"Discrete GPU");
		score += 250;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		WLogln(L"Virtual GPU");
		score += 100;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		WLogln(L"CPU type");
		score += 50;
		break;
	}
	{
		std::uint32_t variant = VK_API_VERSION_VARIANT(deviceProperties.driverVersion);
		std::uint32_t major = VK_API_VERSION_MAJOR(deviceProperties.driverVersion);
		std::uint32_t minor = VK_API_VERSION_MINOR(deviceProperties.driverVersion);
		std::uint32_t patch = VK_API_VERSION_PATCH(deviceProperties.driverVersion);
		WLogln(L"\t Device version: ",
			std::to_wstring(variant), L".",
			std::to_wstring(major), L".",
			std::to_wstring(minor), L".",
			std::to_wstring(patch));
	}
	{
		std::uint32_t variant = VK_API_VERSION_VARIANT(deviceProperties.apiVersion);
		std::uint32_t major = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
		std::uint32_t minor = VK_API_VERSION_MINOR(deviceProperties.apiVersion);
		std::uint32_t patch = VK_API_VERSION_PATCH(deviceProperties.apiVersion);
		WLogln(L"\t API version: ",
			std::to_wstring(variant), L".",
			std::to_wstring(major), L".",
			std::to_wstring(minor), L".",
			std::to_wstring(patch));
	}

	score += deviceProperties.limits.maxImageDimension2D;

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(inPhysicalDevice, &deviceFeatures);

	if (!deviceFeatures.geometryShader || !IsDeviceSuitable(inPhysicalDevice, inSurface)) {
		WLogln(L"\t This device is not suitable");
		return 0;
	}

	return score;
}

VkSampleCountFlagBits VulkanHelper::GetMaxUsableSampleCount(VkPhysicalDevice inPhysicalDevice) {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(inPhysicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

	return VK_SAMPLE_COUNT_1_BIT;
}

VkSurfaceFormatKHR VulkanHelper::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats) {
	for (const auto& availableFormat : inAvailableFormats) {
		if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return inAvailableFormats[0];
}

VkPresentModeKHR VulkanHelper::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentMods) {
	for (const auto& availablePresentMode : inAvailablePresentMods) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanHelper::ChooseSwapExtent(GLFWwindow* pWnd, const VkSurfaceCapabilitiesKHR& inCapabilities) {
	if (inCapabilities.currentExtent.width != UINT32_MAX) {
		return inCapabilities.currentExtent;
	}

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(pWnd, &width, &height);

	VkExtent2D actualExtent = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };

	actualExtent.width = std::max(inCapabilities.minImageExtent.width, std::min(inCapabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(inCapabilities.minImageExtent.height, std::min(inCapabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

VkCommandBuffer VulkanHelper::BeginSingleTimeCommands(const VkDevice& inDevice, const VkCommandPool& inCommandPool) {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = inCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(inDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanHelper::EndSingleTimeCommands(const VkDevice& inDevice, const VkQueue& inQueue, const VkCommandPool& inCommandPool, VkCommandBuffer& ioCommandBuffer) {
	vkEndCommandBuffer(ioCommandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &ioCommandBuffer;

	vkQueueSubmit(inQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(inQueue);

	vkFreeCommandBuffers(inDevice, inCommandPool, 1, &ioCommandBuffer);
}

std::uint32_t VulkanHelper::FindMemoryType(const VkPhysicalDevice& inPhysicalDevice, std::uint32_t inTypeFilter, const VkMemoryPropertyFlags& inProperties) {
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(inPhysicalDevice, &memProperties);

	for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		if ((inTypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & inProperties) == inProperties) {
			index = i;
			break;
		}
	}

	return index;
}

bool VulkanHelper::CreateBuffer(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		VkDeviceSize inSize,
		const VkBufferUsageFlags& inUsage,
		const VkMemoryPropertyFlags& inProperties,
		VkBuffer& outBuffer,
		VkDeviceMemory& outBufferMemory) {
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = inSize;
	bufferInfo.usage = inUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.flags = 0;

	if (vkCreateBuffer(inDevice, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create vertex buffer");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(inDevice, outBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(inPhysicalDevice, memRequirements.memoryTypeBits, inProperties);

	if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &outBufferMemory) != VK_SUCCESS) {
		ReturnFalse(L"Failed to allocate vertex buffer memory");
	}

	vkBindBufferMemory(inDevice, outBuffer, outBufferMemory, 0);

	return true;
}

void VulkanHelper::CopyBuffer(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkBuffer& inSrcBuffer,
		const VkBuffer& inDstBuffer,
		VkDeviceSize inSize) {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = inSize;
	vkCmdCopyBuffer(commandBuffer, inSrcBuffer, inDstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
}

void VulkanHelper::CopyBufferToImage(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkBuffer& inBuffer,
		const VkImage& inImage,
		std::uint32_t inWidth,
		std::uint32_t inHeight) {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { inWidth, inHeight, 1 };

	vkCmdCopyBufferToImage(
		commandBuffer,
		inBuffer,
		inImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
}

bool VulkanHelper::GenerateMipmaps(
		const VkPhysicalDevice& inPhysicalDevice,
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		std::int32_t inTexWidth,
		std::int32_t inTexHeight,
		std::uint32_t inMipLevles) {
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(inPhysicalDevice, inFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		ReturnFalse(L"Texture image format does not support linear bliting");
	}

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = inImage;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	std::int32_t mipWidth = inTexWidth;
	std::int32_t mipHeight = inTexHeight;

	for (std::uint32_t i = 1; i < inMipLevles; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1, mipHeight > 1 ? mipHeight >> 1 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(
			commandBuffer,
			inImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			inImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth = mipWidth >> 1;
		if (mipHeight > 1) mipHeight = mipHeight >> 1;;
	}

	barrier.subresourceRange.baseMipLevel = inMipLevles - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

	return true;
}

bool VulkanHelper::CreateImage(
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
		VkDeviceMemory& outImageMemory) {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<std::uint32_t>(inWidth);
	imageInfo.extent.height = static_cast<std::uint32_t>(inHeight);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = inMipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = inFormat;
	imageInfo.tiling = inTiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = inUsage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = inNumSamples;
	imageInfo.flags = 0;

	if (vkCreateImage(inDevice, &imageInfo, nullptr, &outImage) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(inDevice, outImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(inPhysicalDevice, memRequirements.memoryTypeBits, inProperties);

	if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &outImageMemory) != VK_SUCCESS) {
		ReturnFalse(L"Failed to allocate texture image memory");
	}

	vkBindImageMemory(inDevice, outImage, outImageMemory, 0);

	return true;
}

bool VulkanHelper::CreateImageView(
		const VkDevice& inDevice,
		const VkImage& inImage,
		const VkFormat& inFormat,
		std::uint32_t inMipLevles,
		const VkImageAspectFlags& inAspectFlags,
		VkImageView& outImageView) {
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = inImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = inFormat;
	viewInfo.subresourceRange.aspectMask = inAspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = inMipLevles;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(inDevice, &viewInfo, nullptr, &outImageView) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create texture image view");
	}

	return true;
}

VkFormat VulkanHelper::FindSupportedFormat(
		const VkPhysicalDevice& inPhysicalDevice,
		const std::vector<VkFormat>& inCandidates,
		const VkImageTiling& inTiling,
		const VkFormatFeatureFlags inFeatures) {
	for (VkFormat format : inCandidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(inPhysicalDevice, format, &props);

		if (inTiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & inFeatures) == inFeatures) {
			return format;
		}
		else if (inTiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & inFeatures) == inFeatures) {
			return format;
		}
	}

	return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanHelper::FindDepthFormat(const VkPhysicalDevice& inPhysicalDevice) {
	return FindSupportedFormat(
		inPhysicalDevice,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VulkanHelper::HasStencilComponent(const VkFormat& inFormat) {
	return inFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || inFormat == VK_FORMAT_D24_UNORM_S8_UINT;
}

bool VulkanHelper::TransitionImageLayout(
		const VkDevice& inDevice,
		const VkQueue& inQueue,
		const VkCommandPool& inCommandPool,
		const VkImage& inImage,
		const VkFormat& inFormat,
		const VkImageLayout& inOldLayout,
		const VkImageLayout& inNewLayout,
		std::uint32_t inMipLevels) {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = inOldLayout;
	barrier.newLayout = inNewLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = inImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = inMipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (HasStencilComponent(inFormat))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (inOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && inNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else {
		ReturnFalse(L"Unsupported layout transition");
	}

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

	return true;
}

bool VulkanHelper::ReadFile(const std::string& inFilePath, std::vector<char>& outData) {
	std::ifstream file(inFilePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::wstringstream wsstream;
		wsstream << L"Failed to load file: " << inFilePath.c_str();
		ReturnFalse(wsstream.str().c_str());
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
#ifdef _DEBUG
	Logln(inFilePath.c_str(), " loaded (bytes: ", std::to_string(fileSize), ")");
#endif

	outData.resize(fileSize);

	file.seekg(0);
	file.read(outData.data(), fileSize);
	file.close();

	return true;
}

bool VulkanHelper::CreateShaderModule(const VkDevice& inDevice, const std::vector<char>& inCode, VkShaderModule& outModule) {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = inCode.size();
	createInfo.pCode = reinterpret_cast<const std::uint32_t*>(inCode.data());

	if (vkCreateShaderModule(inDevice, &createInfo, nullptr, &outModule) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create shader module");
	}

	return true;
}