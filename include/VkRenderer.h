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

		UINT			IndexCount;
	};

	struct MaterialData {
		VkImage			TextureImage;
		VkDeviceMemory	TextureImageMemory;
		VkImageView		TextureImageView;
		VkSampler		TextureSampler;

		UINT			 MipLevels;
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
		FLOAT				PassConstantsPad1;
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

		INT NumFramesDirty = SwapChainImageCount;

		BOOL Visible = true;
	};

	enum EDescriptorSetLayout {
		EDSL_Object = 0,
		EDSL_Pass,
		EDSL_Output,
		EDSL_Shadow,
		EDSL_Count
	};

public:
	VkRenderer();
	virtual ~VkRenderer();

public:
	virtual BOOL Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) override;
	virtual void CleanUp() override;

	virtual BOOL Update(FLOAT delta) override;
	virtual BOOL Draw() override;

	virtual BOOL OnResize(UINT width, UINT height) override;

	virtual void* AddModel(const std::string& file, const Transform& trans, RenderType::Type type = RenderType::E_Opaque) override;
	virtual void RemoveModel(void* model) override;
	virtual void UpdateModel(void* model, const Transform& trans) override;
	virtual void SetModelVisibility(void* model, BOOL visible) override;
	virtual void SetModelPickable(void* model, BOOL pickable) override;

	virtual BOOL SetCubeMap(const std::string& file) override;
	virtual BOOL SetEquirectangularMap(const std::string& file) override;

	virtual void Pick(FLOAT x, FLOAT y) override;

protected:
	virtual BOOL RecreateSwapChain() override;
	virtual void CleanUpSwapChain() override;

private:
	BOOL CreateRenderPass();
	BOOL CreateCommandPool();
	BOOL CreateColorResources();
	BOOL CreateDepthResources();
	BOOL CreateFramebuffers();
	BOOL CreateDescriptorSetLayout();
	BOOL CreateGraphicsPipelineLayouts();
	BOOL CreateGraphicsPipelines();
	BOOL CreateDescriptorPool();
	BOOL CreatePassBuffers();
	BOOL CreateCommandBuffers();
	BOOL CreateSyncObjects();

	BOOL RecreateColorResources();
	BOOL RecreateDepthResources();
	BOOL RecreateFramebuffers();
	BOOL RecreateGraphicsPipelines();

	BOOL AddGeometry(const std::string& file);
	BOOL AddMaterial(const std::string& file, const Material& material);
	void* AddRenderItem(const std::string& file, const Transform& trans, RenderType::Type type);

	BOOL CreateTextureImage(INT width, INT height, void* data, MaterialData* mat);
	BOOL CreateTextureImageView(MaterialData* mat);
	BOOL CreateTextureSampler(MaterialData* mat);

	BOOL CreateUniformBuffers(RenderItem* ritem);
	BOOL CreateDescriptorSets(RenderItem* ritem);

	BOOL UpdateUniformBuffer(FLOAT delta);
	BOOL UpdateDescriptorSet(const RenderItem* ritem);

	BOOL DrawShadowMap();
	BOOL DrawBackBuffer();

protected:
	const VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	const UINT ShadowMapSize = 2048;

private:
	BOOL bIsCleanedUp;

	BOOL bFramebufferResized;

	VkRenderPass mRenderPass;
	VkRenderPass mShadowRenderPass;

	VkCommandPool mCommandPool;
	std::array<VkCommandBuffer, SwapChainImageCount> mCommandBuffers;

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

	std::array<VkFramebuffer, SwapChainImageCount> mSwapChainFramebuffers;
	VkFramebuffer mShadowFramebuffer;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorPool mDescriptorPool;

	std::unordered_map<std::string, VkPipelineLayout> mPipelineLayouts;
	std::unordered_map<std::string, VkPipeline> mGraphicsPipelines;

	std::array<VkSemaphore, SwapChainImageCount> mImageAvailableSemaphores;
	std::array<VkSemaphore, SwapChainImageCount> mRenderFinishedSemaphores;
	std::array<VkFence, SwapChainImageCount> mInFlightFences;
	std::array<VkFence, SwapChainImageCount> mImagesInFlight;
	UINT mCurentImageIndex;
	size_t mCurrentFrame;

	std::unordered_map<std::string, std::unique_ptr<MeshData>> mMeshes;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[RenderType::Type::Count];

	std::vector<VkBuffer> mMainPassBuffers;
	std::vector<VkDeviceMemory> mMainPassMemories;

	std::vector<VkBuffer> mShadowPassBuffers;
	std::vector<VkDeviceMemory> mShadowPassMemories;

	DirectX::BoundingSphere mSceneBounds;
	DirectX::XMFLOAT3 mLightDir;
};