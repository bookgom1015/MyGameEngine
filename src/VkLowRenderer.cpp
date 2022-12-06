#include "VkLowRenderer.h"
#include "Logger.h"
#include "VulkanHelper.h"

VkLowRenderer::VkLowRenderer() {
	bIsCleanedUp = false;

	mMSAASamples = VK_SAMPLE_COUNT_1_BIT;
}

VkLowRenderer::~VkLowRenderer() {
	if (!bIsCleanedUp) LowCleanUp();
}

bool VkLowRenderer::LowInitialize(GLFWwindow* glfwWnd, UINT width, UINT height) {
	mGlfwWnd = glfwWnd;

	CheckReturn(CreateInstance());
	CheckReturn(VulkanHelper::SetUpDebugMessenger(mInstance, mDebugMessenger));
	CheckReturn(CreateSurface());
	CheckReturn(SelectPhysicalDevice());
	CheckReturn(CreateLogicalDevice());
	CheckReturn(CreateSwapChain());
	CheckReturn(CreateImageViews());

	return true;
}

void VkLowRenderer::LowCleanUp() {
	vkDestroyDevice(mDevice, nullptr);

	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

#ifdef _DEBUG
	VulkanHelper::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
#endif

	vkDestroyInstance(mInstance, nullptr);

	bIsCleanedUp = true;
}

bool VkLowRenderer::RecreateSwapChain() {
	CheckReturn(CreateSwapChain());
	CheckReturn(CreateImageViews());

	return true;
}

void VkLowRenderer::CleanUpSwapChain() {
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
}

bool VkLowRenderer::CreateInstance() {
#ifdef _DEBUG
	CheckReturn(VulkanHelper::CheckValidationLayersSupport());
#endif

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "MyGameEngine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.pEngineName = "Game Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::uint32_t availableExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

	std::vector<const char*> requiredExtensions;
	VulkanHelper::GetRequiredExtensions(requiredExtensions);

	std::vector<const char*> missingExtensions;
	bool status = true;

	for (const auto& requiredExt : requiredExtensions) {
		bool supported = false;

		for (const auto& availableExt : availableExtensions) {
			if (std::strcmp(requiredExt, availableExt.extensionName) == 0) {
				supported = true;
				break;
			}
		}

		if (!supported) {
			missingExtensions.push_back(requiredExt);
			status = false;
		}
	}

	if (!status) {
		WLogln(L"Upsupported extensions:");
		for (const auto& missingExt : missingExtensions) {
			Logln("\t ", missingExt);
		}

		return false;
	}

	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
#ifdef _DEBUG
	createInfo.enabledLayerCount = static_cast<std::uint32_t>(VulkanHelper::ValidationLayers.size());
	createInfo.ppEnabledLayerNames = VulkanHelper::ValidationLayers.data();

	VulkanHelper::PopulateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;
#endif

	if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create instance");
	}

	return true;
}

bool VkLowRenderer::CreateSurface() {
	if (glfwCreateWindowSurface(mInstance, mGlfwWnd, nullptr, &mSurface) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create surface");
	}

	return true;
}

bool VkLowRenderer::SelectPhysicalDevice() {
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		ReturnFalse(L"Failed to find GPU(s) with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices) {
		int score = VulkanHelper::RateDeviceSuitability(device, mSurface);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0) {
		auto physicalDevice = candidates.rbegin()->second;
		mPhysicalDevice = physicalDevice;

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		Logln("Selected physical device: ", deviceProperties.deviceName);

		mMSAASamples = VulkanHelper::GetMaxUsableSampleCount(physicalDevice);
		WLogln(L"Multi sample counts: ", std::to_wstring(mMSAASamples));
	}
	else {
		ReturnFalse(L"Failed to find a suitable GPU");
	}

	return true;
}

bool VkLowRenderer::CreateLogicalDevice() {
	QueueFamilyIndices indices = QueueFamilyIndices::FindQueueFamilies(mPhysicalDevice, mSurface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<std::uint32_t> uniqueQueueFamilies = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	float queuePriority = 1.0f;
	for (std::uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(VulkanHelper::DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = VulkanHelper::DeviceExtensions.data();

#ifdef _DEBUG
	createInfo.enabledLayerCount = static_cast<std::uint32_t>(VulkanHelper::ValidationLayers.size());
	createInfo.ppEnabledLayerNames = VulkanHelper::ValidationLayers.data();
#else
	createInfo.enabledLayerCount = 0;
#endif

	if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create logical device");
	}

	vkGetDeviceQueue(mDevice, indices.GetGraphicsFamilyIndex(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, indices.GetPresentFamilyIndex(), 0, &mPresentQueue);

	return true;
}

bool VkLowRenderer::CreateSwapChain() {
	SwapChainSupportDetails swapChainSupport = VulkanHelper::QuerySwapChainSupport(mPhysicalDevice, mSurface);

	VkSurfaceFormatKHR surfaceFormat = VulkanHelper::ChooseSwapSurfaceFormat(swapChainSupport.Formats);
	VkPresentModeKHR presentMode = VulkanHelper::ChooseSwapPresentMode(swapChainSupport.PresentModes);
	VkExtent2D extent = VulkanHelper::ChooseSwapExtent(mGlfwWnd, swapChainSupport.Capabilities);

	std::uint32_t imageCount = SwapChainImageCount;

	if (imageCount < swapChainSupport.Capabilities.minImageCount) {
		imageCount = swapChainSupport.Capabilities.minImageCount;
	}
	else if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount) {
		imageCount = swapChainSupport.Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mSurface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = QueueFamilyIndices::FindQueueFamilies(mPhysicalDevice, mSurface);
	std::uint32_t queueFamilyIndices[] = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	if (indices.GraphicsFamily != indices.PresentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 1;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent;

	return true;
}

bool VkLowRenderer::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (size_t i = 0, end = mSwapChainImages.size(); i < end; ++i) {
		CheckReturn(VulkanHelper::CreateImageView(mDevice, mSwapChainImages[i], mSwapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT, mSwapChainImageViews[i]));
	}

	return true;
}