#pragma once

#include <Windows.h>

const INT gNumFrameResources = 3;

#include "DxLowRenderer.h"
#include "MathHelper.h"
#include "HlslCompaction.h"
#include "RenderItem.h"
#include "DxMesh.h"

struct FrameResource;

class ShaderManager;
class ImGuiManager; 
class AccelerationStructureBuffer;

#include "DxForwardDeclarations.h"

namespace DebugMapLayout {
	enum Type {
		// G-Buffer
		E_Albedo = 0,
		E_Normal,
		E_Depth,
		E_RMS,
		E_Velocity,

		E_SSAO,
		E_Bloom,
		E_SSR,

		// Irradiance
		E_DiffuseReflectance,
		E_SpecularReflectance,
		E_TemporaryEquirectangular,
		E_Equirectangular,
		E_DiffuseIrradianceEquirect,

		E_DxrShadow,

		// RTAO
		E_AOCoeff,
		E_TemporalAOCoeff,
		E_Tspp,
		E_RayHitDist,
		E_TemporalRayHitDist,

		// Raytraced Reflection
		E_RR_Reflection,
		E_RR_TemporalReflection,
		E_RR_Tspp,
		E_RR_RayHitDist,
		E_RR_TemporalRayHitDist,

		Count
	};
}

class DxRenderer : public Renderer, public DxLowRenderer {
public:
	DxRenderer();
	virtual ~DxRenderer();

public:
	virtual BOOL Initialize(HWND hwnd, void* glfwWnd, UINT width, UINT height) override;
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

	virtual BOOL CreateRtvAndDsvDescriptorHeaps();

	BOOL AddGeometry(const std::string& file);
	BOOL AddMaterial(const std::string& file, const Material& material);
	void* AddRenderItem(const std::string& file, const Transform& trans, RenderType::Type type);

	UINT AddTexture(const std::string& file, const Material& material);

private:


private:
	BOOL CompileShaders();
	BOOL BuildGeometries();

	BOOL BuildFrameResources();
	void BuildDescriptors();
	BOOL BuildRootSignatures();
	BOOL BuildPSOs();
	void BuildRenderItems();

	BOOL UpdateShadingObjects(FLOAT delta);
	BOOL UpdateCB_Main(FLOAT delta);
	BOOL UpdateCB_SSAO(FLOAT delta);
	BOOL UpdateCB_Blur(FLOAT delta);
	BOOL UpdateCB_DoF(FLOAT delta);
	BOOL UpdateCB_SSR(FLOAT delta);
	BOOL UpdateCB_Objects(FLOAT delta);
	BOOL UpdateCB_Materials(FLOAT delta);
	BOOL UpdateCB_Irradiance(FLOAT delta);
	BOOL UpdateCB_SVGF(FLOAT delta);
	BOOL UpdateCB_RTAO(FLOAT delta);
	BOOL UpdateCB_DebugMap(FLOAT delta);

	BOOL AddBLAS(ID3D12GraphicsCommandList4* const cmdList, MeshGeometry* const geo);
	BOOL BuildTLAS(ID3D12GraphicsCommandList4* const cmdList);
	BOOL UpdateTLAS(ID3D12GraphicsCommandList4* const cmdList);

	BOOL BuildShaderTables();
	
	BOOL DrawShadow();
	BOOL DrawGBuffer();
	BOOL DrawSSAO();
	BOOL DrawBackBuffer();
	BOOL IntegrateSpecIrrad();
	BOOL DrawSkySphere();
	BOOL DrawEquirectangulaToCube();
	BOOL ApplyTAA();
	BOOL ApplySSR();
	BOOL ApplyBloom();
	BOOL ApplyDepthOfField();
	BOOL ApplyMotionBlur();
	BOOL ResolveToneMapping();
	BOOL ApplyGammaCorrection();
	BOOL ApplySharpen();
	BOOL ApplyPixelation();
	BOOL DrawDebuggingInfo();
	BOOL DrawImGui();

	BOOL DrawDXRShadow();
	BOOL DrawDXRBackBuffer();
	BOOL CalcDepthPartialDerivative();
	BOOL DrawRTAO();
	BOOL BuildRaytracedReflection();

private:
	BOOL bIsCleanedUp;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource;
	INT mCurrFrameResourceIndex;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	std::vector<std::unique_ptr<RenderItem>> mRitems;
	std::vector<RenderItem*> mRitemRefs[RenderType::Count];

	UINT mCurrDescriptorIndex = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDescForTexMaps;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuDescForTexMaps;

	std::unique_ptr<ConstantBuffer_Pass> mMainPassCB;
	
	std::unique_ptr<ShaderManager> mShaderManager;

	DirectX::BoundingSphere mSceneBounds;

	std::unique_ptr<ImGuiManager> mImGui;

	std::unique_ptr<BRDF::BRDFClass> mBRDF;
	std::unique_ptr<GBuffer::GBufferClass> mGBuffer;
	std::unique_ptr<Shadow::ShadowClass> mShadow;
	std::unique_ptr<ZDepth::ZDepthClass> mZDepth;
	std::unique_ptr<SSAO::SSAOClass> mSSAO;
	std::unique_ptr<BlurFilter::BlurFilterClass> mBlurFilter;
	std::unique_ptr<Bloom::BloomClass> mBloom;
	std::unique_ptr<SSR::SSRClass> mSSR;
	std::unique_ptr<DepthOfField::DepthOfFieldClass> mDoF;
	std::unique_ptr<MotionBlur::MotionBlurClass> mMotionBlur;
	std::unique_ptr<TemporalAA::TemporalAAClass> mTAA;
	std::unique_ptr<DebugMap::DebugMapClass> mDebugMap;
	std::unique_ptr<DebugCollision::DebugCollisionClass> mDebugCollision;
	std::unique_ptr<GammaCorrection::GammaCorrectionClass> mGammaCorrection;
	std::unique_ptr<ToneMapping::ToneMappingClass> mToneMapping;
	std::unique_ptr<IrradianceMap::IrradianceMapClass> mIrradianceMap;
	std::unique_ptr<MipmapGenerator::MipmapGeneratorClass> mMipmapGenerator;
	std::unique_ptr<Pixelation::PixelationClass> mPixelation;
	std::unique_ptr<Sharpen::SharpenClass> mSharpen;
	std::unique_ptr<GaussianFilter::GaussianFilterClass> mGaussianFilter;
	std::unique_ptr<RaytracedReflection::RaytracedReflectionClass> mRR;
	std::unique_ptr<SVGF::SVGFClass> mSVGF;
	std::unique_ptr<EquirectangularConverter::EquirectangularConverterClass> mEquirectangularConverter;
	std::unique_ptr<VolumetricLight::VolumetricLightClass> mVolumetricLight;

	std::array<DirectX::XMFLOAT4, 3> mBlurWeights;

	BOOL bShadowMapCleanedUp = FALSE;
	BOOL bSsaoMapCleanedUp = FALSE;
	BOOL bSsrMapCleanedUp = FALSE;

	std::array<DirectX::XMFLOAT2, 16> mHaltonSequence;
	std::array<DirectX::XMFLOAT2, 16> mFittedToBakcBufferHaltonSequence;

	std::unordered_map<DebugMapLayout::Type, BOOL> mDebugMapStates;

	RenderItem* mPickedRitem = nullptr;
	RenderItem* mSkySphere = nullptr;

	BOOL bNeedToUpdate_Irrad = TRUE;

	//
	// DirectXTK12
	//
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;	

	//
	// DXR
	//
	std::vector<std::unique_ptr<AccelerationStructureBuffer>> mBLASes;
	std::unordered_map<std::string, AccelerationStructureBuffer*> mBLASRefs;
	std::unique_ptr<AccelerationStructureBuffer> mTLAS;
	BOOL bNeedToRebuildTLAS = true;
	BOOL bNeedToRebuildShaderTables = true;

	std::unique_ptr<DXR_Shadow::DXR_ShadowClass> mDxrShadow;
	std::unique_ptr<BlurFilter::CS::BlurFilterCSClass> mBlurFilterCS;
	std::unique_ptr<RTAO::RTAOClass> mRTAO;

	Light mLights[MaxLights];
	UINT mLightCount = 0;
};