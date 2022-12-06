#pragma once

#include "VkLowRenderer.h"
#include "MeshImporter.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class VkRenderer : public Renderer, public VkLowRenderer {
public:
	struct MeshData {
		VkBuffer		VertexBuffer;
		VkDeviceMemory	VertexBufferMemory;

		VkBuffer		IndexBuffer;
		VkDeviceMemory	IndexBufferMemory;

		std::uint32_t	IndexCount;
	};

	struct MaterialData {
		VkImage			TextureImage;
		VkDeviceMemory	TextureImageMemory;
		VkImageView		TextureImageView;
		VkSampler		TextureSampler;

		std::uint32_t MipLevels;
	};

	struct UniformBufferObject {
		DirectX::XMFLOAT4X4 Model;
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 Proj;
	};

	struct RenderItem {
		std::vector<VkDescriptorSet> DescriptorSets;

		std::vector<VkBuffer>		UniformBuffers;
		std::vector<VkDeviceMemory>	UniformBufferMemories;

		std::string MeshName;
		std::string MatName;

		DirectX::XMVECTOR Scale;
		DirectX::XMVECTOR Rotation;
		DirectX::XMVECTOR Position;

		bool Visible = true;
	};

public:
	VkRenderer();
	virtual ~VkRenderer();

public:
	virtual bool Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) override;
	virtual void CleanUp() override;

	virtual bool Update(float delta) override;
	virtual bool Draw() override;

	virtual bool OnResize(UINT width, UINT height) override;

	virtual void* AddModel(const std::string& file, const Transform& trans, ERenderTypes type = ERenderTypes::EOpaque) override;
	virtual void RemoveModel(void* model) override;

	virtual void UpdateModel(void* model, const Transform& trans) override;
	virtual void SetModelVisibility(void* model, bool visible) override;

protected:
	virtual bool RecreateSwapChain() override;
	virtual void CleanUpSwapChain() override;

private:
	bool CreateRenderPass();
	bool CreateCommandPool();
	bool CreateColorResources();
	bool CreateDepthResources();
	bool CreateFramebuffers();
	bool CreateDescriptorSetLayout();
	bool CreateGraphicsPipeline();
	bool CreateDescriptorPool();
	bool CreateCommandBuffers();
	bool CreateSyncObjects();

	bool AddGeometry(const std::string& file);
	bool AddMaterial(const std::string& file, const Material& material);
	void* AddRenderItem(const std::string& file, const Transform& trans, ERenderTypes type);

	bool CreateTextureImage(int width, int height, void* data, MaterialData* mat);
	bool CreateTextureImageView(MaterialData* mat);
	bool CreateTextureSampler(MaterialData* mat);

	bool CreateUniformBuffers(RenderItem* ritem);
	bool CreateDescriptorSets(RenderItem* ritem);

	bool UpdateUniformBuffer(float delta);
	bool UpdateDescriptorSet(const RenderItem* ritem);

	bool RecreateUniformBuffers();
	bool RecreateDescriptorSets();

protected:
	const VkFormat ImageFormat;

private:
	bool bIsCleanedUp;

	bool bFramebufferResized;

	VkRenderPass mRenderPass;

	VkCommandPool mCommandPool;
	std::vector<VkCommandBuffer> mCommandBuffers;

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;

	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	std::vector<VkFramebuffer> mSwapChainFramebuffers;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorPool mDescriptorPool;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mGraphicsPipeline;

	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<VkFence> mImagesInFlight;
	std::uint32_t mCurentImageIndex;
	size_t mCurrentFrame;

	std::unordered_map<std::string, std::unique_ptr<MeshData>> mMeshes;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[ERenderTypes::ENumTypes];
};