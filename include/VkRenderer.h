#pragma once

#include "VkLowRenderer.h"
#include "MeshImporter.h"
#include "Light.h"

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
	};

	struct UniformBufferPass {
		DirectX::XMFLOAT4X4	View;
		DirectX::XMFLOAT4X4	InvView;
		DirectX::XMFLOAT4X4	Proj;
		DirectX::XMFLOAT4X4	InvProj;
		DirectX::XMFLOAT4X4	ViewProj;
		DirectX::XMFLOAT4X4	InvViewProj;
		DirectX::XMFLOAT4X4 ShadowTransform;
		DirectX::XMFLOAT3	EyePosW;
		float				PassConstantsPad1;
		DirectX::XMFLOAT4	AmbientLight;
		Light				Lights[MaxLights];
	};

	struct RenderItem {
		std::array<VkDescriptorSet, SwapChainImageCount> DescriptorSets;

		std::array<VkBuffer, SwapChainImageCount>		UniformBuffers;
		std::array<VkDeviceMemory, SwapChainImageCount>	UniformBufferMemories;

		std::string MeshName;
		std::string MatName;

		DirectX::XMVECTOR Scale;
		DirectX::XMVECTOR Rotation;
		DirectX::XMVECTOR Position;

		int NumFramesDirty = SwapChainImageCount;

		bool Visible = true;
	};

	enum EDescriptorSetLayout {
		EDSL_Object = 0,
		EDSL_Pass,
		EDSL_Output,
		EDSL_Shadow,
		EDSL_Count
	};

	enum ECommandBufferLayout {
		ECBL_Shadow = 0,
		ECBL_BackBuffer,
		ECBL_Debug,
		ECBL_Count
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
	bool CreateCommandBuffers();
	bool CreateColorResources();
	bool CreateDepthResources();
	bool CreateFramebuffers();
	bool CreateDescriptorSetLayout();
	bool CreateGraphicsPipelineLayouts();
	bool CreateGraphicsPipelines();
	bool CreateDescriptorPool();
	bool CreateDescriptorSets();
	bool CreatePassBuffers();
	bool CreateSyncObjects();

	bool RecreateColorResources();
	bool RecreateDepthResources();
	bool RecreateFramebuffers();
	bool RecreateGraphicsPipelines();

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

	bool DrawShadowMap();
	bool DrawBackBuffer();
	bool DrawDebuggingInfo();

protected:
	const VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	const std::uint32_t ShadowMapSize = 2048;

	static const std::uint32_t CommandPoolCount = 3;

private:
	bool bIsCleanedUp;

	bool bFramebufferResized;
	
	std::unordered_map<std::string, VkRenderPass> mRenderPasses;

	std::array<VkCommandPool, CommandPoolCount> mCommandPools;
	std::array<std::array<VkCommandBuffer, ECommandBufferLayout::ECBL_Count>, CommandPoolCount> mCommandBuffers;

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;

	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	VkImage mShadowImage;
	VkDeviceMemory mShadowImageMemory;
	VkImageView mShadowDepthImageView;
	VkSampler mShadowImageSampler;

	std::unordered_map < std::string, std::vector<VkFramebuffer>> mFramebuffers;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorPool mDescriptorPool;

	std::array<VkDescriptorSet, SwapChainImageCount> mDebugDescriptorSets;

	std::unordered_map<std::string, VkPipelineLayout> mPipelineLayouts;
	std::unordered_map<std::string, VkPipeline> mGraphicsPipelines;

	std::array<VkSemaphore, SwapChainImageCount> mImageAvailableSemaphores;
	std::array<VkSemaphore, SwapChainImageCount> mRenderFinishedSemaphores;
	std::array<VkFence, SwapChainImageCount> mInFlightFences;
	std::array<VkFence, SwapChainImageCount> mImagesInFlight;
	std::uint32_t mCurentImageIndex;
	size_t mCurrentFrame;

	std::unordered_map<std::string, std::unique_ptr<MeshData>> mMeshes;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[ERenderTypes::ENumTypes];

	std::vector<VkBuffer> mMainPassBuffers;
	std::vector<VkDeviceMemory> mMainPassMemories;

	std::vector<VkBuffer> mShadowPassBuffers;
	std::vector<VkDeviceMemory> mShadowPassMemories;

	DirectX::BoundingSphere mSceneBounds;
	DirectX::XMFLOAT3 mLightDir;

	VkExtent2D mShadowExtent;

	std::uint32_t mCurrCommandPoolIndex;
};