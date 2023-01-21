#pragma once

#include "DxLowRenderer.h"

struct FrameResource;
struct PassConstants;

class ShaderManager;
class ShadowMap;
class GBuffer;
class Ssao;
class TemporalAA;
class MotionBlur;
class DepthOfField;
class Bloom;

class DxRenderer : public Renderer, public DxLowRenderer {
public:
	// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
	// geometries are stored in one vertex and index buffer.  It provides the offsets
	// and data needed to draw a subset of geometry stores in the vertex and index 
	// buffers so that we can implement the technique described by Figure 6.3.
	struct SubmeshGeometry {
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		INT BaseVertexLocation = 0;

		// Bounding box of the geometry defined by this submesh. 
		// This is used in later chapters of the book.
		DirectX::BoundingBox AABB;
	};

	struct MeshGeometry {
		// Give it a name so we can look it up by name.
		std::string Name;

		// System memory copies.  Use Blobs because the vertex/index format can be generic.
		// It is up to the client to cast appropriately.  
		Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

		// Data about the buffers.
		UINT VertexByteStride = 0;
		UINT VertexBufferByteSize = 0;
		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
		UINT IndexBufferByteSize = 0;

		// A MeshGeometry may store multiple geometries in one vertex/index buffer.
		// Use this container to define the Submesh geometries so we can draw
		// the Submeshes individually.
		std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.StrideInBytes = VertexByteStride;
			vbv.SizeInBytes = VertexBufferByteSize;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		// We can free this memory after we finish upload to the GPU.
		void DisposeUploaders() {
			VertexBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
	};

	struct Texture {
		UINT DescriptorIndex = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};

	struct MaterialData {
		// Index into constant buffer corresponding to this material.
		int MatCBIndex = -1;

		// Index into SRV heap for diffuse texture.
		int DiffuseSrvHeapIndex = -1;

		// Index into SRV heap for normal texture.
		int NormalSrvHeapIndex = -1;

		// Index into SRV heap for alpha texture.
		int AlphaSrvHeapIndex = -1;

		// Dirty flag indicating the material has changed and we need to update the constant buffer.
		// Because we have a material constant buffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify a material we should set 
		// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
		int NumFramesDirty = gNumFrameResources;

		// Material constant buffer data used for shading.
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.5f, 0.5f, 0.5f };
		float Roughness = 0.5f;
		DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
	};

	struct RenderItem {
		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		int ObjCBIndex = -1;

		// World matrix of the shape that describes the object's local space
		// relative to the world space, which defines the position, orientation,
		// and scale of the object in thw world.
		DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 PrevWolrd = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

		// Dirty flag indicating the object data has changed and we need to update the constant buffer.
		// Because we have an object cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource. Thus, when we modify object data we should set
		// NumFrameDirty = gNumFrameResources so that each frame resource gets the update.
		int NumFramesDirty = gNumFrameResources << 1;

		MaterialData* Material = nullptr;
		MeshGeometry* Geometry = nullptr;

		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		DirectX::BoundingBox AABB;

		// DrawIndexedInstanced parameters.
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		UINT BaseVertexLocation = 0;
	};

	enum EDefaultRootSignatureLayout {
		EDRS_ObjectCB = 0,
		EDRS_PassCB,
		EDRS_MatCB,
		EDRS_ReservedTexMaps,
		EDRS_AllocatedTexMaps,
		EDRS_Count
	};

	enum ESsaoRootSignatureLayout {
		ESRS_PassCB = 0,
		ESRS_NormalDepth,
		ESRS_RandomVector,
		ESRS_Count
	};

	enum EBlurRootSignatureLayout {
		EBRS_BlurCB = 0,
		EBRS_Consts,
		EBRS_NormalDepth,
		EBRS_Input,
		EBRS_Count
	};

	enum EBlurRootConstantsLayout {
		EBRC_HorizontalBlur = 0,
		EBRC_Count
	};

	enum ETaaRootSignatureLayout {
		ETRS_Input = 0,
		ETRS_History,
		ETRS_Velocity,
		ERTS_Factor,
		ETRS_Count
	};

	enum EMotionBlurRootSignatureLayout {
		EMBRS_Input = 0,
		EMBRS_Depth,
		EMBRS_Velocity,
		EMBRS_Consts,
		EMBRS_Count
	};

	enum EMotionBlurRootConstantsLayout {
		EMBRC_Intensity = 0,
		EMBRC_Limit,
		EMBRS_DepthBias,
		EMBRS_NumSamples,
		EMBRC_Count
	};

	enum EMappingRootSignatureLayout {
		EMRS_Input = 0,
		EMRS_Count
	};

	enum ECocRootSignatureLayout {
		ECRS_DofCB = 0,
		ECRS_Depth,
		ECRS_FocalDist,
		ECRS_Count
	};
	
	enum EBokehRootSignatureLayout {
		EBKHRS_Input = 0,
		EBKHRS_Consts,
		EBKHRS_Count
	};

	enum EBokehRootConstantsLayout {
		EBKHRCS_BokehRadius = 0,
		EBKHRCS_Count
	};

	enum EDofRootSignatureLayout {
		EDOFRS_Consts = 0,
		EDOFRS_BackBuffer,
		EDOFRS_Coc,
		EDOFRS_Count
	};

	enum EDofRootConstantLayout {
		EDRC_BokehRadius = 0,
		EDRC_CocThreshold,
		EDRC_CocDiffThreshold,
		EDRC_HighlightPower,
		EDRC_NumSamples,
		EDRC_Count
	};

	enum EDofBlurRootSignatureLayout {
		EDBRS_BlurCB = 0,
		EDBRS_Consts,
		EDBRS_Input,
		EDBRS_Coc,
		EDBRS_Count
	};
	
	enum EDofBlurRootConstantLayout {
		EDBRC_HorizontalBlur = 0,
		EDBRC_Count
	};

	enum EExtHlightsRootSignatureLayout {
		EEHRS_BackBuffer = 0,
		EEHRS_Consts,
		EEHRS_Count
	};

	enum EExtHlightsRootConstatLayout {
		EEHRC_Threshold = 0,
		EEHRC_Count
	};

	enum EBloomRootSignatureLayout {
		EBLMRS_BackBuffer = 0,
		EBLMRS_Bloom,
		EBLMRS_Count
	};

	enum EFocalDistanceRootSignatureLayout {
		EFDRS_DofCB = 0,
		EFDRS_Depth,
		EFDRS_FocalDist,
		EFDRS_Count
	};

	enum ERtvHeapLayout {
		ERHL_BackBuffer0 = 0,
		ERHL_BackBuffer1,
		ERHL_Color,
		ERHL_Albedo,
		ERHL_Normal,
		ERHL_Specular,
		ERHL_Velocity,
		ERHL_Ambient0,
		ERHL_Ambient1,
		ERHL_Resolve,
		ERHL_MotionBlur,
		ERHL_Coc,
		ERHL_Dof,
		ERHL_DofBlur,
		ERHL_Bloom0,
		ERHL_Bloom1,
		ERHL_BloomTemp,
		ERHL_Count
	};

	enum EDsvHeapLayout {
		EDHL_Default = 0,
		EDHL_Shadow,
		EDHL_Count
	};

	enum ECbvSrvUavHeapLayout {
		ECSUHL_Srvs = 0,
		ECSUHL_Uavs = 64,
		ECSUHL_Count = 128
	};

	enum EReservedSrvs {
		ERS_Cube = ECbvSrvUavHeapLayout::ECSUHL_Srvs,
		ERS_Color,
		ERS_Albedo,
		ERS_Normal,
		ERS_Depth,
		ERS_Specular,
		ERS_Velocity,
		ERS_Shadow,
		ERS_Ambient0,
		ERS_Ambient1,
		ERS_RandomVector,
		ERS_Resolve,
		ERS_History,
		ERS_BackBuffer0,
		ERS_BackBuffer1,
		ERS_Coc,
		ERS_Dof,
		ERS_DofBlur,
		ERS_Bloom0,
		ERS_Bloom1,
		ERS_Font,
		ERS_Count = (EReservedSrvs::ERS_Font - EReservedSrvs::ERS_Cube) + 1
	};

	enum EReservedUavs {
		ERU_FocalDist = ECbvSrvUavHeapLayout::ECSUHL_Uavs,
		ERU_Count = (EReservedUavs::ERU_FocalDist - EReservedUavs::ERU_FocalDist) + 1
	};

public:
	DxRenderer();
	virtual ~DxRenderer();

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

	virtual bool SetCubeMap(const std::string& file) override;

	virtual bool CreateRtvAndDsvDescriptorHeaps();

	bool AddGeometry(const std::string& file);
	bool AddMaterial(const std::string& file, const Material& material);
	void* AddRenderItem(const std::string& file, const Transform& trans, ERenderTypes type);

	UINT AddTexture(const std::string& file, const Material& material);

private:
	bool InitImGui();
	void CleanUpImGui();

	bool CompileShaders();
	bool BuildGeometries();

	bool BuildFrameResources();
	bool BuildDescriptorHeaps();
	void BuildDescriptors();
	void BuildBackBufferDescriptors();
	bool BuildRootSignatures();
	bool BuildPSOs();
	void BuildRenderItems();

	bool UpdateShadowPassCB(float delta);
	bool UpdateMainPassCB(float delta);
	bool UpdateSsaoPassCB(float delta);
	bool UpdateBlurPassCB(float delta);
	bool UpdateDofCB(float delta);
	bool UpdateObjectCBs(float delta);
	bool UpdateMaterialCBs(float delta);

	void DrawRenderItems(const std::vector<RenderItem*>& ritems);

	bool DrawShadowMap();
	bool DrawGBuffer();
	bool DrawSsao();
	bool DrawBackBuffer();
	bool DrawSkyCube();
	bool ApplyTAA();
	bool ApplyBloom();
	bool ApplyDepthOfField();
	bool ApplyMotionBlur();
	bool DrawDebuggingInfo();
	bool DrawImGui();

protected:
	static const int gNumFrameResources = 3;

private:
	bool bIsCleanedUp;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource;
	int mCurrFrameResourceIndex;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavHeap;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[ERenderTypes::ENumTypes];

	UINT mCurrDescriptorIndex;

	std::unique_ptr<PassConstants> mMainPassCB;
	std::unique_ptr<PassConstants> mShadowPassCB;
	
	std::unique_ptr<ShaderManager> mShaderManager;

	std::unique_ptr<ShadowMap> mShadowMap;
	DirectX::BoundingSphere mSceneBounds;
	DirectX::XMFLOAT3 mLightDir;

	std::unique_ptr<GBuffer> mGBuffer;
	std::unique_ptr<Ssao> mSsao;
	std::unique_ptr<TemporalAA> mTaa;
	std::unique_ptr<MotionBlur> mMotionBlur;
	std::unique_ptr<DepthOfField> mDof;
	std::unique_ptr<Bloom> mBloom;

	std::array<DirectX::XMFLOAT4, 3> mBlurWeights;

	bool bShadowMapCleanedUp;
	bool bSsaoMapCleanedUp;

	std::array<DirectX::XMFLOAT2, 16> mHaltonSequence;
	std::array<DirectX::XMFLOAT2, 16> mFittedToBakcBufferHaltonSequence;

	int mNumSsaoBlurs;

	float mTaaModulationFactor;

	float mMotionBlurIntensity;
	float mMotionBlurLimit;
	float mMotionBlurDepthBias;
	int mNumMotionBlurSamples;

	float mFocusRange;
	float mFocusingSpeed;

	float mBokehRadius;
	float mCocThreshold;
	float mCocDiffThreshold;
	float mHighlightPower;
	int mNumDofSamples;
	int mNumDofBlurs;

	int mNumBloomBlurs;
	float mHighlightThreshold;

	//
	// DirectXTK12
	//
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;	
};