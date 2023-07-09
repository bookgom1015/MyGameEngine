#pragma once

const int gNumFrameResources = 3;

#include "DxLowRenderer.h"
#include "MathHelper.h"
#include "HlslCompaction.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "AccelerationStructure.h"

struct FrameResource;
struct PassConstants;

class ShaderManager;
class ImGuiManager;

namespace BackBuffer { class BackBufferClass; }
namespace GBuffer { class GBufferClass; }
namespace ShadowMap { class ShadowMapClass; }
namespace Ssao { class SsaoClass; }
namespace BlurFilter { class BlurFilterClass; }
namespace Bloom { class BloomClass; }
namespace Ssr { class SsrClass; }
namespace DepthOfField { class DepthOfFieldClass; }
namespace MotionBlur { class MotionBlurClass; }
namespace TemporalAA { class TemporalAAClass; }
namespace Debug { class DebugClass; }
namespace SkyCube { class SkyCubeClass; }
namespace DxrShadowMap { class DxrShadowMapClass; }

namespace EMappingRootSignatureLayout {
	enum {
		EInput = 0,
		Count
	};
}

namespace ERtvHeapLayout {
	enum {
		EBackBuffer0 = 0,
		EBackBuffer1,
		EColor,
		EAlbedo,
		ENormal,
		ESpecular,
		EVelocity,
		EAmbient0,
		EAmbient1,
		EResolve,
		EMotionBlur,
		ECoc,
		EDof,
		EDofBlur,
		EBloom0,
		EBloom1,
		EBloomTemp,
		ESsr0,
		ESsr1,
		ESsrTemp,
		Count
	};
}

namespace EDsvHeapLayout {
	enum {
		EDefault = 0,
		EShadow,
		Count
	};
}

namespace DebugMapLayout {
	enum Type {
		EColor = 0,
		EAlbedo,
		ENormal,
		EDepth,
		ESpecular,
		EVelocity,
		EShadow,
		ESsao,
		EBloom,
		ESsr
	};
}

class DxRenderer : public Renderer, public DxLowRenderer {
public:
	DxRenderer();
	virtual ~DxRenderer();

public:
	virtual bool Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) override;
	virtual void CleanUp() override;

	virtual bool Update(float delta) override;
	virtual bool Draw() override;

	virtual bool OnResize(UINT width, UINT height) override;

	virtual void* AddModel(const std::string& file, const Transform& trans, RenderType::Type type = RenderType::EOpaque) override;
	virtual void RemoveModel(void* model) override;
	virtual void UpdateModel(void* model, const Transform& trans) override;
	virtual void SetModelVisibility(void* model, bool visible) override;

	virtual bool SetCubeMap(const std::string& file) override;

	virtual bool CreateRtvAndDsvDescriptorHeaps();

	bool AddGeometry(const std::string& file);
	bool AddMaterial(const std::string& file, const Material& material);
	void* AddRenderItem(const std::string& file, const Transform& trans, RenderType::Type type);

	bool AddBLAS(ID3D12GraphicsCommandList4*const cmdList, MeshGeometry*const geo);
	bool BuildTLASs(ID3D12GraphicsCommandList4*const cmdList);

	UINT AddTexture(const std::string& file, const Material& material);

private:
	bool CompileShaders();
	bool BuildGeometries();

	bool BuildFrameResources();
	void BuildDescriptors();
	bool BuildRootSignatures();
	bool BuildPSOs();
	bool BuildShaderTables();
	void BuildRenderItems();

	bool UpdateShadowPassCB(float delta);
	bool UpdateMainPassCB(float delta);
	bool UpdateSsaoPassCB(float delta);
	bool UpdateBlurPassCB(float delta);
	bool UpdateDofCB(float delta);
	bool UpdateSsrCB(float delta);
	bool UpdateObjectCBs(float delta);
	bool UpdateMaterialCBs(float delta);
	
	bool DrawShadowMap();
	bool DrawGBuffer();
	bool DrawSsao();
	bool DrawBackBuffer();
	bool DrawSkyCube();
	bool ApplyTAA();
	bool ApplySsr();
	bool ApplyBloom();
	bool ApplyDepthOfField();
	bool ApplyMotionBlur();
	bool DrawDebuggingInfo();
	bool DrawImGui();

private:
	bool bIsCleanedUp;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource;
	int mCurrFrameResourceIndex;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[RenderType::Count];

	UINT mCurrDescriptorIndex;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDescForTexMaps;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuDescForTexMaps;

	std::unique_ptr<PassConstants> mMainPassCB;
	std::unique_ptr<PassConstants> mShadowPassCB;
	
	std::unique_ptr<ShaderManager> mShaderManager;

	DirectX::BoundingSphere mSceneBounds;
	DirectX::XMFLOAT3 mLightDir;

	std::unique_ptr<ImGuiManager> mImGui;

	std::unique_ptr<BackBuffer::BackBufferClass> mBackBuffer;
	std::unique_ptr<GBuffer::GBufferClass> mGBuffer;
	std::unique_ptr<ShadowMap::ShadowMapClass> mShadowMap;
	std::unique_ptr<Ssao::SsaoClass> mSsao;
	std::unique_ptr<BlurFilter::BlurFilterClass> mBlurFilter;
	std::unique_ptr<Bloom::BloomClass> mBloom;
	std::unique_ptr<Ssr::SsrClass> mSsr;
	std::unique_ptr<DepthOfField::DepthOfFieldClass> mDof;
	std::unique_ptr<MotionBlur::MotionBlurClass> mMotionBlur;
	std::unique_ptr<TemporalAA::TemporalAAClass> mTaa;
	std::unique_ptr<Debug::DebugClass> mDebug;
	std::unique_ptr<SkyCube::SkyCubeClass> mSkyCube;

	std::array<DirectX::XMFLOAT4, 3> mBlurWeights;

	bool bShadowMapCleanedUp;
	bool bSsaoMapCleanedUp;
	bool bSsrMapCleanedUp;

	std::array<DirectX::XMFLOAT2, 16> mHaltonSequence;
	std::array<DirectX::XMFLOAT2, 16> mFittedToBakcBufferHaltonSequence;

	std::unordered_map<DebugMapLayout::Type, bool> mDebugMapStates;

	//
	// DirectXTK12
	//
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;	

	//
	// DXR
	//
	std::unordered_map<std::string, std::unique_ptr<AccelerationStructureBuffer>> mBLASs;
	std::unique_ptr<AccelerationStructureBuffer> mTLAS;

	std::unique_ptr<DxrShadowMap::DxrShadowMapClass> mDxrShadowMap;
};