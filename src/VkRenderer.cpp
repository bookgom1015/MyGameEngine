#include "VkRenderer.h"
#include "Logger.h"
#include "VulkanHelper.h"

#include <fstream>
#include <stb/stb_image.h>

#undef min
#undef max

using namespace DirectX;

VkRenderer::VkRenderer() {
	bIsCleanedUp = false;

	bFramebufferResized = false;

	mClientWidth = 0;
	mClientHeight = 0;

	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float widthSquared = 32.0f * 32.0f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);
	mLightDir = { 0.57735f, -0.57735f, 0.57735f };
}

VkRenderer::~VkRenderer() {
	if (!bIsCleanedUp) CleanUp();
}

bool VkRenderer::Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;

	CheckReturn(LowInitialize(glfwWnd, width, height));

	CheckReturn(CreateRenderPass());
	CheckReturn(CreateCommandPool());
	CheckReturn(CreateColorResources());
	CheckReturn(CreateDepthResources());
	CheckReturn(CreateFramebuffers());
	CheckReturn(CreateDescriptorSetLayout());
	CheckReturn(CreatePassBuffers());
	CheckReturn(CreateGraphicsPipelineLayouts());
	CheckReturn(CreateGraphicsPipelines());
	CheckReturn(CreateDescriptorPool());
	CheckReturn(CreateCommandBuffers());
	CheckReturn(CreateSyncObjects());

	bInitialized = true;
	return true;
}

void VkRenderer::CleanUp() {
	vkDeviceWaitIdle(mDevice);

	for (const auto& matPair : mMaterials) {
		const auto& mat = matPair.second;
		vkDestroySampler(mDevice, mat->TextureSampler, nullptr);

		vkDestroyImageView(mDevice, mat->TextureImageView, nullptr);

		vkDestroyImage(mDevice, mat->TextureImage, nullptr);
		vkFreeMemory(mDevice, mat->TextureImageMemory, nullptr);
	}

	for (const auto& meshPair : mMeshes) {
		const auto& mesh = meshPair.second;
		vkDestroyBuffer(mDevice, mesh->IndexBuffer, nullptr);
		vkFreeMemory(mDevice, mesh->IndexBufferMemory, nullptr);

		vkDestroyBuffer(mDevice, mesh->VertexBuffer, nullptr);
		vkFreeMemory(mDevice, mesh->VertexBufferMemory, nullptr);
	}

	for (const auto& ritem : mRitems) {
		for (size_t i = 0; i < SwapChainImageCount; ++i) {
			vkDestroyBuffer(mDevice, ritem->UniformBuffers[i], nullptr);
			vkFreeMemory(mDevice, ritem->UniformBufferMemories[i], nullptr);
		}
	}

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
	}

	vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<std::uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyBuffer(mDevice, mShadowPassBuffers[i], nullptr);
		vkFreeMemory(mDevice, mShadowPassMemories[i], nullptr);
	}
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyBuffer(mDevice, mMainPassBuffers[i], nullptr);
		vkFreeMemory(mDevice, mMainPassMemories[i], nullptr);
	}

	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

	vkDestroyPipeline(mDevice, mGraphicsPipelines["shadow"], nullptr);

	vkDestroyFramebuffer(mDevice, mShadowFramebuffer, nullptr);

	vkDestroySampler(mDevice, mShadowImageSampler, nullptr);
	vkDestroyImageView(mDevice, mShadowDepthImageView, nullptr);
	vkDestroyImage(mDevice, mShadowImage, nullptr);
	vkFreeMemory(mDevice, mShadowImageMemory, nullptr);

	vkDestroyRenderPass(mDevice, mShadowRenderPass, nullptr);
	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

	CleanUpSwapChain();

	for (const auto& layout : mPipelineLayouts) {
		vkDestroyPipelineLayout(mDevice, layout.second, nullptr);
	}

	vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
	vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

	LowCleanUp();

	bIsCleanedUp = true;
}

bool VkRenderer::Update(float delta) {
	vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(
		mDevice,
		mSwapChain,
		UINT64_MAX,
		mImageAvailableSemaphores[mCurrentFrame],
		VK_NULL_HANDLE,
		&mCurentImageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		CheckReturn(RecreateSwapChain());
		return true;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		ReturnFalse(L"Failed to acquire swap chain image");
	}

	if (mImagesInFlight[mCurentImageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(mDevice, 1, &mImagesInFlight[mCurentImageIndex], VK_TRUE, UINT64_MAX);

	mImagesInFlight[mCurentImageIndex] = mInFlightFences[mCurrentFrame];

	CheckReturn(UpdateUniformBuffer(delta));

	return true;
}

bool VkRenderer::Draw() {
	CheckReturn(DrawShadowMap());
	CheckReturn(DrawBackBuffer());

	mCurrentFrame = (mCurrentFrame + 1) % SwapChainImageCount;

	return true;
}

bool VkRenderer::OnResize(UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;

	bFramebufferResized = true;

	return true;
}

void* VkRenderer::AddModel(const std::string& file, const Transform& trans, RenderType::Type type) {
	if (mMeshes.count(file) == 0) CheckReturn(AddGeometry(file));
	return AddRenderItem(file, trans, type);
}

void VkRenderer::RemoveModel(void* model) {

}

void VkRenderer::UpdateModel(void* model, const Transform& trans) {
	RenderItem* ritem = reinterpret_cast<RenderItem*>(model);
	if (ritem == nullptr) return;

	auto begin = mRitems.begin();
	auto end = mRitems.end();
	auto iter = std::find_if(begin, end, [&](std::unique_ptr<RenderItem>& p) {
		return p.get() == ritem;
		});

	auto ptr = iter->get();
	ptr->Scale = trans.Scale;
	ptr->Rotation = trans.Rotation;
	ptr->Position = trans.Position;
	ptr->NumFramesDirty = SwapChainImageCount;
}

void VkRenderer::SetModelVisibility(void* model, bool visible) {
	RenderItem* ritem = reinterpret_cast<RenderItem*>(model);
	if (ritem == nullptr) return;

	auto begin = mRitems.begin();
	auto end = mRitems.end();
	auto iter = std::find_if(begin, end, [&](std::unique_ptr<RenderItem>& p) {
		return p.get() == ritem;
		});

	iter->get()->Visible = visible;
}

void VkRenderer::SetModelPickable(void* model, bool pickable) {}

bool VkRenderer::SetCubeMap(const std::string& file) {
	return true;
}

bool VkRenderer::SetEquirectangularMap(const std::string& file) {
	return true;
}

void VkRenderer::Pick(float x, float y) {

}

bool VkRenderer::CreateRenderPass() {
	auto depthFormat = VulkanHelper::FindDepthFormat(mPhysicalDevice);
	// Default
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = mSwapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create render pass");
		}
	}
	// For shadows
	{
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 0;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 0;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
	
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	
		std::array<VkAttachmentDescription, 1> attachments = { depthAttachment };
	
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
	
		if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mShadowRenderPass) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create shadow render pass");
		}
	}

	return true;
}

bool VkRenderer::CreateCommandPool() {
	QueueFamilyIndices indices = QueueFamilyIndices::FindQueueFamilies(mPhysicalDevice, mSurface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.GetGraphicsFamilyIndex();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create command pool");
	}

	return true;
}

bool VkRenderer::CreateColorResources() {
	VkFormat colorFormat = mSwapChainImageFormat;

	// Default
	{
		CheckReturn(VulkanHelper::CreateImage(
			mPhysicalDevice,
			mDevice,
			mSwapChainExtent.width,
			mSwapChainExtent.height,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mColorImage,
			mColorImageMemory
		));
		CheckReturn(VulkanHelper::CreateImageView(
			mDevice,
			mColorImage,
			colorFormat,
			1,
			VK_IMAGE_ASPECT_COLOR_BIT,
			mColorImageView
		));
	}

	return true;
}

bool VkRenderer::CreateDepthResources() {
	VkFormat depthFormat = VulkanHelper::FindDepthFormat(mPhysicalDevice);

	// Default
	{
		CheckReturn(VulkanHelper::CreateImage(
			mPhysicalDevice,
			mDevice,
			mSwapChainExtent.width,
			mSwapChainExtent.height,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mDepthImage,
			mDepthImageMemory
		));
		CheckReturn(VulkanHelper::CreateImageView(
			mDevice,
			mDepthImage,
			depthFormat,
			1,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			mDepthImageView
		));
		CheckReturn(VulkanHelper::TransitionImageLayout(
			mDevice,
			mGraphicsQueue,
			mCommandPool,
			mDepthImage,
			depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			1
		));
	}
	// For shadows
	{
		CheckReturn(VulkanHelper::CreateImage(
			mPhysicalDevice,
			mDevice,
			ShadowMapSize,
			ShadowMapSize,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mShadowImage,
			mShadowImageMemory
		));
		CheckReturn(VulkanHelper::CreateImageView(
			mDevice,
			mShadowImage,
			depthFormat,
			1,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			mShadowDepthImageView
		));

		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mShadowImageSampler) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create texture sampler");
		}

		CheckReturn(VulkanHelper::TransitionImageLayout(
			mDevice,
			mGraphicsQueue,
			mCommandPool,
			mShadowImage,
			depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			1
		));
	}

	return true;
}

bool VkRenderer::CreateFramebuffers() {
	// Default
	{
		for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
			std::array<VkImageView, 2> attachments = {
				mSwapChainImageViews[i],
				mDepthImageView
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mRenderPass;
			framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = mSwapChainExtent.width;
			framebufferInfo.height = mSwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
				ReturnFalse(L"Failed to create framebuffer");
			}
		}
	}
	// For shadows
	//{
	//	std::array<VkImageView, 1> attachments = {
	//		mShadowDepthImageView
	//	};
	//
	//	VkFramebufferCreateInfo framebufferInfo = {};
	//	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	//	framebufferInfo.renderPass = mShadowRenderPass;
	//	framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	//	framebufferInfo.pAttachments = attachments.data();
	//	framebufferInfo.width = ShadowMapSize;
	//	framebufferInfo.height = ShadowMapSize;
	//	framebufferInfo.layers = 1;
	//
	//	if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mShadowFramebuffer) != VK_SUCCESS) {
	//		ReturnFalse(L"Failed to create framebuffer for shadows");
	//	}
	//}

	return true;
}

bool VkRenderer::CreateDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = EDescriptorSetLayout::EDSL_Object;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding ubpLayoutBinding = {};
	ubpLayoutBinding.binding = EDescriptorSetLayout::EDSL_Pass;
	ubpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubpLayoutBinding.descriptorCount = 1;
	ubpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	ubpLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outputMapLayoutBinding = {};
	outputMapLayoutBinding.binding = EDescriptorSetLayout::EDSL_Output;
	outputMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	outputMapLayoutBinding.descriptorCount = 1;
	outputMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	outputMapLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowMapLayoutBinding = {};
	shadowMapLayoutBinding.binding = EDescriptorSetLayout::EDSL_Shadow;
	shadowMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapLayoutBinding.descriptorCount = 1;
	shadowMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowMapLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, EDescriptorSetLayout::EDSL_Count> bindings = {
		uboLayoutBinding, ubpLayoutBinding, outputMapLayoutBinding, shadowMapLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor set layout");
	}

	return true;
}

bool VkRenderer::CreateGraphicsPipelineLayouts() {
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayouts["default"]) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create pipeline layout");
	}

	return true;
}

bool VkRenderer::CreateGraphicsPipelines() {
	// Default pipeline
	{
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;

		{
			std::vector<char> vertShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/DefaultVS.spv", vertShaderCode));
			std::vector<char> fragShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/DefaultPS.spv", fragShaderCode));

			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, vertShaderCode, vertShaderModule));
			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, fragShaderCode, fragShaderModule));
		}

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			vertShaderStageInfo, fragShaderStageInfo
		};

		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptioins = Vertex::GetAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptioins.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptioins.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mSwapChainExtent.width);
		viewport.height = static_cast<float>(mSwapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = mSwapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 0.2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = mPipelineLayouts["default"];
		pipelineInfo.renderPass = mRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipelines["default"]) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create graphics pipeline");
		}

		vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
	}
	// Pipelien for shadows
	{
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;

		{
			std::vector<char> vertShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/ShadowVS.spv", vertShaderCode));
			std::vector<char> fragShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/ShadowPS.spv", fragShaderCode));

			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, vertShaderCode, vertShaderModule));
			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, fragShaderCode, fragShaderModule));
		}

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			vertShaderStageInfo, fragShaderStageInfo
		};

		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptioins = Vertex::GetAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptioins.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptioins.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		float viewportSize = static_cast<float>(ShadowMapSize);
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = viewportSize;
		viewport.height = viewportSize;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkExtent2D extent;
		extent.width = ShadowMapSize;
		extent.height = ShadowMapSize;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 0.2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		//VkGraphicsPipelineCreateInfo pipelineInfo = {};
		//pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		//pipelineInfo.stageCount = 2;
		//pipelineInfo.pStages = shaderStages;
		//pipelineInfo.pVertexInputState = &vertexInputInfo;
		//pipelineInfo.pInputAssemblyState = &inputAssembly;
		//pipelineInfo.pViewportState = &viewportState;
		//pipelineInfo.pRasterizationState = &rasterizer;
		//pipelineInfo.pMultisampleState = &multisampling;
		//pipelineInfo.pDepthStencilState = &depthStencil;
		//pipelineInfo.pColorBlendState = nullptr;
		//pipelineInfo.pDynamicState = nullptr;
		//pipelineInfo.layout = mPipelineLayouts["default"];
		//pipelineInfo.renderPass = mShadowRenderPass;
		//pipelineInfo.subpass = 0;
		//pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		//pipelineInfo.basePipelineIndex = -1;
		//
		//if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipelines["shadow"]) != VK_SUCCESS) {
		//	ReturnFalse(L"Failed to create graphics pipeline");
		//}

		vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
	}

	return true;
}

bool VkRenderer::CreateDescriptorPool() {
	auto maxSets = static_cast<std::uint32_t>(SwapChainImageCount * 32);

	std::array<VkDescriptorPoolSize, EDescriptorSetLayout::EDSL_Count> poolSizes = {};
	poolSizes[EDescriptorSetLayout::EDSL_Object].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[EDescriptorSetLayout::EDSL_Object].descriptorCount = maxSets;

	poolSizes[EDescriptorSetLayout::EDSL_Pass].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[EDescriptorSetLayout::EDSL_Pass].descriptorCount = maxSets;

	poolSizes[EDescriptorSetLayout::EDSL_Output].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[EDescriptorSetLayout::EDSL_Output].descriptorCount = maxSets;

	poolSizes[EDescriptorSetLayout::EDSL_Shadow].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[EDescriptorSetLayout::EDSL_Shadow].descriptorCount = maxSets;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = maxSets;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor pool");
	}

	return true;
}

bool VkRenderer::CreatePassBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferPass);

	mMainPassBuffers.resize(SwapChainImageCount);
	mMainPassMemories.resize(SwapChainImageCount);

	mShadowPassBuffers.resize(SwapChainImageCount);
	mShadowPassMemories.resize(SwapChainImageCount);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mMainPassBuffers[i],
			mMainPassMemories[i]
		);
	}

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mShadowPassBuffers[i],
			mShadowPassMemories[i]
		);
	}

	return true;
}

bool VkRenderer::CreateCommandBuffers() {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = SwapChainImageCount;

	if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create command buffers");
	}

	return true;
}

bool VkRenderer::CreateSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		mImagesInFlight[i] = VK_NULL_HANDLE;

		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			ReturnFalse(L"Failed to create synchronization object(s) for a frame");
	}

	return true;
}

bool VkRenderer::RecreateColorResources() {
	VkFormat colorFormat = mSwapChainImageFormat;

	// Default
	{
		CheckReturn(VulkanHelper::CreateImage(
			mPhysicalDevice,
			mDevice,
			mSwapChainExtent.width,
			mSwapChainExtent.height,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mColorImage,
			mColorImageMemory
		));
		CheckReturn(VulkanHelper::CreateImageView(
			mDevice,
			mColorImage,
			colorFormat,
			1,
			VK_IMAGE_ASPECT_COLOR_BIT,
			mColorImageView
		));
	}

	return true;
}

bool VkRenderer::RecreateDepthResources() {
	VkFormat depthFormat = VulkanHelper::FindDepthFormat(mPhysicalDevice);

	// Default
	{
		CheckReturn(VulkanHelper::CreateImage(
			mPhysicalDevice,
			mDevice,
			mSwapChainExtent.width,
			mSwapChainExtent.height,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mDepthImage,
			mDepthImageMemory
		));
		CheckReturn(VulkanHelper::CreateImageView(
			mDevice,
			mDepthImage,
			depthFormat,
			1,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			mDepthImageView
		));
		CheckReturn(VulkanHelper::TransitionImageLayout(
			mDevice,
			mGraphicsQueue,
			mCommandPool,
			mDepthImage,
			depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			1
		));
	}

	return true;
}

bool VkRenderer::RecreateFramebuffers() {
	// Default
	{
		for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
			std::array<VkImageView, 2> attachments = {
				mSwapChainImageViews[i],
				mDepthImageView
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mRenderPass;
			framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = mSwapChainExtent.width;
			framebufferInfo.height = mSwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
				ReturnFalse(L"Failed to create framebuffer");
			}
		}
	}

	return true;
}

bool VkRenderer::RecreateGraphicsPipelines() {
	// Default pipeline
	{
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;

		{
			std::vector<char> vertShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/DefaultVS.spv", vertShaderCode));
			std::vector<char> fragShaderCode;
			CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/glsl/DefaultPS.spv", fragShaderCode));

			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, vertShaderCode, vertShaderModule));
			CheckReturn(VulkanHelper::CreateShaderModule(mDevice, fragShaderCode, fragShaderModule));
		}

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			vertShaderStageInfo, fragShaderStageInfo
		};

		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptioins = Vertex::GetAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptioins.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptioins.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mSwapChainExtent.width);
		viewport.height = static_cast<float>(mSwapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = mSwapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 0.2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = mPipelineLayouts["default"];
		pipelineInfo.renderPass = mRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipelines["default"]) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create graphics pipeline");
		}

		vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
	}

	return true;
}

bool VkRenderer::RecreateSwapChain() {
	vkDeviceWaitIdle(mDevice);

	CleanUpSwapChain();

	VkLowRenderer::RecreateSwapChain();

	CheckReturn(RecreateColorResources());
	CheckReturn(RecreateDepthResources());
	CheckReturn(RecreateFramebuffers());
	CheckReturn(RecreateGraphicsPipelines());

	for (const auto& ritem : mRitems) {
		ritem->NumFramesDirty = SwapChainImageCount;
	}

	return true;
}

void VkRenderer::CleanUpSwapChain() {
	vkDestroyPipeline(mDevice, mGraphicsPipelines["default"], nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i)
		vkDestroyFramebuffer(mDevice, mSwapChainFramebuffers[i], nullptr);

	vkDestroyImageView(mDevice, mDepthImageView, nullptr);
	vkDestroyImage(mDevice, mDepthImage, nullptr);
	vkFreeMemory(mDevice, mDepthImageMemory, nullptr);

	vkDestroyImageView(mDevice, mColorImageView, nullptr);
	vkDestroyImage(mDevice, mColorImage, nullptr);
	vkFreeMemory(mDevice, mColorImageMemory, nullptr);

	VkLowRenderer::CleanUpSwapChain();
}

bool VkRenderer::AddGeometry(const std::string& file) {
	Mesh mesh;
	Material mat;
	CheckReturn(MeshImporter::LoadObj(file, mesh, mat));

	std::unique_ptr<MeshData> meshData = std::make_unique<MeshData>();

	// Create a vertex buffer.
	{
		auto& vertices = mesh.Vertices;

		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CheckReturn(VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory));

		void* data;
		vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
		vkUnmapMemory(mDevice, stagingBufferMemory);

		CheckReturn(VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			meshData->VertexBuffer,
			meshData->VertexBufferMemory));

		VulkanHelper::CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, meshData->VertexBuffer, bufferSize);

		vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
	}
	// Create a index buffer.
	{
		auto& indices = mesh.Indices;

		meshData->IndexCount = static_cast<std::uint32_t>(indices.size());

		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CheckReturn(VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory));

		void* data;
		vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		std::memcpy(data, indices.data(), bufferSize);
		vkUnmapMemory(mDevice, stagingBufferMemory);

		CheckReturn(VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			meshData->IndexBuffer,
			meshData->IndexBufferMemory));

		VulkanHelper::CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, meshData->IndexBuffer, bufferSize);

		vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
	}

	mMeshes[file] = std::move(meshData);

	if (mMaterials.count(file) == 0) CheckReturn(AddMaterial(file, mat));

	return true;
}

bool VkRenderer::AddMaterial(const std::string& file, const Material& material) {
	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;

	stbi_uc* pixels = stbi_load(material.DiffuseMapFileName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) ReturnFalse(L"Failed to load texture image");

	std::unique_ptr<MaterialData> matData = std::make_unique<MaterialData>();
	auto pMat = matData.get();
	CheckReturn(CreateTextureImage(texWidth, texHeight, pixels, pMat));
	CheckReturn(CreateTextureImageView(pMat));
	CheckReturn(CreateTextureSampler(pMat));
	mMaterials[file] = std::move(matData);

	stbi_image_free(pixels);

	return true;
}

void* VkRenderer::AddRenderItem(const std::string& file, const Transform& trans, RenderType::Type type) {
	std::unique_ptr<RenderItem> ritem = std::make_unique<RenderItem>();
	ritem->Scale = trans.Scale;
	ritem->Rotation = trans.Rotation;
	ritem->Position = trans.Position;
	ritem->MeshName = file;
	ritem->MatName = file;

	auto pRitem = ritem.get();
	CheckReturn(CreateUniformBuffers(pRitem));
	CheckReturn(CreateDescriptorSets(pRitem));
	CheckReturn(UpdateDescriptorSet(pRitem));

	mRitemRefs[type].push_back(pRitem);
	mRitems.push_back(std::move(ritem));

	return mRitems.back().get();
}

bool VkRenderer::CreateTextureImage(int width, int height, void* data, MaterialData* mat) {
	VkDeviceSize imageSize = width * height * 4;

	mat->MipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VulkanHelper::CreateBuffer(
		mPhysicalDevice,
		mDevice,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* pData;
	vkMapMemory(mDevice, stagingBufferMemory, 0, imageSize, 0, &pData);
	std::memcpy(pData, data, static_cast<size_t>(imageSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckReturn(VulkanHelper::CreateImage(
		mPhysicalDevice,
		mDevice,
		width,
		height,
		mat->MipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		ImageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mat->TextureImage,
		mat->TextureImageMemory)
	);
	CheckReturn(VulkanHelper::TransitionImageLayout(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		mat->TextureImage,
		ImageFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mat->MipLevels)
	);
	VulkanHelper::CopyBufferToImage(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		stagingBuffer,
		mat->TextureImage,
		static_cast<std::uint32_t>(width),
		static_cast<std::uint32_t>(height)
	);
	VulkanHelper::GenerateMipmaps(
		mPhysicalDevice,
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		mat->TextureImage,
		ImageFormat,
		width,
		height,
		mat->MipLevels
	);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return true;
}

bool VkRenderer::CreateTextureImageView(MaterialData* mat) {
	CheckReturn(VulkanHelper::CreateImageView(
		mDevice,
		mat->TextureImage,
		ImageFormat,
		mat->MipLevels,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mat->TextureImageView
	));

	return true;
}

bool VkRenderer::CreateTextureSampler(MaterialData* mat) {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mat->MipLevels);

	if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mat->TextureSampler) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create texture sampler");
	}

	return true;
}

bool VkRenderer::CreateUniformBuffers(RenderItem* ritem) {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VulkanHelper::CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			ritem->UniformBuffers[i],
			ritem->UniformBufferMemories[i]
		);
	}

	return true;
}

bool VkRenderer::CreateDescriptorSets(RenderItem* ritem) {
	std::vector<VkDescriptorSetLayout> layouts(SwapChainImageCount, mDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(mDevice, &allocInfo, ritem->DescriptorSets.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to allocate descriptor sets");
	}

	return true;
}

bool VkRenderer::UpdateUniformBuffer(float delta) {
	UniformBufferPass mainPass = {};
	UniformBufferPass shadowPass = {};

	// Update shadow pass buffers.
	{
		XMVECTOR lightDir = XMLoadFloat3(&mLightDir);
		XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
		XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
		XMVECTOR lightUp = UnitVectors::UpVector;
		XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

		// Transform bounding sphere to light space.
		XMFLOAT3 sphereCenterLS;
		XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

		// Ortho frustum in light space encloses scene.
		float l = sphereCenterLS.x - mSceneBounds.Radius;
		float b = sphereCenterLS.y - mSceneBounds.Radius;
		float n = sphereCenterLS.z - mSceneBounds.Radius;
		float r = sphereCenterLS.x + mSceneBounds.Radius;
		float t = sphereCenterLS.y + mSceneBounds.Radius;
		float f = sphereCenterLS.z + mSceneBounds.Radius;

		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		// Transform NDC space [-1 , +1]^2 to texture space [0, 1]^2
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		);

		XMMATRIX S = lightView * lightProj * T;
		XMStoreFloat4x4(&mainPass.ShadowTransform, XMMatrixTranspose(S));

		XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);
		XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(lightView), lightView);
		XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(lightProj), lightProj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		XMStoreFloat4x4(&shadowPass.View, XMMatrixTranspose(lightView));
		XMStoreFloat4x4(&shadowPass.InvView, XMMatrixTranspose(invView));
		XMStoreFloat4x4(&shadowPass.Proj, XMMatrixTranspose(lightProj));
		XMStoreFloat4x4(&shadowPass.InvProj, XMMatrixTranspose(invProj));
		XMStoreFloat4x4(&shadowPass.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&shadowPass.InvViewProj, XMMatrixTranspose(invViewProj));
		XMStoreFloat3(&shadowPass.EyePosW, lightPos);

		void* data;
		const auto& bufferMemory = mMainPassMemories[mCurentImageIndex];
		vkMapMemory(mDevice, bufferMemory, 0, sizeof(shadowPass), 0, &data);
		std::memcpy(data, &shadowPass, sizeof(shadowPass));
		vkUnmapMemory(mDevice, bufferMemory);
	}

	// Update main pass buffers.
	{
		mainPass.View = mCamera->GetView();
		mainPass.Proj = mCamera->GetProj();
		mainPass.Proj.m[1][1] *= -1.0f;
		XMStoreFloat3(&mainPass.EyePosW, mCamera->GetPosition());
		mainPass.AmbientLight = { 0.3f, 0.3f, 0.42f, 1.0f };
		mainPass.Lights[0].Direction = mLightDir;
		mainPass.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };

		void* data;
		const auto& bufferMemory = mMainPassMemories[mCurentImageIndex];
		vkMapMemory(mDevice, bufferMemory, 0, sizeof(mainPass), 0, &data);
		std::memcpy(data, &mainPass, sizeof(mainPass));
		vkUnmapMemory(mDevice, bufferMemory);
	}

	for (auto& ritem : mRitems) {
		if (ritem->NumFramesDirty > 0) {
			UniformBufferObject ubo = {};

			XMStoreFloat4x4(
				&ubo.Model,
				XMMatrixAffineTransformation(
					ritem->Scale,
					XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
					ritem->Rotation,
					ritem->Position
				)
			);

			void* data;
			const auto& bufferMemory = ritem->UniformBufferMemories[mCurentImageIndex];
			vkMapMemory(mDevice, bufferMemory, 0, sizeof(ubo), 0, &data);
			std::memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(mDevice, bufferMemory);

			ritem->NumFramesDirty--;
		}
	}

	return true;
}

bool VkRenderer::UpdateDescriptorSet(const RenderItem* ritem) {
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = ritem->UniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		std::array<VkWriteDescriptorSet, EDescriptorSetLayout::EDSL_Count> descriptorWrites = {};
		auto uboIndex = EDescriptorSetLayout::EDSL_Object;
		descriptorWrites[uboIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[uboIndex].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[uboIndex].dstBinding = uboIndex;
		descriptorWrites[uboIndex].dstArrayElement = 0;
		descriptorWrites[uboIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[uboIndex].descriptorCount = 1;
		descriptorWrites[uboIndex].pBufferInfo = &bufferInfo;

		VkDescriptorBufferInfo passBufferInfo = {};
		passBufferInfo.buffer = mMainPassBuffers[i];
		passBufferInfo.offset = 0;
		passBufferInfo.range = sizeof(UniformBufferPass);

		auto ubpIndex = EDescriptorSetLayout::EDSL_Pass;
		descriptorWrites[ubpIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ubpIndex].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[ubpIndex].dstBinding = ubpIndex;
		descriptorWrites[ubpIndex].dstArrayElement = 0;
		descriptorWrites[ubpIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ubpIndex].descriptorCount = 1;
		descriptorWrites[ubpIndex].pBufferInfo = &passBufferInfo;

		const auto& mat = mMaterials[ritem->MatName];
		VkDescriptorImageInfo outputMapInfo = {};
		outputMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		outputMapInfo.imageView = mat->TextureImageView;
		outputMapInfo.sampler = mat->TextureSampler;

		auto outputIndex = EDescriptorSetLayout::EDSL_Output;
		descriptorWrites[outputIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[outputIndex].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[outputIndex].dstBinding = outputIndex;
		descriptorWrites[outputIndex].dstArrayElement = 0;
		descriptorWrites[outputIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[outputIndex].descriptorCount = 1;
		descriptorWrites[outputIndex].pImageInfo = &outputMapInfo;

		VkDescriptorImageInfo shadowMapInfo = {};
		shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadowMapInfo.imageView = mShadowDepthImageView;
		shadowMapInfo.sampler = mShadowImageSampler;

		auto shadowIndex = EDescriptorSetLayout::EDSL_Shadow;
		descriptorWrites[shadowIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[shadowIndex].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[shadowIndex].dstBinding = shadowIndex;
		descriptorWrites[shadowIndex].dstArrayElement = 0;
		descriptorWrites[shadowIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[shadowIndex].descriptorCount = 1;
		descriptorWrites[shadowIndex].pImageInfo = &shadowMapInfo;

		vkUpdateDescriptorSets(mDevice, static_cast<std::uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return true;
}

bool VkRenderer::DrawShadowMap() {


	return true;
}

bool VkRenderer::DrawBackBuffer() {
	auto& commandBuffer = mCommandBuffers[mCurrentFrame];
	if (vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) {
		ReturnFalse(L"Failed to reset command buffer");
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		ReturnFalse(L"Failed to begin recording command buffer");
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = mRenderPass;
	renderPassInfo.framebuffer = mSwapChainFramebuffers[mCurentImageIndex];
	renderPassInfo.renderArea.offset = { 0,0 };
	renderPassInfo.renderArea.extent = mSwapChainExtent;

	// Note: that the order of clearValues should be identical to the order of attachments.
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.941176534f, 0.972549081f, 1.000000000f, 1.000000000f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipelines["default"]);

	for (auto ritem : mRitemRefs[RenderType::E_Opaque]) {
		if (!ritem->Visible) continue;

		const auto& mesh = mMeshes[ritem->MeshName];

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayouts["default"], 0, 1, &ritem->DescriptorSets[mCurrentFrame], 0, nullptr);

		VkBuffer vertexBuffers[] = { mesh->VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(commandBuffer, mesh->IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, mesh->IndexCount, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		ReturnFalse(L"Failed to record command buffer");
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {
		mImageAvailableSemaphores[mCurrentFrame]
	};
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = {
		mRenderFinishedSemaphores[mCurrentFrame]
	};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
		ReturnFalse(L"Failed to submit draw command buffer");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &mCurentImageIndex;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || bFramebufferResized) {
		bFramebufferResized = false;
		CheckReturn(RecreateSwapChain());
	}
	else if (result != VK_SUCCESS) {
		ReturnFalse(L"Failed to present swap chain image");
	}

	return true;
}