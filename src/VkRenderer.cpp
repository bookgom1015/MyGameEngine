#include "VkRenderer.h"
#include "Logger.h"
#include "VulkanHelper.h"

#include <fstream>
#include <stb_image.h>

#undef min
#undef max

using namespace DirectX;

VkRenderer::VkRenderer() : ImageFormat(VK_FORMAT_R8G8B8A8_UNORM) {
	bInitialized = false;
	bIsCleanedUp = false;

	bFramebufferResized = false;

	mClientWidth = 0;
	mClientHeight = 0;
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
	CheckReturn(CreateGraphicsPipeline());
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

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
	}

	CleanUpSwapChain();

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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

	for (auto ritem : mRitemRefs[ERenderTypes::EOpaque]) {
		if (!ritem->Visible) continue;
		
		const auto& mesh = mMeshes[ritem->MeshName];
		
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &ritem->DescriptorSets[mCurrentFrame], 0, nullptr);

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

	mCurrentFrame = (mCurrentFrame + 1) % SwapChainImageCount;

	return true;
}

bool VkRenderer::OnResize(UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;

	bFramebufferResized = true;

	return true;
}

void* VkRenderer::AddModel(const std::string& file, const Transform& trans, ERenderTypes type) {
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

bool VkRenderer::CreateRenderPass() {
	auto msaaSamples = GetMSAASamples();

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = VulkanHelper::FindDepthFormat(mPhysicalDevice);
	depthAttachment.samples = msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve = {};
	colorAttachmentResolve.format = mSwapChainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentResolveRef = {};
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

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

	CheckReturn(VulkanHelper::CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		GetMSAASamples(),
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mColorImage,
		mColorImageMemory));

	CheckReturn(VulkanHelper::CreateImageView(
		mDevice,
		mColorImage,
		colorFormat,
		1,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mColorImageView));

	return true;
}

bool VkRenderer::CreateDepthResources() {
	VkFormat depthFormat = VulkanHelper::FindDepthFormat(mPhysicalDevice);

	CheckReturn(VulkanHelper::CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		GetMSAASamples(),
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mDepthImage,
		mDepthImageMemory));

	CheckReturn(VulkanHelper::CreateImageView(
		mDevice,
		mDepthImage,
		depthFormat,
		1,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		mDepthImageView));

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

	return true;
}

bool VkRenderer::CreateFramebuffers() {
	mSwapChainFramebuffers.resize(mSwapChainImageViews.size());

	for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
		std::array<VkImageView, 3> attachments = {
			mColorImageView,
			mDepthImageView,
			mSwapChainImageViews[i],
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

	return true;
}

bool VkRenderer::CreateDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor set layout");
	}

	return true;
}

bool VkRenderer::CreateGraphicsPipeline() {
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	{
		std::vector<char> vertShaderCode;
		CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/vert.spv", vertShaderCode));
		std::vector<char> fragShaderCode;
		CheckReturn(VulkanHelper::ReadFile("./../../assets/shaders/frag.spv", fragShaderCode));

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
	multisampling.rasterizationSamples = GetMSAASamples();
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

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create pipeline layout");
	}

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
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create graphics pipeline");
	}

	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);

	return true;
}

bool VkRenderer::CreateDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<std::uint32_t>(SwapChainImageCount * 32);
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor pool");
	}

	return true;
}

bool VkRenderer::CreateCommandBuffers() {
	mCommandBuffers.resize(mSwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size());

	if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create command buffers");
	}

	return true;
}

bool VkRenderer::CreateSyncObjects() {
	mImageAvailableSemaphores.resize(SwapChainImageCount);
	mRenderFinishedSemaphores.resize(SwapChainImageCount);
	mInFlightFences.resize(SwapChainImageCount);
	mImagesInFlight.resize(SwapChainImageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			ReturnFalse(L"Failed to create synchronization object(s) for a frame");
	}

	return true;
}

bool VkRenderer::RecreateSwapChain() {
	vkDeviceWaitIdle(mDevice);

	CleanUpSwapChain();

	VkLowRenderer::RecreateSwapChain();

	CheckReturn(CreateRenderPass());
	CheckReturn(CreateColorResources());
	CheckReturn(CreateDepthResources());
	CheckReturn(CreateFramebuffers());
	CheckReturn(CreateGraphicsPipeline());
	CheckReturn(CreateDescriptorPool());
	CheckReturn(CreateCommandBuffers());
	CheckReturn(RecreateUniformBuffers());
	CheckReturn(RecreateDescriptorSets());

	return true;
}

void VkRenderer::CleanUpSwapChain() {
	for (const auto& ritem : mRitems) {
		for (size_t i = 0; i < SwapChainImageCount; ++i) {
			vkDestroyBuffer(mDevice, ritem->UniformBuffers[i], nullptr);
			vkFreeMemory(mDevice, ritem->UniformBufferMemories[i], nullptr);
		}
	}

	vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<std::uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());

	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i)
		vkDestroyFramebuffer(mDevice, mSwapChainFramebuffers[i], nullptr);

	vkDestroyImageView(mDevice, mDepthImageView, nullptr);
	vkDestroyImage(mDevice, mDepthImage, nullptr);
	vkFreeMemory(mDevice, mDepthImageMemory, nullptr);

	vkDestroyImageView(mDevice, mColorImageView, nullptr);
	vkDestroyImage(mDevice, mColorImage, nullptr);
	vkFreeMemory(mDevice, mColorImageMemory, nullptr);

	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

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

void* VkRenderer::AddRenderItem(const std::string& file, const Transform& trans, ERenderTypes type) {
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

	ritem->UniformBuffers.resize(SwapChainImageCount);
	ritem->UniformBufferMemories.resize(SwapChainImageCount);

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
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(SwapChainImageCount);
	allocInfo.pSetLayouts = layouts.data();

	ritem->DescriptorSets.resize(SwapChainImageCount);
	if (vkAllocateDescriptorSets(mDevice, &allocInfo, ritem->DescriptorSets.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to allocate descriptor sets");
	}

	return true;
}

bool VkRenderer::UpdateUniformBuffer(float delta) {
	for (auto& ritem : mRitems) {
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
		ubo.View = mCamera->GetView();
		ubo.Proj = mCamera->GetProj();
		ubo.Proj.m[1][1] *= -1.0f;
		
		void* data;
		const auto& bufferMemory = ritem->UniformBufferMemories[mCurentImageIndex];
		vkMapMemory(mDevice, bufferMemory, 0, sizeof(ubo), 0, &data);
		std::memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(mDevice, bufferMemory);
	}

	return true;
}

bool VkRenderer::UpdateDescriptorSet(const RenderItem* ritem) {
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = ritem->UniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		const auto& mat = mMaterials[ritem->MatName];
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mat->TextureImageView;
		imageInfo.sampler = mat->TextureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = ritem->DescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(mDevice, static_cast<std::uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return true;
}

bool VkRenderer::RecreateUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	for (auto& ritem : mRitems) {
		ritem->UniformBuffers.resize(SwapChainImageCount);
		ritem->UniformBufferMemories.resize(SwapChainImageCount);

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
	}

	return true;
}

bool VkRenderer::RecreateDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(SwapChainImageCount, mDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(SwapChainImageCount);
	allocInfo.pSetLayouts = layouts.data();

	for (auto& ritem : mRitems) {
		ritem->DescriptorSets.resize(SwapChainImageCount);
		if (vkAllocateDescriptorSets(mDevice, &allocInfo, ritem->DescriptorSets.data()) != VK_SUCCESS) {
			ReturnFalse(L"Failed to allocate descriptor sets");
		}
	}

	for (auto& ritem : mRitems) {
		CheckReturn(UpdateDescriptorSet(ritem.get()));
	}

	return true;
}