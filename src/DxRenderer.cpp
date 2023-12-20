#include "DxRenderer.h"
#include "Logger.h"
#include "FrameResource.h"
#include "MathHelper.h"
#include "MeshImporter.h"
#include "GeometryGenerator.h"
#include "ShaderManager.h"
#include "ShadowMap.h"
#include "GBuffer.h"
#include "Ssao.h"
#include "Blur.h"
#include "TemporalAA.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "Bloom.h"
#include "Ssr.h"
#include "BRDF.h"
#include "Samplers.h"
#include "BlurFilter.h"
#include "DebugMap.h"
#include "ImGuiManager.h"
#include "DxrShadowMap.h"
#include "DxrGeometryBuffer.h"
#include "BlurFilterCS.h"
#include "Rtao.h"
#include "DebugCollision.h"
#include "GammaCorrection.h"
#include "ToneMapping.h"
#include "IrradianceMap.h"
#include "MipmapGenerator.h"
#include "Pixelation.h"
#include "Sharpen.h"
#include "GaussianFilter.h"
#include "RaytracedReflection.h"
#include "AccelerationStructure.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

#undef max
#undef min

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";
}

namespace ShaderArgs {
	namespace Bloom {
		FLOAT HighlightThreshold = 0.99f;
		INT BlurCount = 3;
	}

	namespace DepthOfField {
		FLOAT FocusRange = 8.0f;
		FLOAT FocusingSpeed = 8.0f;
		FLOAT BokehRadius = 2.0f;
		FLOAT CocThreshold = 0.3f;
		FLOAT CocDiffThreshold = 0.8f;
		FLOAT HighlightPower = 4.0f;
		INT SampleCount = 4;
		INT BlurCount = 1;
	}

	namespace Ssr {
		FLOAT MaxDistance = 20.0f;
		FLOAT RayLength = 0.5f;
		FLOAT NoiseIntensity = 0.01f;
		INT StepCount = 16;
		INT BackStepCount = 8;
		INT BlurCount = 3;
		FLOAT DepthThreshold = 1.0f;
	}

	namespace MotionBlur {
		FLOAT Intensity = 0.01f;
		FLOAT Limit = 0.005f;
		FLOAT DepthBias = 0.05f;
		INT SampleCount = 10;
	}

	namespace TemporalAA {
		FLOAT ModulationFactor = 0.8f;
	}

	namespace Ssao {
		INT SampleCount = 14;
		INT BlurCount = 3;
	}

	namespace ToneMapping {
		FLOAT Exposure = 1.4f;
	}

	namespace GammaCorrection {
		FLOAT Gamma = 2.2f;
	}

	namespace DxrShadowMap {
		INT BlurCount = 3;
	}

	namespace Debug {
		BOOL ShowCollisionBox = false;
	}

	namespace Light {
		FLOAT AmbientLight[3] = { 0.164f, 0.2f, 0.235f };

		namespace DirectionalLight {
			FLOAT Strength[3] = { 0.9568f, 0.9411f, 0.8941f };
			FLOAT Multiplier = 2.885f;
		}
	}

	namespace IrradianceMap {
		BOOL ShowIrradianceCubeMap = false;
		FLOAT MipLevel = 0.0f;
	}

	namespace Pixelization {
		FLOAT PixelSize = 5.0f;
	}

	namespace Sharpen {
		FLOAT Amount = 0.8f;
	}

	namespace Rtao {
		BOOL CheckerboardSamplingEnabled = false;
		BOOL CheckerboardGenerateRaysForEvenPixels = false;

		FLOAT OcclusionRadius = 10.0f;
		FLOAT OcclusionFadeStart = 1.0f;
		FLOAT OcclusionFadeEnd = 100.0f;
		FLOAT OcclusionEpsilon = 0.05f;
		UINT SampleCount = 2;
		BOOL QuarterResolutionAO = false;
		FLOAT MaxRayHitTime = 22.0f;

		namespace Denoiser {
			BOOL UseSmoothingVariance = true;
			BOOL DisocclusionBlur = true;
			UINT LowTsppBlurPasses = 3;

			namespace TemporalSupersampling {
				UINT MaxTspp = 33;

				namespace ClampCachedValues {
					BOOL UseClamping = true;
					FLOAT StdDevGamma = 0.6f;
					FLOAT MinStdDevTolerance = 0.05f;
					FLOAT DepthSigma = 1.0f;
				}

				FLOAT ClampDifferenceToTsppScale = 4.0f;
				UINT MinTsppToUseTemporalVariance = 4;
				UINT LowTsppMaxTspp = 12;
				FLOAT LowTsppDecayConstant = 1.0f;
			}

			namespace AtrousWaveletTransformFilter {
				FLOAT ValueSigma = 1.0f;
				FLOAT DepthSigma = 1.0f;
				FLOAT DepthWeightCutoff = 0.2f;
				FLOAT NormalSigma = 64.0f;
				FLOAT MinVarianceToDenoise = 0.0f;
				BOOL UseSmoothedVariance = false;
				BOOL PerspectiveCorrectDepthInterpolation = true;
				BOOL UseAdaptiveKernelSize = true;
				BOOL KernelRadiusRotateKernelEnabled = true;
				INT KernelRadiusRotateKernelNumCycles = 3;
				INT FilterMinKernelWidth = 3;
				FLOAT FilterMaxKernelWidthPercentage = 1.5f;
				FLOAT AdaptiveKernelSizeRayHitDistanceScaleFactor = 0.02f;
				FLOAT AdaptiveKernelSizeRayHitDistanceScaleExponent = 2.0f;

			}
		}
	}

	namespace RaytracedReflection {
		FLOAT ReflectionRadius = 10.0f;
	}
}

DxRenderer::DxRenderer() {
	bIsCleanedUp = false;

	mMainPassCB = std::make_unique<PassConstants>();
	mShadowPassCB = std::make_unique<PassConstants>();

	mShaderManager = std::make_unique<ShaderManager>();

	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	FLOAT widthSquared = 32.0f * 32.0f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);
	mLightDir = { 0.57735f, -0.57735f, 0.57735f };

	mImGui = std::make_unique<ImGuiManager>();

	mBRDF = std::make_unique<BRDF::BRDFClass>();
	mGBuffer = std::make_unique<GBuffer::GBufferClass>();
	mShadowMap = std::make_unique<ShadowMap::ShadowMapClass>();
	mSsao = std::make_unique<Ssao::SsaoClass>();
	mBlurFilter = std::make_unique<BlurFilter::BlurFilterClass>();
	mBloom = std::make_unique<Bloom::BloomClass>();
	mSsr = std::make_unique<Ssr::SsrClass>();
	mDof = std::make_unique<DepthOfField::DepthOfFieldClass>();
	mMotionBlur = std::make_unique<MotionBlur::MotionBlurClass>();
	mTaa = std::make_unique<TemporalAA::TemporalAAClass>();
	mDebugMap = std::make_unique<DebugMap::DebugMapClass>();
	mDebugCollision = std::make_unique<DebugCollision::DebugCollisionClass>();
	mGammaCorrection = std::make_unique<GammaCorrection::GammaCorrectionClass>();
	mToneMapping = std::make_unique<ToneMapping::ToneMappingClass>();
	mIrradianceMap = std::make_unique<IrradianceMap::IrradianceMapClass>();
	mMipmapGenerator = std::make_unique<MipmapGenerator::MipmapGeneratorClass>();
	mPixelation = std::make_unique<Pixelation::PixelationClass>();
	mSharpen = std::make_unique<Sharpen::SharpenClass>();
	mGaussianFilter = std::make_unique<GaussianFilter::GaussianFilterClass>();
	mRr = std::make_unique<RaytracedReflection::RaytracedReflectionClass>();

	mTLAS = std::make_unique<AccelerationStructureBuffer>();
	mDxrShadowMap = std::make_unique<DxrShadowMap::DxrShadowMapClass>();
	mDxrGeometryBuffer = std::make_unique<DxrGeometryBuffer::DxrGeometryBufferClass>();
	mBlurFilterCS = std::make_unique<BlurFilterCS::BlurFilterCSClass>();
	mRtao = std::make_unique<Rtao::RtaoClass>();

	auto blurWeights = Blur::CalcGaussWeights(2.5f);
	mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	bInitiatingTaa = true;

	mHaltonSequence = {
		XMFLOAT2(0.5f, 0.333333f),
		XMFLOAT2(0.25f, 0.666667f),
		XMFLOAT2(0.75f, 0.111111f),
		XMFLOAT2(0.125f, 0.444444f),
		XMFLOAT2(0.625f, 0.777778f),
		XMFLOAT2(0.375f, 0.222222f),
		XMFLOAT2(0.875f, 0.555556f),
		XMFLOAT2(0.0625f, 0.888889f),
		XMFLOAT2(0.5625f, 0.037037f),
		XMFLOAT2(0.3125f, 0.37037f),
		XMFLOAT2(0.8125f, 0.703704f),
		XMFLOAT2(0.1875f, 0.148148f),
		XMFLOAT2(0.6875f, 0.481481f),
		XMFLOAT2(0.4375f, 0.814815f),
		XMFLOAT2(0.9375f, 0.259259f),
		XMFLOAT2(0.03125f, 0.592593f)
	};
}

DxRenderer::~DxRenderer() {
	if (!bIsCleanedUp) CleanUp();
}

BOOL DxRenderer::Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;

	CheckReturn(LowInitialize(hwnd, width, height));

	auto device = md3dDevice.Get();
	mGraphicsMemory = std::make_unique<GraphicsMemory>(device);

	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	const auto shaderManager = mShaderManager.get();
	
	CheckReturn(mShaderManager->Initialize());
	CheckReturn(mImGui->Initialize(mhMainWnd, device, mCbvSrvUavHeap.Get(), SwapChainBufferCount, SwapChainBuffer::BackBufferFormat));
	
#ifdef _DEBUG
	WLogln(L"Initializing shading components...");
#endif
	CheckReturn(mBRDF->Initialize(device, shaderManager, width, height));
	CheckReturn(mGBuffer->Initialize(device, width, height, shaderManager, mDepthStencilBuffer->Resource(), mDepthStencilBuffer->Dsv()));
	CheckReturn(mShadowMap->Initialize(device, shaderManager, 2048, 2048));
	CheckReturn(mSsao->Initialize(device, cmdList, shaderManager, width, height, 1));
	CheckReturn(mBlurFilter->Initialize(device, shaderManager));
	CheckReturn(mBloom->Initialize(device, shaderManager, width, height, 4));
	CheckReturn(mSsr->Initialize(device, shaderManager, width, height, 2));
	CheckReturn(mDof->Initialize(device, shaderManager, cmdList, width, height));
	CheckReturn(mMotionBlur->Initialize(device, shaderManager, width, height));
	CheckReturn(mTaa->Initialize(device, shaderManager, width, height));
	CheckReturn(mDebugMap->Initialize(device, shaderManager));
	CheckReturn(mDxrShadowMap->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mBlurFilterCS->Initialize(device, shaderManager));
	CheckReturn(mRtao->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mDebugCollision->Initialize(device, shaderManager));
	CheckReturn(mGammaCorrection->Initialize(device, shaderManager, width, height));
	CheckReturn(mToneMapping->Initialize(device, shaderManager, width, height));
	CheckReturn(mIrradianceMap->Initialize(device, cmdList, shaderManager));
	CheckReturn(mMipmapGenerator->Initialize(device, cmdList, shaderManager));
	CheckReturn(mPixelation->Initialize(device, shaderManager, width, height));
	CheckReturn(mSharpen->Initialize(device, shaderManager, width, height));
	CheckReturn(mGaussianFilter->Initialize(device, shaderManager));
	CheckReturn(mRr->Initialize(device, cmdList, shaderManager, width, height));

#ifdef _DEBUG
	WLogln(L"Finished initializing shading components \n");
#endif
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	CheckReturn(CompileShaders());
	CheckReturn(BuildGeometries());

	CheckReturn(BuildFrameResources());

	BuildDescriptors();

	CheckReturn(BuildRootSignatures());
	CheckReturn(BuildPSOs());

	BuildRenderItems();

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}

	bInitialized = true;
#ifdef _DEBUG
	WLogln(L"Succeeded to initialize d3d12 renderer \n");
#endif

	return true;
}

void DxRenderer::CleanUp() {
	if (!bInitialized) return;

	FlushCommandQueue();
	
	mImGui->CleanUp();
	mShaderManager->CleanUp();
	LowCleanUp();

	bIsCleanedUp = true;
}

BOOL DxRenderer::Update(FLOAT delta) {
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		CheckHRESULT(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	CheckReturn(UpdateShadowPassCB(delta));
	CheckReturn(UpdateMainPassCB(delta));
	CheckReturn(UpdateBlurPassCB(delta));
	CheckReturn(UpdateDofCB(delta));
	CheckReturn(UpdateObjectCBs(delta));
	CheckReturn(UpdateMaterialCBs(delta));
	CheckReturn(UpdateConvEquirectToCubeCB(delta));
	CheckReturn(UpdateDebugMapCB(delta));

	if (bRaytracing) {
		CheckReturn(UpdateRtaoCB(delta));
		CheckReturn(UpdateRrCB(delta));
	}
	else {
		CheckReturn(UpdateSsaoPassCB(delta));
		CheckReturn(UpdateSsrCB(delta));
	}
	
	CheckReturn(UpdateShadingObjects(delta));

	return true;
}

BOOL DxRenderer::Draw() {
	CheckHRESULT(mCurrFrameResource->CmdListAlloc->Reset());
	
	// Pre-pass and main-pass
	{
		CheckReturn(DrawGBuffer());
		if (bRaytracing) {
			CheckReturn(DrawDxrShadowMap());
			CheckReturn(DrawRtao());
			CheckReturn(DrawDxrBackBuffer());
		}
		else {
			CheckReturn(DrawShadowMap());
			CheckReturn(DrawSsao());
			CheckReturn(DrawBackBuffer());
		}
	}
	// Post-pass
	{
		CheckReturn(DrawSkySphere());
	
		if (bRaytracing) { CheckReturn(BuildRaytracedReflection()); }
		else { CheckReturn(BuildSsr()); }
	
		CheckReturn(IntegrateSpecIrrad());
		if (bBloomEnabled) CheckReturn(ApplyBloom());
		if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
		
		CheckReturn(ResolveToneMapping());
		if (bGammaCorrectionEnabled) CheckReturn(ApplyGammaCorrection());
	
		if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
		if (bTaaEnabled) CheckReturn(ApplyTAA());
		if (bSharpenEnabled) CheckReturn(ApplySharpen());
		if (bPixelationEnabled) CheckReturn(ApplyPixelation());
	}
	
	if (ShaderArgs::IrradianceMap::ShowIrradianceCubeMap) CheckReturn(DrawEquirectangulaToCube());
	CheckReturn(DrawDebuggingInfo());
	if (bShowImGui) CheckReturn(DrawImGui());
	
	CheckHRESULT(mSwapChain->Present(0, AllowTearing() ? DXGI_PRESENT_ALLOW_TEARING : 0));
	mSwapChainBuffer->NextBackBuffer();
	
	mCurrFrameResource->Fence = static_cast<UINT>(IncreaseFence());
	mCommandQueue->Signal(mFence.Get(), GetCurrentFence());

	return true;
}

BOOL DxRenderer::OnResize(UINT width, UINT height) {
	bool bNeedToReszie = mClientWidth != width || mClientHeight != height;

	mClientWidth = width;
	mClientHeight = height;
	
	CheckReturn(LowOnResize(width, height));
	
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (INT i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}

	CheckReturn(mSwapChainBuffer->OnResize());
	if (bNeedToReszie) {
		CheckReturn(mBRDF->OnResize(width, height));
		CheckReturn(mGBuffer->OnResize(width, height));
		CheckReturn(mDof->OnResize(cmdList, width, height));
		CheckReturn(mMotionBlur->OnResize(width, height));
		CheckReturn(mTaa->OnResize(width, height));
		CheckReturn(mGammaCorrection->OnResize(width, height));
		CheckReturn(mToneMapping->OnResize(width, height));
		CheckReturn(mPixelation->OnResize(width, height));
		CheckReturn(mSharpen->OnResize(width, height));

		CheckReturn(mDxrShadowMap->OnResize(cmdList, width, height));
		CheckReturn(mRtao->OnResize(cmdList, width, height));
		CheckReturn(mRr->OnResize(cmdList, width, height));
	}
	CheckReturn(mSsao->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));
	CheckReturn(mSsr->OnResize(width, height));


	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}
	
	return true;
}

void* DxRenderer::AddModel(const std::string& file, const Transform& trans, RenderType::Type type) {
	if (mGeometries.count(file) == 0) CheckReturn(AddGeometry(file));
	return AddRenderItem(file, trans, type);
}

void DxRenderer::RemoveModel(void* model) {

}

void DxRenderer::UpdateModel(void* model, const Transform& trans) {
	RenderItem* ritem = reinterpret_cast<RenderItem*>(model);
	if (ritem == nullptr) return;

	auto begin = mRitems.begin();
	auto end = mRitems.end();
	auto iter = std::find_if(begin, end, [&](std::unique_ptr<RenderItem>& p) {
		return p.get() == ritem;
		});

	auto ptr = iter->get();
	XMStoreFloat4x4(
		&ptr->World, 
		XMMatrixAffineTransformation(
			trans.Scale, 
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), 
			trans.Rotation, 
			trans.Position
		)
	);
	ptr->NumFramesDirty = gNumFrameResources << 1;
}

void DxRenderer::SetModelVisibility(void* model, BOOL visible) {

}

void DxRenderer::SetModelPickable(void* model, BOOL pickable) {
	auto begin = mRitems.begin();
	auto end = mRitems.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<RenderItem>& p) {
		return p.get() == model;
	});
	if (iter != end) iter->get()->Pickable = pickable;
}

BOOL DxRenderer::SetCubeMap(const std::string& file) {

	return true;
}

BOOL DxRenderer::SetEquirectangularMap(const std::string& file) {	
	mIrradianceMap->SetEquirectangularMap(mCommandQueue.Get(), file);

	return true;
}

void DxRenderer::Pick(FLOAT x, FLOAT y) {
	const auto& P = mCamera->GetProj();

	// Compute picking ray in vew space.
	FLOAT vx = (2.0f * x / mClientWidth - 1.0f) / P(0, 0);
	FLOAT vy = (-2.0f * y / mClientHeight + 1.0f) / P(1, 1);

	// Ray definition in view space.
	auto origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	auto dir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	const auto V = XMLoadFloat4x4(&mCamera->GetView());
	const auto InvView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	FLOAT closestT = std::numeric_limits<FLOAT>().max();
	for (auto ri : mRitemRefs[RenderType::E_Opaque]) {
		if (!ri->Pickable) continue;

		auto geo = ri->Geometry;

		const auto W = XMLoadFloat4x4(&ri->World);
		const auto InvWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

		// Transform ray to vi space of the mesh.
		const auto ToLocal = XMMatrixMultiply(InvView, InvWorld);

		auto originL = XMVector3TransformCoord(origin, ToLocal);
		auto dirL = XMVector3TransformNormal(dir, ToLocal);

		// Make the ray direction unit length for the intersection tests.
		dirL = XMVector3Normalize(dirL);
		FLOAT tmin = 0.0f;
		if (ri->AABB.Intersects(originL, dirL, tmin) && tmin < closestT) {
			closestT = tmin;
			mPickedRitem = ri;
		}
	}
}

BOOL DxRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors =
		SwapChainBufferCount
		+ GBuffer::NumRenderTargets
		+ Ssao::NumRenderTargets
		+ MotionBlur::NumRenderTargets
		+ DepthOfField::NumRenderTargets
		+ Bloom::NumRenderTargets
		+ Ssr::NumRenderTargets
		+ ToneMapping::NumRenderTargets
		+ IrradianceMap::NumRenderTargets;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + ShadowMap::NumDepthStenciles;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = MAX_DESCRIPTOR_SIZE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvUavHeap)));

	return true;
}

BOOL DxRenderer::CompileShaders() {
#ifdef _DEBUG
	WLogln(L"Compiling shaders...");
#endif 

	CheckReturn(mBRDF->CompileShaders(ShaderFilePath));
	CheckReturn(mGBuffer->CompileShaders(ShaderFilePath));
	CheckReturn(mShadowMap->CompileShaders(ShaderFilePath));
	CheckReturn(mSsao->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilter->CompileShaders(ShaderFilePath));
	CheckReturn(mBloom->CompileShaders(ShaderFilePath));
	CheckReturn(mSsr->CompileShaders(ShaderFilePath));
	CheckReturn(mDof->CompileShaders(ShaderFilePath));
	CheckReturn(mMotionBlur->CompileShaders(ShaderFilePath));
	CheckReturn(mTaa->CompileShaders(ShaderFilePath));
	CheckReturn(mDebugMap->CompileShaders(ShaderFilePath));
	CheckReturn(mDebugCollision->CompileShaders(ShaderFilePath));
	CheckReturn(mGammaCorrection->CompileShaders(ShaderFilePath));
	CheckReturn(mToneMapping->CompileShaders(ShaderFilePath));
	CheckReturn(mIrradianceMap->CompileShaders(ShaderFilePath));
	CheckReturn(mMipmapGenerator->CompileShaders(ShaderFilePath));
	CheckReturn(mPixelation->CompileShaders(ShaderFilePath));
	CheckReturn(mSharpen->CompileShaders(ShaderFilePath));
	CheckReturn(mGaussianFilter->CompileShaders(ShaderFilePath));

	CheckReturn(mDxrShadowMap->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilterCS->CompileShaders(ShaderFilePath));
	CheckReturn(mRtao->CompileShaders(ShaderFilePath));
	CheckReturn(mRr->CompileShaders(ShaderFilePath));

#ifdef _DEBUG
	WLogln(L"Finished compiling shaders \n");
#endif 

	return true;
}

BOOL DxRenderer::BuildGeometries() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 32, 32);
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 1);

	UINT numVertices = 0;
	UINT numIndices = 0;

	UINT startIndexLocation = 0;
	UINT baseVertexLocation = 0;

	// 1.
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = startIndexLocation;
	sphereSubmesh.BaseVertexLocation = baseVertexLocation;
	{
		auto numIdx = static_cast<UINT>(sphere.GetIndices16().size());
		auto numVert = static_cast<UINT>(sphere.Vertices.size());

		sphereSubmesh.IndexCount = numIdx;

		numIndices += numIdx;
		numVertices += numVert;

		startIndexLocation += numIdx;
		baseVertexLocation += numVert;
	}

	// 2.
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = startIndexLocation;
	boxSubmesh.BaseVertexLocation = baseVertexLocation;
	{
		auto numIdx = static_cast<UINT>(box.GetIndices16().size());
		auto numVert = static_cast<UINT>(box.Vertices.size());

		boxSubmesh.IndexCount = numIdx;

		numIndices += numIdx;
		numVertices += numVert;

		startIndexLocation += numIdx;
		baseVertexLocation += numVert;
	}

	std::vector<Vertex> vertices(numVertices);

	for (size_t i = 0, end = sphere.Vertices.size(); i < end; ++i) {
		auto index = i + sphereSubmesh.BaseVertexLocation;
		vertices[index].Position = sphere.Vertices[i].Position;
		vertices[index].Normal = sphere.Vertices[i].Normal;
		vertices[index].TexCoord = sphere.Vertices[i].TexC;
	}
	for (size_t i = 0, end = box.Vertices.size(); i < end; ++i) {
		auto index = i + boxSubmesh.BaseVertexLocation;
		vertices[index].Position = box.Vertices[i].Position;
		vertices[index].Normal = box.Vertices[i].Normal;
		vertices[index].TexCoord = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices(numIndices);
	for (size_t i = 0, end = sphere.GetIndices16().size(); i < end; ++i) {
		auto index = i + sphereSubmesh.StartIndexLocation;
		indices[index] = sphere.GetIndices16()[i];
	}
	for (size_t i = 0, end = box.GetIndices16().size(); i < end; ++i) {
		auto index = i + boxSubmesh.StartIndexLocation;
		indices[index] = box.GetIndices16()[i];
	}

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "basic";

	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckHRESULT(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	CheckHRESULT(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	CheckReturn(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(),
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckReturn(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader,
		geo->IndexBufferGPU)
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	geo->VertexByteStride = static_cast<UINT>(sizeof(Vertex));
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);

	return true;
}

BOOL DxRenderer::BuildFrameResources() {
	for (INT i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 2, 32, 32));
		CheckReturn(mFrameResources.back()->Initialize());
	}

	return true;
}

void DxRenderer::BuildDescriptors() {
	auto& hCpu = mhCpuCbvSrvUav;
	auto& hGpu = mhGpuCbvSrvUav;
	auto& hCpuDsv = mhCpuDsv;
	auto& hCpuRtv = mhCpuRtv;

	auto descSize = GetCbvSrvUavDescriptorSize();
	auto rtvDescSize = GetRtvDescriptorSize();
	auto dsvDescSize = GetDsvDescriptorSize();

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (INT i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}

	mImGui->BuildDescriptors(hCpu, hGpu, descSize);
	mSwapChainBuffer->BuildDescriptors(hCpu, hGpu, descSize);
	mBRDF->BuildDescriptors(hCpu, hGpu, descSize);
	mGBuffer->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mShadowMap->BuildDescriptors(hCpu, hGpu, hCpuDsv, descSize, dsvDescSize);
	mSsao->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mBloom->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSsr->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mDof->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mMotionBlur->BuildDescriptors(hCpuRtv, rtvDescSize);
	mTaa->BuildDescriptors(hCpu, hGpu, descSize);
	mGammaCorrection->BuildDescriptors(hCpu, hGpu, descSize);
	mToneMapping->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mIrradianceMap->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mPixelation->BuildDescriptors(hCpu, hGpu, descSize);
	mSharpen->BuildDescriptors(hCpu, hGpu, descSize);

	mDxrShadowMap->BuildDescriptors(hCpu, hGpu, descSize);
	mDxrGeometryBuffer->BuildDescriptors(hCpu, hGpu, descSize);
	mRtao->BuildDescriptors(hCpu, hGpu, descSize);
	mRr->BuildDesscriptors(hCpu, hGpu, descSize);

	mhCpuDescForTexMaps = hCpu;
	mhGpuDescForTexMaps = hGpu;
}

BOOL DxRenderer::BuildRootSignatures() {
#if _DEBUG
	WLogln(L"Building root-signatures...");
#endif

	auto staticSamplers = Samplers::GetStaticSamplers();
	CheckReturn(mBRDF->BuildRootSignature(staticSamplers));
	CheckReturn(mGBuffer->BuildRootSignature(staticSamplers));
	CheckReturn(mShadowMap->BuildRootSignature(staticSamplers));
	CheckReturn(mSsao->BuildRootSignature(staticSamplers));
	CheckReturn(mBlurFilter->BuildRootSignature(staticSamplers));
	CheckReturn(mBloom->BuildRootSignature(staticSamplers));
	CheckReturn(mSsr->BuildRootSignature(staticSamplers));
	CheckReturn(mDof->BuildRootSignature(staticSamplers));
	CheckReturn(mMotionBlur->BuildRootSignature(staticSamplers));
	CheckReturn(mTaa->BuildRootSignature(staticSamplers));
	CheckReturn(mDebugMap->BuildRootSignature(staticSamplers));
	CheckReturn(mDebugCollision->BuildRootSignature());
	CheckReturn(mGammaCorrection->BuildRootSignature(staticSamplers));
	CheckReturn(mToneMapping->BuildRootSignature(staticSamplers));
	CheckReturn(mIrradianceMap->BuildRootSignature(staticSamplers));
	CheckReturn(mMipmapGenerator->BuildRootSignature(staticSamplers));
	CheckReturn(mPixelation->BuildRootSignature(staticSamplers));
	CheckReturn(mSharpen->BuildRootSignature(staticSamplers));
	CheckReturn(mGaussianFilter->BuildRootSignature(staticSamplers));

	CheckReturn(mDxrShadowMap->BuildRootSignatures(staticSamplers, DxrGeometryBuffer::GeometryBufferCount));
	CheckReturn(mBlurFilterCS->BuildRootSignature(staticSamplers));
	CheckReturn(mRtao->BuildRootSignatures(staticSamplers));
	CheckReturn(mRr->BuildRootSignatures(staticSamplers));

#if _DEBUG
	WLogln(L"Finished building root-signatures \n");
#endif

	return true;
}

BOOL DxRenderer::BuildPSOs() {
#ifdef _DEBUG
	WLogln(L"Building pipeline state objects...");
#endif

	CheckReturn(mBRDF->BuildPso());
	CheckReturn(mGBuffer->BuildPso());
	CheckReturn(mShadowMap->BuildPso());
	CheckReturn(mSsao->BuildPso());
	CheckReturn(mBloom->BuildPso());
	CheckReturn(mBlurFilter->BuildPso());
	CheckReturn(mSsr->BuildPso());
	CheckReturn(mDof->BuildPso());
	CheckReturn(mMotionBlur->BuildPso());
	CheckReturn(mTaa->BuildPso());
	CheckReturn(mDebugMap->BuildPso());
	CheckReturn(mDebugCollision->BuildPso());
	CheckReturn(mGammaCorrection->BuildPso());
	CheckReturn(mToneMapping->BuildPso());
	CheckReturn(mIrradianceMap->BuildPso());
	CheckReturn(mMipmapGenerator->BuildPso());
	CheckReturn(mPixelation->BuildPso());
	CheckReturn(mSharpen->BuildPso());
	CheckReturn(mGaussianFilter->BuildPso());
	
	CheckReturn(mDxrShadowMap->BuildPso());
	CheckReturn(mBlurFilterCS->BuildPso());
	CheckReturn(mRtao->BuildPSO());
	CheckReturn(mRr->BuildPSO());

#ifdef _DEBUG
	WLogln(L"Finished building pipeline state objects \n");
#endif

	return true;
}

void DxRenderer::BuildRenderItems() {
	{
		auto skyRitem = std::make_unique<RenderItem>();
		skyRitem->ObjCBIndex = static_cast<INT>(mRitems.size());
		skyRitem->Geometry = mGeometries["basic"].get();
		skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		skyRitem->IndexCount = skyRitem->Geometry->DrawArgs["sphere"].IndexCount;
		skyRitem->StartIndexLocation = skyRitem->Geometry->DrawArgs["sphere"].StartIndexLocation;
		skyRitem->BaseVertexLocation = skyRitem->Geometry->DrawArgs["sphere"].BaseVertexLocation;
		XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(1000.0f, 1000.0f, 1000.0f));
		mSkySphere = skyRitem.get();
		mRitems.push_back(std::move(skyRitem));
	}
	{
		auto boxRitem = std::make_unique<RenderItem>();
		boxRitem->ObjCBIndex = static_cast<INT>(mRitems.size());
		boxRitem->Geometry = mGeometries["basic"].get();
		boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		boxRitem->IndexCount = boxRitem->Geometry->DrawArgs["box"].IndexCount;
		boxRitem->StartIndexLocation = boxRitem->Geometry->DrawArgs["box"].StartIndexLocation;
		boxRitem->BaseVertexLocation = boxRitem->Geometry->DrawArgs["box"].BaseVertexLocation;
		XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 4.5f, 0.0f));
		mIrradianceCubeMap = boxRitem.get();
		mRitems.push_back(std::move(boxRitem));
	}
}

BOOL DxRenderer::AddGeometry(const std::string& file) {
	Mesh mesh;
	Material mat;
	CheckReturn(MeshImporter::LoadObj(file, mesh, mat));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = file;

	const auto& vertices = mesh.Vertices;
	const auto& indices = mesh.Indices;

	const size_t vertexSize = sizeof(Vertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(UINT));

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	for (size_t i = 0, end = vertices.size(); i < end; ++i) {
		const XMVECTOR P = XMLoadFloat3(&vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	BoundingBox bound;
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));

	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckHRESULT(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	CheckHRESULT(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);	

	CheckReturn(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(),
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckReturn(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader,
		geo->IndexBufferGPU)
	);

	geo->VertexByteStride = static_cast<UINT>(vertexSize);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;
	
	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.AABB = bound;

	geo->DrawArgs["mesh"] = submesh;

	CheckReturn(AddBLAS(cmdList, geo.get()));

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	mGeometries[file] = std::move(geo);

	if (mMaterials.count(file) == 0) CheckReturn(AddMaterial(file, mat));

	return true;
}

BOOL DxRenderer::AddMaterial(const std::string& file, const Material& material) {
	UINT index = AddTexture(file, material);
	if (index == -1) ReturnFalse("Failed to create texture");

	auto matData = std::make_unique<MaterialData>();
	matData->MatCBIndex = static_cast<INT>(mMaterials.size());
	matData->MatTransform = MathHelper::Identity4x4();
	matData->DiffuseSrvHeapIndex = index;
	matData->Albedo = material.Albedo;
	matData->Specular = material.Specular;
	matData->Roughness = material.Roughness;

	mMaterials[file] = std::move(matData);

	return true;
}

void* DxRenderer::AddRenderItem(const std::string& file, const Transform& trans, RenderType::Type type) {
	if (mGeometries.count(file) == 0) CheckReturn(AddGeometry(file));

	auto ritem = std::make_unique<RenderItem>();
	ritem->ObjCBIndex = static_cast<INT>(mRitems.size());
	ritem->Material = mMaterials[file].get();
	ritem->Geometry = mGeometries[file].get();
	ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ritem->IndexCount = ritem->Geometry->DrawArgs["mesh"].IndexCount;
	ritem->StartIndexLocation = ritem->Geometry->DrawArgs["mesh"].StartIndexLocation;
	ritem->BaseVertexLocation = ritem->Geometry->DrawArgs["mesh"].BaseVertexLocation;
	ritem->AABB = ritem->Geometry->DrawArgs["mesh"].AABB;
	XMStoreFloat4x4(
		&ritem->World,
		XMMatrixAffineTransformation(
			trans.Scale,
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			trans.Rotation,
			trans.Position
		)
	);

	mRitemRefs[type].push_back(ritem.get());
	mRitems.push_back(std::move(ritem));

	return mRitems.back().get();
}

UINT DxRenderer::AddTexture(const std::string& file, const Material& material) {
	ID3D12GraphicsCommandList* cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	auto texMap = std::make_unique<Texture>();
	texMap->DescriptorIndex = mCurrDescriptorIndex;

	std::wstring filename;
	filename.assign(material.DiffuseMapFileName.begin(), material.DiffuseMapFileName.end());

	auto index = filename.rfind(L'.');
	filename = filename.replace(filename.begin() + index, filename.end(), L".dds");

	ResourceUploadBatch resourceUpload(md3dDevice.Get());

	resourceUpload.Begin();

	HRESULT status = DirectX::CreateDDSTextureFromFile(
		md3dDevice.Get(),
		resourceUpload,
		filename.c_str(),
		texMap->Resource.ReleaseAndGetAddressOf()
	);

	auto finished = resourceUpload.End(mCommandQueue.Get());
	finished.wait();

	if (FAILED(status)) {
		std::wstringstream wsstream;
		wsstream << "Returned " << std::hex << status << "; when creating texture:  " << filename;
		WLogln(wsstream.str());
		return -1;
	}

	const auto& resource = texMap->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	auto hDescriptor = mhCpuDescForTexMaps;
	hDescriptor.Offset(texMap->DescriptorIndex, GetCbvSrvUavDescriptorSize());

	md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, hDescriptor);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	mTextures[file] = std::move(texMap);

	return mCurrDescriptorIndex++;
}

BOOL DxRenderer::UpdateShadingObjects(FLOAT delta) {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	CheckReturn(mIrradianceMap->Update(
		mCommandQueue.Get(),
		mCbvSrvUavHeap.Get(),
		cmdList,
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->ConvEquirectToCubeCB.Resource()->GetGPUVirtualAddress(),
		mMipmapGenerator.get(),
		mIrradianceCubeMap
	));
	
	if (bNeedToRebuildTLAS) {
		bNeedToRebuildTLAS = false;
		CheckReturn(BuildTLAS(cmdList));
	}
	else {
		CheckReturn(UpdateTLAS(cmdList));
	}
	
	if (bNeedToRebuildShaderTables) {
		bNeedToRebuildShaderTables = false;
		BuildShaderTables();
	}
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	return true;
}

BOOL DxRenderer::UpdateShadowPassCB(FLOAT delta) {
	XMVECTOR lightDir = XMLoadFloat3(&mLightDir);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = UnitVectors::UpVector;
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	FLOAT l = sphereCenterLS.x - mSceneBounds.Radius;
	FLOAT b = sphereCenterLS.y - mSceneBounds.Radius;
	FLOAT n = sphereCenterLS.z - mSceneBounds.Radius;
	FLOAT r = sphereCenterLS.x + mSceneBounds.Radius;
	FLOAT t = sphereCenterLS.y + mSceneBounds.Radius;
	FLOAT f = sphereCenterLS.z + mSceneBounds.Radius;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1 , +1]^2 to texture space [0, 1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mMainPassCB->ShadowTransform, XMMatrixTranspose(S));

	XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(lightView), lightView);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(lightProj), lightProj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mShadowPassCB->View, XMMatrixTranspose(lightView));
	XMStoreFloat4x4(&mShadowPassCB->InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB->Proj, XMMatrixTranspose(lightProj));
	XMStoreFloat4x4(&mShadowPassCB->InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB->ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB->InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat3(&mShadowPassCB->EyePosW, lightPos);

	auto& currPassCB = mCurrFrameResource->PassCB;
	currPassCB.CopyData(1, *mShadowPassCB);

	return true;
}

BOOL DxRenderer::UpdateMainPassCB(FLOAT delta) {
	XMMATRIX view = XMLoadFloat4x4(&mCamera->GetView());
	XMMATRIX proj = XMLoadFloat4x4(&mCamera->GetProj());
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);
	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);

	size_t offsetIndex = static_cast<size_t>(GetCurrentFence()) % mFittedToBakcBufferHaltonSequence.size();

	mMainPassCB->PrevViewProj = mMainPassCB->ViewProj;
	XMStoreFloat4x4(&mMainPassCB->View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB->InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB->Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB->InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB->ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB->InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB->ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat3(&mMainPassCB->EyePosW, mCamera->GetPosition());
	mMainPassCB->JitteredOffset = bTaaEnabled ? mFittedToBakcBufferHaltonSequence[offsetIndex] : XMFLOAT2(0.0f, 0.0f);

	const auto ambient = ShaderArgs::Light::AmbientLight;
	mMainPassCB->AmbientLight = XMFLOAT4(ambient[0], ambient[1], ambient[2], ambient[3]);

	XMVECTOR strength = XMLoadFloat3(&XMFLOAT3(ShaderArgs::Light::DirectionalLight::Strength));
	strength = XMVectorScale(strength, ShaderArgs::Light::DirectionalLight::Multiplier);
	XMStoreFloat3(&mMainPassCB->Lights[0].Strength, strength);
	mMainPassCB->Lights[0].Direction = mLightDir;

	auto& currPassCB = mCurrFrameResource->PassCB;
	currPassCB.CopyData(0, *mMainPassCB);

	return true;
}

BOOL DxRenderer::UpdateSsaoPassCB(FLOAT delta) {
	SsaoConstants ssaoCB;
	ssaoCB.View = mMainPassCB->View;
	ssaoCB.Proj = mMainPassCB->Proj;
	ssaoCB.InvProj = mMainPassCB->InvProj;

	XMMATRIX P = XMLoadFloat4x4(&mCamera->GetProj());
	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 2.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	ssaoCB.SampleCount = ShaderArgs::Ssao::SampleCount;

	auto& currSsaoCB = mCurrFrameResource->SsaoCB;
	currSsaoCB.CopyData(0, ssaoCB);

	return true;
}

BOOL DxRenderer::UpdateBlurPassCB(FLOAT delta) {
	BlurConstants blurCB;
	blurCB.Proj = mMainPassCB->Proj;
	blurCB.BlurWeights[0] = mBlurWeights[0];
	blurCB.BlurWeights[1] = mBlurWeights[1];
	blurCB.BlurWeights[2] = mBlurWeights[2];
	blurCB.BlurRadius = 5;

	auto& currBlurCB = mCurrFrameResource->BlurCB;
	currBlurCB.CopyData(0, blurCB);

	return true;
}

BOOL DxRenderer::UpdateDofCB(FLOAT delta) {
	DofConstants dofCB;
	dofCB.Proj = mMainPassCB->Proj;
	dofCB.InvProj = mMainPassCB->InvProj;
	dofCB.FocusRange = ShaderArgs::DepthOfField::FocusRange;
	dofCB.FocusingSpeed = ShaderArgs::DepthOfField::FocusingSpeed;
	dofCB.DeltaTime = delta;

	auto& currDofCB = mCurrFrameResource->DofCB;
	currDofCB.CopyData(0, dofCB);

	return true;
}

BOOL DxRenderer::UpdateSsrCB(FLOAT delta) {
	SsrConstants ssrCB;
	ssrCB.View = mMainPassCB->View;
	ssrCB.InvView = mMainPassCB->InvView;
	ssrCB.Proj = mMainPassCB->Proj;
	ssrCB.InvProj = mMainPassCB->InvProj;
	XMStoreFloat3(&ssrCB.EyePosW, mCamera->GetPosition());
	ssrCB.MaxDistance = ShaderArgs::Ssr::MaxDistance;
	ssrCB.RayLength = ShaderArgs::Ssr::RayLength;
	ssrCB.NoiseIntensity = ShaderArgs::Ssr::NoiseIntensity;
	ssrCB.NumSteps = ShaderArgs::Ssr::StepCount;
	ssrCB.NumBackSteps = ShaderArgs::Ssr::BackStepCount;
	ssrCB.DepthThreshold = ShaderArgs::Ssr::DepthThreshold;

	auto& currSsrCB = mCurrFrameResource->SsrCB;
	currSsrCB.CopyData(0, ssrCB);

	return true;
}

BOOL DxRenderer::UpdateObjectCBs(FLOAT delta) {
	auto& currObjectCB = mCurrFrameResource->ObjectCB;
	for (auto& e : mRitems) {
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			objConstants.PrevWorld = e->PrevWolrd;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.Center = { e->AABB.Center.x, e->AABB.Center.y, e->AABB.Center.z, 1.0f };
			objConstants.Extents = { e->AABB.Extents.x, e->AABB.Extents.y, e->AABB.Extents.z, 0.0f };

			e->PrevWolrd = objConstants.World;

			currObjectCB.CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;

		}
	}

	return true;
}

BOOL DxRenderer::UpdateMaterialCBs(FLOAT delta) {
	auto& currMaterialCB = mCurrFrameResource->MaterialCB;
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		auto* mat = e.second.get();
		//if (mat->NumFramesDirty > 0) {
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConsts;
			matConsts.DiffuseSrvIndex = mat->DiffuseSrvHeapIndex;
			matConsts.Albedo = mat->Albedo;
			matConsts.Roughness = mat->Roughness;
			matConsts.Metalic = mat->Metailic;
			matConsts.Specular = mat->Specular;
			XMStoreFloat4x4(&matConsts.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB.CopyData(mat->MatCBIndex, matConsts);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return true;
}

BOOL DxRenderer::UpdateConvEquirectToCubeCB(FLOAT delta) {
	ConvertEquirectangularToCubeConstantBuffer cubeCB;
		
	XMStoreFloat4x4(&cubeCB.Proj, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f)));

	// Positive +X
	XMStoreFloat4x4(
		&cubeCB.View[0],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive -X
	XMStoreFloat4x4(
		&cubeCB.View[1],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive +Y
	XMStoreFloat4x4(
		&cubeCB.View[2],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)
		))
	);
	// Positive -Y
	XMStoreFloat4x4(
		&cubeCB.View[3],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
		))
	);
	// Positive +Z
	XMStoreFloat4x4(
		&cubeCB.View[4],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive -Z
	XMStoreFloat4x4(
		&cubeCB.View[5],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	
	auto& currCubeCB = mCurrFrameResource->ConvEquirectToCubeCB;
	currCubeCB.CopyData(0, cubeCB);

	return true;
}

BOOL DxRenderer::UpdateRtaoCB(FLOAT delta) {
	// Ambient occlusion
	{
		static UINT count = 0;
		static auto prev = mMainPassCB->View;

		RtaoConstants rtaoCB;
		rtaoCB.View = mMainPassCB->View;
		rtaoCB.InvView = mMainPassCB->InvView;
		rtaoCB.Proj = mMainPassCB->Proj;
		rtaoCB.InvProj = mMainPassCB->InvProj;

		// Coordinates given in view space.
		rtaoCB.OcclusionRadius = ShaderArgs::Rtao::OcclusionRadius;
		rtaoCB.OcclusionFadeStart = ShaderArgs::Rtao::OcclusionFadeStart;
		rtaoCB.OcclusionFadeEnd = ShaderArgs::Rtao::OcclusionFadeEnd;
		rtaoCB.SurfaceEpsilon = ShaderArgs::Rtao::OcclusionEpsilon;

		rtaoCB.FrameCount = count++;
		rtaoCB.SampleCount = ShaderArgs::Rtao::SampleCount;

		prev = mMainPassCB->View;

		auto& currRtaoCB = mCurrFrameResource->RtaoCB;
		currRtaoCB.CopyData(0, rtaoCB);
	}
	// Calculate local mean/variance
	{
		CalcLocalMeanVarianceConstants calcLocalMeanVarCB;

		ShaderArgs::Rtao::CheckerboardGenerateRaysForEvenPixels = !ShaderArgs::Rtao::CheckerboardGenerateRaysForEvenPixels;

		calcLocalMeanVarCB.TextureDim = { mClientWidth, mClientHeight };
		calcLocalMeanVarCB.KernelWidth = 9;
		calcLocalMeanVarCB.KernelRadius = 9 >> 1;

		calcLocalMeanVarCB.CheckerboardSamplingEnabled = ShaderArgs::Rtao::CheckerboardSamplingEnabled;
		calcLocalMeanVarCB.EvenPixelActivated = ShaderArgs::Rtao::CheckerboardGenerateRaysForEvenPixels;
		calcLocalMeanVarCB.PixelStepY = ShaderArgs::Rtao::CheckerboardSamplingEnabled ? 2 : 1;

		auto& currLocalCalcMeanVarCB = mCurrFrameResource->CalcLocalMeanVarCB;
		currLocalCalcMeanVarCB.CopyData(0, calcLocalMeanVarCB);
	}
	// Temporal supersampling reverse reproject
	{
		CrossBilateralFilterConstants filterCB;
		filterCB.DepthSigma = 1.0f;
		filterCB.DepthNumMantissaBits = D3D12Util::NumMantissaBitsInFloatFormat(16);

		auto& currFilterCB = mCurrFrameResource->CrossBilateralFilterCB;
		currFilterCB.CopyData(0, filterCB);
	}
	// Temporal supersampling blend with current frame
	{
		TemporalSupersamplingBlendWithCurrentFrameConstants tsppBlendCB;
		tsppBlendCB.StdDevGamma = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::ClampCachedValues::StdDevGamma;
		tsppBlendCB.ClampCachedValues = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::ClampCachedValues::UseClamping;
		tsppBlendCB.ClampingMinStdDevTolerance = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::ClampCachedValues::MinStdDevTolerance;

		tsppBlendCB.ClampDifferenceToTsppScale = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::ClampDifferenceToTsppScale;
		tsppBlendCB.ForceUseMinSmoothingFactor = false;
		tsppBlendCB.MinSmoothingFactor = 1.0f / ShaderArgs::Rtao::Denoiser::TemporalSupersampling::MaxTspp;
		tsppBlendCB.MinTsppToUseTemporalVariance = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::MinTsppToUseTemporalVariance;

		tsppBlendCB.BlurStrengthMaxTspp = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::LowTsppMaxTspp;
		tsppBlendCB.BlurDecayStrength = ShaderArgs::Rtao::Denoiser::TemporalSupersampling::LowTsppDecayConstant;
		tsppBlendCB.CheckerboardEnabled = ShaderArgs::Rtao::CheckerboardSamplingEnabled;
		tsppBlendCB.CheckerboardEvenPixelActivated = ShaderArgs::Rtao::CheckerboardGenerateRaysForEvenPixels;

		auto& currTsppBlendCB = mCurrFrameResource->TsppBlendCB;
		currTsppBlendCB.CopyData(0, tsppBlendCB);
	}
	// Atrous wavelet transform filter
	{
		AtrousWaveletTransformFilterConstantBuffer atrousFilterCB;

		// Adaptive kernel radius rotation.
		FLOAT kernelRadiusLerfCoef = 0;
		if (ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::KernelRadiusRotateKernelEnabled) {
			static UINT frameID = 0;
			UINT i = frameID++ % ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles;
			kernelRadiusLerfCoef = i / static_cast<FLOAT>(ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles);
		}

		atrousFilterCB.TextureDim = XMUINT2(mClientWidth, mClientHeight);
		atrousFilterCB.DepthWeightCutoff = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::DepthWeightCutoff;
		atrousFilterCB.UsingBilateralDownsamplingBuffers = ShaderArgs::Rtao::QuarterResolutionAO;

		atrousFilterCB.UseAdaptiveKernelSize = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::UseAdaptiveKernelSize;
		atrousFilterCB.KernelRadiusLerfCoef = kernelRadiusLerfCoef;
		atrousFilterCB.MinKernelWidth = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::FilterMinKernelWidth;
		atrousFilterCB.MaxKernelWidth = static_cast<UINT>((ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::FilterMaxKernelWidthPercentage / 100) * mClientWidth);

		atrousFilterCB.RayHitDistanceToKernelWidthScale = 22 / ShaderArgs::Rtao::MaxRayHitTime *
			ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleFactor;
		atrousFilterCB.RayHitDistanceToKernelSizeScaleExponent = D3D12Util::Lerp(
			1,
			ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleExponent,
			D3D12Util::RelativeCoef(ShaderArgs::Rtao::MaxRayHitTime, 4, 22)
		);
		atrousFilterCB.PerspectiveCorrectDepthInterpolation = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::PerspectiveCorrectDepthInterpolation;
		atrousFilterCB.MinVarianceToDenoise = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::MinVarianceToDenoise;

		atrousFilterCB.ValueSigma = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::ValueSigma;
		atrousFilterCB.DepthSigma = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::DepthSigma;
		atrousFilterCB.NormalSigma = ShaderArgs::Rtao::Denoiser::AtrousWaveletTransformFilter::NormalSigma;
		atrousFilterCB.FovY = mCamera->FovY();

		atrousFilterCB.DepthNumMantissaBits = D3D12Util::NumMantissaBitsInFloatFormat(16);

		auto& currAtrousFilterCB = mCurrFrameResource->AtrousFilterCB;
		currAtrousFilterCB.CopyData(0, atrousFilterCB);
	}

	return true;
}

BOOL DxRenderer::UpdateRrCB(FLOAT delta) {
	RaytracedReflectionConstantBuffer rrCB;

	rrCB.View = mMainPassCB->View;
	rrCB.InvView = mMainPassCB->InvView;
	rrCB.Proj = mMainPassCB->Proj;
	rrCB.InvProj = mMainPassCB->InvProj;
	rrCB.ViewProj = mMainPassCB->ViewProj;

	rrCB.EyePosW = mMainPassCB->EyePosW;
	rrCB.ReflectionRadius = ShaderArgs::RaytracedReflection::ReflectionRadius;

	rrCB.TextureDim = { mClientWidth, mClientHeight };

	auto& currRrCB = mCurrFrameResource->RrCB;
	currRrCB.CopyData(0, rrCB);

	return true;
}

BOOL DxRenderer::UpdateDebugMapCB(FLOAT delta) {
	DebugMapConstantBuffer debugMapCB;

	for (UINT i = 0; i < DebugMap::MapSize; ++i) {
		auto desc = mDebugMap->SampleDesc(i);
		debugMapCB.SampleDescs[i] = desc;
	}

	auto& currDebugMapCB = mCurrFrameResource->DebugMapCB;
	currDebugMapCB.CopyData(0, debugMapCB);

	return true;
}

BOOL DxRenderer::AddBLAS(ID3D12GraphicsCommandList4* const cmdList, MeshGeometry* const geo) {
	std::unique_ptr<AccelerationStructureBuffer> blas = std::make_unique<AccelerationStructureBuffer>();

	CheckReturn(blas->BuildBLAS(md3dDevice.Get(), cmdList, geo));

	mBLASRefs[geo->Name] = blas.get();
	mBLASes.emplace_back(std::move(blas));
	
	bNeedToRebuildTLAS = true;
	bNeedToRebuildShaderTables = true;

	return true;
}

BOOL DxRenderer::BuildTLAS(ID3D12GraphicsCommandList4* const cmdList) {
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

	const auto& opaques = mRitemRefs[RenderType::E_Opaque];

	for (const auto ri : opaques) {
		auto iter = std::find(opaques.begin(), opaques.end(), ri);

		UINT hitGroupIndex = static_cast<UINT>(std::distance(opaques.begin(), iter));

		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceContributionToHitGroupIndex = hitGroupIndex;
		instanceDesc.InstanceMask = 0xFF;
		for (INT r = 0; r < 3; ++r) {
			for (INT c = 0; c < 4; ++c) {
				instanceDesc.Transform[r][c] = ri->World.m[c][r];
			}
		}
		instanceDesc.AccelerationStructure = mBLASRefs[ri->Geometry->Name]->Result->GetGPUVirtualAddress();
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instanceDescs.push_back(instanceDesc);
	}

	CheckReturn(mTLAS->BuildTLAS(md3dDevice.Get(), cmdList, instanceDescs));

	return true;
}

BOOL DxRenderer::UpdateTLAS(ID3D12GraphicsCommandList4* const cmdList) {
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

	const auto& opaques = mRitemRefs[RenderType::E_Opaque];

	for (const auto ri : opaques) {
		auto iter = std::find(opaques.begin(), opaques.end(), ri);

		UINT hitGroupIndex = static_cast<UINT>(std::distance(opaques.begin(), iter));

		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceContributionToHitGroupIndex = hitGroupIndex;
		instanceDesc.InstanceMask = 0xFF;
		for (INT r = 0; r < 3; ++r) {
			for (INT c = 0; c < 4; ++c) {
				instanceDesc.Transform[r][c] = ri->World.m[c][r];
			}
		}
		instanceDesc.AccelerationStructure = mBLASRefs[ri->Geometry->Name]->Result->GetGPUVirtualAddress();
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instanceDescs.push_back(instanceDesc);
	}

	mTLAS->UpdateTLAS(cmdList, instanceDescs);

	return true;
}

BOOL DxRenderer::BuildShaderTables() {
	const auto& opaques = mRitemRefs[RenderType::E_Opaque];

	UINT numRitems = static_cast<UINT>(opaques.size());
	CheckReturn(mDxrShadowMap->BuildShaderTables(numRitems));
	CheckReturn(mRtao->BuildShaderTables(numRitems));
	CheckReturn(mRr->BuildShaderTables(
		opaques,
		mCurrFrameResource->MaterialCB.Resource()->GetGPUVirtualAddress())
	);

	return true;
}

BOOL DxRenderer::DrawShadowMap() {
	const auto cmdList = mCommandList.Get();

	if (!bShadowEnabled) {
		if (!bShadowMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			const auto shadow = mShadowMap->Resource();

			shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			cmdList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);

			CheckHRESULT(cmdList->Close());
			mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

			bShadowMapCleanedUp = true;
		}
		
		return true;
	}
	bShadowMapCleanedUp = false;

	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto passCB = mCurrFrameResource->PassCB.Resource();
	auto passCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;

	auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	auto matCBAddress = mCurrFrameResource->MaterialCB.Resource()->GetGPUVirtualAddress();

	mShadowMap->Run(
		cmdList,
		passCBAddress,
		objCBAddress,
		matCBAddress,
		mhGpuDescForTexMaps,
		mRitemRefs[RenderType::E_Opaque]
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawGBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	const auto matCBAddress = mCurrFrameResource->MaterialCB.Resource()->GetGPUVirtualAddress();

	mGBuffer->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		passCBAddress,
		objCBAddress,
		matCBAddress,
		mhGpuDescForTexMaps,
		mRitemRefs[RenderType::E_Opaque]
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawSsao() {
	const auto cmdList = mCommandList.Get();

	if (!bSsaoEnabled) {
		if (!bSsaoMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			const auto aoCoeffMap = mSsao->AOCoefficientMapResource(0);
			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cmdList->ClearRenderTargetView(mSsao->AOCoefficientMapRtv(0), Ssao::AOCoefficientMapClearValues, 0, nullptr);

			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CheckHRESULT(cmdList->Close());
			mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

			bSsaoMapCleanedUp = true;
		}

		return true;
	}
	bSsaoMapCleanedUp = false;

	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	auto ssaoCBAddress = mCurrFrameResource->SsaoCB.Resource()->GetGPUVirtualAddress();
	mSsao->Run(
		cmdList,
		ssaoCBAddress,
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv()
	);

	const auto blurPassCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mBlurFilter->Run(
		cmdList,
		blurPassCBAddress,
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mSsao->AOCoefficientMapResource(0),
		mSsao->AOCoefficientMapResource(1),
		mSsao->AOCoefficientMapRtv(0),
		mSsao->AOCoefficientMapSrv(0),
		mSsao->AOCoefficientMapRtv(1),
		mSsao->AOCoefficientMapSrv(1),
		BlurFilter::FilterType::R16,
		3
	);	

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawBackBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mBRDF->CalcReflectanceWithoutSpecIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapRtv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->RMSMapSrv(),
		mGBuffer->PositionMapSrv(),
		mShadowMap->Srv(),
		mSsao->AOCoefficientMapSrv(0),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		BRDF::Render::E_Raster
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::IntegrateSpecIrrad() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto aoCoeffDesc = bRaytracing ? mRtao->ResolvedAOCoefficientSrv() : mSsao->AOCoefficientMapSrv(0);
	auto reflectionDesc = bRaytracing ? mRr->ReflectionMapSrv() : mSsr->SsrMapSrv(0);

	mBRDF->IntegrateSpecularIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapRtv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->RMSMapSrv(),
		mGBuffer->PositionMapSrv(),
		aoCoeffDesc,
		mIrradianceMap->PrefilteredEnvironmentCubeMapSrv(),
		mIrradianceMap->IntegratedBrdfMapSrv(),
		reflectionDesc
	);	

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawSkySphere() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	
	mIrradianceMap->DrawSkySphere(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mToneMapping->InterMediateMapRtv(),
		DepthStencilView(),
		passCBAddress,
		objCBAddress,
		objCBByteSize,
		mSkySphere
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawEquirectangulaToCube() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	mIrradianceMap->DrawCubeMap(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		passCBAddress,
		objCBAddress,
		objCBByteSize,
		mIrradianceCubeMap,
		ShaderArgs::IrradianceMap::MipLevel
	);
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplyTAA() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mTaa->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		mSwapChainBuffer->CurrentBackBufferSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::TemporalAA::ModulationFactor
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(&cmdList));

	return true;
}

BOOL DxRenderer::BuildSsr() {
	const auto cmdList = mCommandList.Get();

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	const auto ssrMap0 = mSsr->SsrMapResource(0);
	const auto ssrMap1 = mSsr->SsrMapResource(1);

	auto ssrCBAddress = mCurrFrameResource->SsrCB.Resource()->GetGPUVirtualAddress();

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	if (bSsrEnabled) {
		bSsrMapCleanedUp = false;

		CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

		ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
		cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mSsr->Build(
			cmdList,
			ssrCBAddress,
			backBuffer,
			backBufferSrv,
			mGBuffer->NormalMapSrv(),
			mGBuffer->DepthMapSrv()
		);

		auto blurPassCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
		mBlurFilter->Run(
			cmdList,
			blurPassCBAddress,
			ssrMap0,
			ssrMap1,
			mSsr->SsrMapRtv(0),
			mSsr->SsrMapSrv(0),
			mSsr->SsrMapRtv(1),
			mSsr->SsrMapSrv(1),
			BlurFilter::FilterType::R16G16B16A16,
			ShaderArgs::Ssr::BlurCount
		);

		CheckHRESULT(cmdList->Close());
		mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	}
	else {
		if (!bSsrMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			ssrMap0->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cmdList->ClearRenderTargetView(mSsr->SsrMapRtv(0), Ssr::ClearValues, 0, nullptr);

			ssrMap0->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CheckHRESULT(cmdList->Close());
			mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

			bSsrMapCleanedUp = true;
		};
	}

	return true;
}

BOOL DxRenderer::ApplyBloom() {
	const auto cmdList= mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	const auto bloomMap0 = mBloom->BloomMapResource(0);
	const auto bloomMap1 = mBloom->BloomMapResource(1);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mBloom->ExtractHighlights(
		cmdList,
		backBufferSrv,
		ShaderArgs::Bloom::HighlightThreshold
	);
	
	auto blurPassCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mBlurFilter->Run(
		cmdList,
		blurPassCBAddress,
		bloomMap0,
		bloomMap1,
		mBloom->BloomMapRtv(0),
		mBloom->BloomMapSrv(0),
		mBloom->BloomMapRtv(1),
		mBloom->BloomMapSrv(1),
		BlurFilter::FilterType::R16G16B16A16,
		ShaderArgs::Bloom::BlurCount
	);
	
	mBloom->Bloom(
		cmdList,
		backBufferSrv
	);

	auto resultMap = mBloom->ResultMapResource();
	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	mCommandList->CopyResource(backBuffer->Resource(), resultMap->Resource());

	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplyDepthOfField() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferRtv = mToneMapping->InterMediateMapRtv();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	const auto dofCBAddress = mCurrFrameResource->DofCB.Resource()->GetGPUVirtualAddress();
	mDof->CalcFocalDist(
		cmdList,
		mScreenViewport,
		mScissorRect,
		dofCBAddress,
		mGBuffer->DepthMapSrv()
	);

	mDof->CalcCoc(
		cmdList,
		mScreenViewport,
		mScissorRect,
		dofCBAddress,
		mGBuffer->DepthMapSrv()
	);

	mDof->ApplyDof(
		cmdList,
		mScreenViewport,
		mScissorRect,
		backBuffer,
		backBufferRtv,
		ShaderArgs::DepthOfField::BokehRadius,
		ShaderArgs::DepthOfField::CocThreshold,
		ShaderArgs::DepthOfField::CocDiffThreshold,
		ShaderArgs::DepthOfField::HighlightPower,
		ShaderArgs::DepthOfField::SampleCount
	);

	const auto blurCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mDof->BlurDof(
		cmdList,
		mScreenViewport,
		mScissorRect,
		blurCBAddress,
		backBuffer,
		backBufferRtv,
		backBufferSrv,
		ShaderArgs::DepthOfField::BlurCount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplyMotionBlur() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	mMotionBlur->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::MotionBlur::Intensity,
		ShaderArgs::MotionBlur::Limit,
		ShaderArgs::MotionBlur::DepthBias,
		ShaderArgs::MotionBlur::SampleCount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ResolveToneMapping() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	if (bToneMappingEnabled) {
		mToneMapping->Resolve(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBuffer(),
			mSwapChainBuffer->CurrentBackBufferRtv(),
			ShaderArgs::ToneMapping::Exposure
		);
	}
	else {
		mToneMapping->Resolve(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBuffer(),
			mSwapChainBuffer->CurrentBackBufferRtv()
		);
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplyGammaCorrection() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mGammaCorrection->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgs::GammaCorrection::Gamma
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplySharpen() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mSharpen->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgs::Sharpen::Amount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::ApplyPixelation() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mPixelation->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgs::Pixelization::PixelSize
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawDebuggingInfo() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mDebugMap->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mCurrFrameResource->DebugMapCB.Resource()->GetGPUVirtualAddress(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		DepthStencilView()
	);

	if (ShaderArgs::Debug::ShowCollisionBox) {
		mDebugCollision->Run(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBuffer(),
			mSwapChainBuffer->CurrentBackBufferRtv(),
			mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
			mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress(),
			mRitemRefs[RenderType::E_Opaque]
		);
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawImGui() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	const auto backBufferRtv = mSwapChainBuffer->CurrentBackBufferRtv();
	cmdList->OMSetRenderTargets(1, &backBufferRtv, true, nullptr);

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static const auto BuildDebugMap = [&](BOOL& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, DebugMap::SampleMask::Type type) {
		if (mode) { if (!mDebugMap->AddDebugMap(handle, type)) mode = false; }
		else { mDebugMap->RemoveDebugMap(handle); }
	};
	static const auto BuildDebugMapWithSampleDesc = [&](
			BOOL& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, DebugMap::SampleMask::Type type, DebugMapSampleDesc desc) {
		if (mode) { if (!mDebugMap->AddDebugMap(handle, type, desc)) mode = false; }
		else { mDebugMap->RemoveDebugMap(handle); }
	};

	{
		ImGui::Begin("Main Panel");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Debug")) {
			if (ImGui::TreeNode("Texture Maps")) {
				if (ImGui::TreeNode("G-Buffer")) {
					if (ImGui::Checkbox("Albedo", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Albedo]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Albedo],
							mGBuffer->AlbedoMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Normal", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Normal]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Normal],
							mGBuffer->NormalMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Depth", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Depth]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Depth],
							mGBuffer->DepthMapSrv(),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::Checkbox("RoughnessMetalicSpecular", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RMS]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_RMS],
							mGBuffer->RMSMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Velocity", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Velocity]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Velocity],
							mGBuffer->VelocityMapSrv(),
							DebugMap::SampleMask::RG);
					}
					ImGui::TreePop();
				} // ImGui::TreeNode("G-Buffer")
				if (ImGui::TreeNode("Irradiance")) {
					if (ImGui::Checkbox("Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Equirectangular]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Equirectangular],
							mIrradianceMap->EquirectangularMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Temporary Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular],
							mIrradianceMap->TemporaryEquirectangularMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Diffuse Irradiance Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect],
							mIrradianceMap->DiffuseIrradianceEquirectMapSrv(),
							DebugMap::SampleMask::RGB);
					}
					ImGui::TreePop(); // ImGui::TreeNode("Irradiance")
				} 
				if (ImGui::Checkbox("Bloom", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Bloom]))) {
					BuildDebugMap(
						mDebugMapStates[DebugMapLayout::E_Bloom],
						mBloom->BloomMapSrv(0),
						DebugMap::SampleMask::RGB);
				}

				if (ImGui::TreeNode("Rasterization")) {
					if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Shadow]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Shadow],
							mShadowMap->Srv(),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::Checkbox("SSAO", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSAO]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSAO],
							mSsao->AOCoefficientMapSrv(0),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::Checkbox("SSR", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSR]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSR],
							mSsr->SsrMapSrv(0),
							DebugMap::SampleMask::RGB);
					}

					ImGui::TreePop();
				}								
				if (ImGui::TreeNode("Raytracing")) {
					if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DxrShadow]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_DxrShadow],
							mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow0),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::TreeNode("RTAO")) {
						auto index = mRtao->TemporalCurrentFrameResourceIndex();

						if (ImGui::Checkbox("AO Coefficients", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_AOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_AOCoeff],
								mRtao->AOResourcesGpuDescriptors()[Rtao::Descriptor::AO::ES_AmbientCoefficient],
								DebugMap::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Temporal AO Coefficients",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff],
								mRtao->TemporalAOCoefficientsGpuDescriptors()[index][Rtao::Descriptor::TemporalAOCoefficient::Srv],
								DebugMap::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Tspp", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Tspp]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 153.0f / 255.0f, 18.0f / 255.0f, 15.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = 22.0f;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_Tspp],
								mRtao->TemporalCachesGpuDescriptors()[index][Rtao::Descriptor::TemporalCache::ES_Tspp],
								DebugMap::SampleMask::UINT,
								desc);
						}
						if (ImGui::Checkbox("Ray Hit Distance", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 15.0f / 255.0f, 18.0f / 255.0f, 153.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::Rtao::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RayHitDist],
								mRtao->AOResourcesGpuDescriptors()[Rtao::Descriptor::AO::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Temporal Ray Hit Distance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 12.0f / 255.0f, 64.0f / 255.0f, 18.0f / 255.0f, 1.0f };
							desc.MaxColor = { 180.0f / 255.0f, 197.0f / 255.0f, 231.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::Rtao::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist],
								mRtao->TemporalCachesGpuDescriptors()[index][Rtao::Descriptor::TemporalCache::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Local Mean Variance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_LocalMeanVariance]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_LocalMeanVariance],
								mRtao->LocalMeanVarianceResourcesGpuDescriptors()[Rtao::Descriptor::LocalMeanVariance::ES_Raw],
								DebugMap::SampleMask::RG);
						}
						if (ImGui::Checkbox("Disocclusion Blur Strength",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DiocclusionBlurStrength]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_DiocclusionBlurStrength],
								mRtao->DisocclusionBlurStrengthSrv(),
								DebugMap::SampleMask::RRR);
						}
						ImGui::TreePop();
					} // ImGui::TreeNode("RTAO")
					if (ImGui::Checkbox("Raytraced Reflection", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RaytracedReflection]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_RaytracedReflection],
							mRr->ReflectionMapSrv(),
							DebugMap::SampleMask::RGB);
					}
				
					ImGui::TreePop();
				}				
				ImGui::TreePop(); // ImGui::TreeNode("Texture Maps")
			} 
			ImGui::Checkbox("Show Collision Box", reinterpret_cast<bool*>(&ShaderArgs::Debug::ShowCollisionBox));
		}
		if (ImGui::CollapsingHeader("Effects")) {
			ImGui::Checkbox("TAA", reinterpret_cast<bool*>(&bTaaEnabled));
			ImGui::Checkbox("Motion Blur", reinterpret_cast<bool*>(&bMotionBlurEnabled));
			ImGui::Checkbox("Depth of Field", reinterpret_cast<bool*>(&bDepthOfFieldEnabled));
			ImGui::Checkbox("Bloom", reinterpret_cast<bool*>(&bBloomEnabled));
			ImGui::Checkbox("Gamma Correction", reinterpret_cast<bool*>(&bGammaCorrectionEnabled));
			ImGui::Checkbox("Tone Mapping", reinterpret_cast<bool*>(&bToneMappingEnabled));
			ImGui::Checkbox("Pixelation", reinterpret_cast<bool*>(&bPixelationEnabled));
			ImGui::Checkbox("Sharpen", reinterpret_cast<bool*>(&bSharpenEnabled));

			if (ImGui::TreeNode("Rasterization")) {
				ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(&bShadowEnabled));
				ImGui::Checkbox("SSAO", reinterpret_cast<bool*>(&bSsaoEnabled));
				ImGui::Checkbox("SSR", reinterpret_cast<bool*>(&bSsrEnabled));

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Raytracing")) {

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("Lights")) {
			if (ImGui::TreeNode("Directional Lights")) {
				ImGui::ColorPicker3("Strength", ShaderArgs::Light::DirectionalLight::Strength);
				ImGui::SliderFloat("Multiplier", &ShaderArgs::Light::DirectionalLight::Multiplier, 0, 100.0f);

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("BRDF")) {
			ImGui::RadioButton("Blinn-Phong", reinterpret_cast<INT*>(&mBRDF->ModelType), BRDF::Model::E_BlinnPhong); ImGui::SameLine();
			ImGui::RadioButton("Cook-Torrance", reinterpret_cast<INT*>(&mBRDF->ModelType), BRDF::Model::E_CookTorrance);
			
		}
		if (ImGui::CollapsingHeader("Environment")) {
			ImGui::Checkbox("Show Irradiance CubeMap", reinterpret_cast<bool*>(&ShaderArgs::IrradianceMap::ShowIrradianceCubeMap));
			if (ShaderArgs::IrradianceMap::ShowIrradianceCubeMap) {
				ImGui::RadioButton(
					"Environment CubeMap", 
					reinterpret_cast<INT*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_EnvironmentCube);
				ImGui::RadioButton(
					"Equirectangular Map", 
					reinterpret_cast<INT*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_Equirectangular);
				ImGui::RadioButton(
					"Diffuse Irradiance CubeMap",
					reinterpret_cast<INT*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_DiffuseIrradianceCube);
				ImGui::RadioButton(
					"Prefiltered Irradiance CubeMap",
					reinterpret_cast<INT*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_PrefilteredIrradianceCube);
				ImGui::SliderFloat("Mip Level", &ShaderArgs::IrradianceMap::MipLevel, 0.0f, IrradianceMap::MaxMipLevel - 1);
			}
		}

		ImGui::End();
	}
	{
		ImGui::Begin("Sub Panel");
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Pre Pass")) {
			if (ImGui::TreeNode("SSAO")) {
				ImGui::SliderInt("Sample Count", &ShaderArgs::Ssao::SampleCount, 1, 32);
				ImGui::SliderInt("Number of Blurs", &ShaderArgs::Ssao::BlurCount, 0, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("RTAO")) {
				ImGui::Checkbox("Use Smoothing Variance", reinterpret_cast<bool*>(&ShaderArgs::Rtao::Denoiser::UseSmoothingVariance));
				ImGui::Checkbox("Disocclusion Blur", reinterpret_cast<bool*>(&ShaderArgs::Rtao::Denoiser::DisocclusionBlur));
				ImGui::Checkbox("Clamp Cached Values",
					reinterpret_cast<bool*>(&ShaderArgs::Rtao::Denoiser::TemporalSupersampling::ClampCachedValues::UseClamping));

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("Post Pass")) {
			if (ImGui::TreeNode("TAA")) {
				ImGui::SliderFloat("Modulation Factor", &ShaderArgs::TemporalAA::ModulationFactor, 0.1f, 0.9f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Motion Blur")) {
				ImGui::SliderFloat("Intensity", &ShaderArgs::MotionBlur::Intensity, 0.01f, 0.1f);
				ImGui::SliderFloat("Limit", &ShaderArgs::MotionBlur::Limit, 0.001f, 0.01f);
				ImGui::SliderFloat("Depth Bias", &ShaderArgs::MotionBlur::DepthBias, 0.001f, 0.01f);
				ImGui::SliderInt("Sample Count", &ShaderArgs::MotionBlur::SampleCount, 1, 32);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Depth of Field")) {
				ImGui::SliderFloat("Focus Range", &ShaderArgs::DepthOfField::FocusRange, 0.1f, 100.0f);
				ImGui::SliderFloat("Focusing Speed", &ShaderArgs::DepthOfField::FocusingSpeed, 1.0f, 10.0f);
				ImGui::SliderFloat("Bokeh Radius", &ShaderArgs::DepthOfField::BokehRadius, 1.0f, 8.0f);
				ImGui::SliderFloat("CoC Threshold", &ShaderArgs::DepthOfField::CocThreshold, 0.01f, 0.9f);
				ImGui::SliderFloat("CoC Diff Threshold", &ShaderArgs::DepthOfField::CocDiffThreshold, 0.1f, 0.9f);
				ImGui::SliderFloat("Highlight Power", &ShaderArgs::DepthOfField::HighlightPower, 1.0f, 32.0f);
				ImGui::SliderInt("Sample Count", &ShaderArgs::DepthOfField::SampleCount, 1, 8);
				ImGui::SliderInt("Blur Count", &ShaderArgs::DepthOfField::BlurCount, 0, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Bloom")) {
				ImGui::SliderInt("Blur Count", &ShaderArgs::Bloom::BlurCount, 0, 8);
				ImGui::SliderFloat("Highlight Threshold", &ShaderArgs::Bloom::HighlightThreshold, 0.1f, 0.99f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("SSR")) {
				ImGui::SliderFloat("Max Distance", &ShaderArgs::Ssr::MaxDistance, 1.0f, 100.0f);
				ImGui::SliderFloat("Ray Length", &ShaderArgs::Ssr::RayLength, 0.1f, 2.0f);
				ImGui::SliderFloat("Noise Intensity", &ShaderArgs::Ssr::NoiseIntensity, 0.1f, 0.001f);
				ImGui::SliderInt("Step Count", &ShaderArgs::Ssr::StepCount, 1, 32);
				ImGui::SliderInt("Back Step Count", &ShaderArgs::Ssr::BackStepCount, 1, 16);
				ImGui::SliderInt("Blur Count", &ShaderArgs::Ssr::BlurCount, 0, 8);
				ImGui::SliderFloat("Depth Threshold", &ShaderArgs::Ssr::DepthThreshold, 0.1f, 10.0f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Tone Mapping")) {
				ImGui::SliderFloat("Exposure", &ShaderArgs::ToneMapping::Exposure, 0.1f, 10.0f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Gamma Correction")) {
				ImGui::SliderFloat("Gamma", &ShaderArgs::GammaCorrection::Gamma, 0.1f, 10.0f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Pixelization")) {
				ImGui::SliderFloat("Pixel Size", &ShaderArgs::Pixelization::PixelSize, 1.0f, 20.0f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Sharpen")) {
				ImGui::SliderFloat("Amount", &ShaderArgs::Sharpen::Amount, 0.0f, 10.0f);

				ImGui::TreePop();
			}
			
		}

		ImGui::End();
	}
	{
		ImGui::Begin("Reference Panel");
		ImGui::NewLine();

		if (mPickedRitem != nullptr) {
			auto mat = mPickedRitem->Material;
			ImGui::Text(mPickedRitem->Geometry->Name.c_str());

			FLOAT albedo[4] = { mat->Albedo.x, mat->Albedo.y, mat->Albedo.z, mat->Albedo.w };
			if (ImGui::ColorPicker4("Albedo", albedo)) {
				mat->Albedo.x = albedo[0];
				mat->Albedo.y = albedo[1];
				mat->Albedo.z = albedo[2];
				mat->Albedo.w = albedo[3];
			}			
			ImGui::SliderFloat("Roughness", &mat->Roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Metalic", &mat->Metailic, 0.0f, 1.0f);
			ImGui::SliderFloat("Specular", &mat->Specular, 0.0f, 1.0f);
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawDxrShadowMap() {
	const auto cmdList = mCommandList.Get();	
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();	
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	mDxrShadowMap->Run(
		cmdList,
		mTLAS->Result->GetGPUVirtualAddress(),
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mGBuffer->PositionMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mClientWidth, mClientHeight
	);
	
	mBlurFilterCS->Run(
		cmdList,
		mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mDxrShadowMap->Resource(DxrShadowMap::Resources::EShadow0),
		mDxrShadowMap->Resource(DxrShadowMap::Resources::EShadow1),
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow0),
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::EU_Shadow0),
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow1),
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::EU_Shadow1),
		BlurFilterCS::Filter::R16,
		mClientWidth, mClientHeight,
		ShaderArgs::DxrShadowMap::BlurCount
	);
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawDxrBackBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mBRDF->CalcReflectanceWithoutSpecIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapRtv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->RMSMapSrv(),
		mGBuffer->PositionMapSrv(),
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow0),
		mRtao->ResolvedAOCoefficientSrv(),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		BRDF::Render::E_Raytrace
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::DrawRtao() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto& aoResources = mRtao->AOResources();
	const auto& aoResourcesGpuDescriptors = mRtao->AOResourcesGpuDescriptors();
	const auto& temporalCaches = mRtao->TemporalCaches();
	const auto& temporalCachesGpuDescriptors = mRtao->TemporalCachesGpuDescriptors();
	const auto& temporalAOCoefficients = mRtao->TemporalAOCoefficients();
	const auto& temporalAOCoefficientsGpuDescriptors = mRtao->TemporalAOCoefficientsGpuDescriptors();
	const auto& localMeanVarianceResources = mRtao->LocalMeanVarianceResources();
	const auto& localMeanVarianceResourcesGpuDescriptors = mRtao->LocalMeanVarianceResourcesGpuDescriptors();
	const auto& varianceResources = mRtao->AOVarianceResources();
	const auto& varianceResourcesGpuDescriptors = mRtao->AOVarianceResourcesGpuDescriptors();

	// Calculate ambient occlusion.
	{
		const auto& ambientCoefficient = aoResources[Rtao::Resource::AO::E_AmbientCoefficient].get();
		{
			ambientCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			D3D12Util::UavBarrier(cmdList, ambientCoefficient);
		}

		mRtao->RunCalculatingAmbientOcclusion(
			cmdList,
			mTLAS->Result->GetGPUVirtualAddress(),
			mCurrFrameResource->RtaoCB.Resource()->GetGPUVirtualAddress(),
			mGBuffer->NormalDepthMapSrv(),
			mGBuffer->DepthMapSrv(),
			aoResourcesGpuDescriptors[Rtao::Descriptor::AO::EU_AmbientCoefficient],
			aoResourcesGpuDescriptors[Rtao::Descriptor::AO::EU_RayHitDistance],
			mClientWidth, mClientHeight
		);
		{
			ambientCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			D3D12Util::UavBarrier(cmdList, ambientCoefficient);
		}
	}
	// Calculate partial-derivatives.
	{
		const auto depthPartialDerivative = mRtao->DepthPartialDerivativeMapResource();

		depthPartialDerivative->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		D3D12Util::UavBarrier(cmdList, depthPartialDerivative);

		mRtao->RunCalculatingDepthPartialDerivative(
			cmdList,
			mGBuffer->DepthMapSrv(),
			mRtao->DepthPartialDerivativeUav(),
			mClientWidth, mClientHeight
		);

		depthPartialDerivative->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		D3D12Util::UavBarrier(cmdList, depthPartialDerivative);
	}
	// Denosing(Spatio-Temporal Variance Guided Filtering)
	{
		// Stage 1: Reverse reprojection
		{
			UINT temporalPreviousFrameResourceIndex = mRtao->TemporalCurrentFrameResourceIndex();
			UINT temporalCurrentFrameResourcIndex = mRtao->MoveToNextFrame();

			UINT temporalPreviousFrameTemporalAOCoefficientResourceIndex = mRtao->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
			UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRtao->MoveToNextFrameTemporalAOCoefficient();

			const auto currTsppMap = temporalCaches[temporalCurrentFrameResourcIndex][Rtao::Resource::TemporalCache::E_Tspp].get();
			const auto tsppCoefficientSquaredMeanRayHitDistance = mRtao->TsppCoefficientSquaredMeanRayHitDistance();

			currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			tsppCoefficientSquaredMeanRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			std::vector<GpuResource*> resources = { currTsppMap, tsppCoefficientSquaredMeanRayHitDistance };
			D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());

			// Retrieves values from previous frame via reverse reprojection.				
			mRtao->ReverseReprojectPreviousFrame(
				cmdList,
				mCurrFrameResource->CrossBilateralFilterCB.Resource()->GetGPUVirtualAddress(),
				mGBuffer->NormalDepthMapSrv(),
				mRtao->DepthPartialDerivativeSrv(),
				mGBuffer->ReprojNormalDepthMapSrv(),
				mRtao->PrevFrameNormalDepthSrv(),
				mGBuffer->VelocityMapSrv(),
				temporalAOCoefficientsGpuDescriptors[temporalPreviousFrameTemporalAOCoefficientResourceIndex][Rtao::Descriptor::TemporalAOCoefficient::Srv],
				temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][Rtao::Descriptor::TemporalCache::ES_Tspp],
				temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][Rtao::Descriptor::TemporalCache::ES_CoefficientSquaredMean],
				temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][Rtao::Descriptor::TemporalCache::ES_RayHitDistance],
				temporalCachesGpuDescriptors[temporalCurrentFrameResourcIndex][Rtao::Descriptor::TemporalCache::EU_Tspp],
				mRtao->TsppCoefficientSquaredMeanRayHitDistanceUav(),
				mClientWidth, mClientHeight
			);

			currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			tsppCoefficientSquaredMeanRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());

			// Copy the current normal and depth values to the cached map.
			{
				const auto normal = mGBuffer->NormalDepthMapResource();
				const auto prevFrameNormalDepth = mRtao->PrevFrameNormalDepth();
			
				normal->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
				prevFrameNormalDepth->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
			
				cmdList->CopyResource(prevFrameNormalDepth->Resource(), normal->Resource());
			
				normal->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				prevFrameNormalDepth->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
		}
		// Stage 2: Blending current frame value with the reprojected cachec value
		{
			// Calculate local mean and variance for clamping during the blending operation.
			{
				const auto rawLocalMeanVariance = localMeanVarianceResources[Rtao::Resource::LocalMeanVariance::E_Raw].get();
				rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);

				mRtao->RunCalculatingLocalMeanVariance(
					cmdList,
					mCurrFrameResource->CalcLocalMeanVarCB.Resource()->GetGPUVirtualAddress(),
					aoResourcesGpuDescriptors[Rtao::Descriptor::AO::ES_AmbientCoefficient],
					localMeanVarianceResourcesGpuDescriptors[Rtao::Descriptor::LocalMeanVariance::EU_Raw],
					mClientWidth, mClientHeight,
					false //bCheckerboardSamplingEnabled
				);

				rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);

				// Interpolate the variance for the inactive cells from the valid checkerboard cells.
				if (false) { // bCheckerboardSamplingEnabled
					rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);

					mRtao->FillInCheckerboard(
						cmdList,
						mCurrFrameResource->CalcLocalMeanVarCB.Resource()->GetGPUVirtualAddress(),
						localMeanVarianceResourcesGpuDescriptors[Rtao::Descriptor::LocalMeanVariance::EU_Raw],
						mClientWidth, mClientHeight
					);


					rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);
				}
			}

			// Blends reprojected values with current frame values.
			// Inactive pixels are filtered from active neighbors on checkerboard sampling before the blending operation.
			{
				UINT temporalCurrentFrameResourceIndex = mRtao->TemporalCurrentFrameResourceIndex();
				UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRtao->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

				const auto currTemporalAOCoefficient = temporalAOCoefficients[temporalCurrentFrameTemporalAOCoefficientResourceIndex].get();
				const auto currTemporalSupersampling = temporalCaches[temporalCurrentFrameResourceIndex][Rtao::Resource::TemporalCache::E_Tspp].get();
				const auto currCoefficientSquaredMean = temporalCaches[temporalCurrentFrameResourceIndex][Rtao::Resource::TemporalCache::E_CoefficientSquaredMean].get();
				const auto currRayHitDistance = temporalCaches[temporalCurrentFrameResourceIndex][Rtao::Resource::TemporalCache::E_RayHitDistance].get();
				const auto rawVariance = varianceResources[Rtao::Resource::AOVariance::E_Raw].get();
				const auto diocclusionBlurStrength = mRtao->DisocclusionBlurStrengthResource();

				std::vector<GpuResource*> resources = {
					currTemporalAOCoefficient,
					currTemporalSupersampling,
					currCoefficientSquaredMean,
					currRayHitDistance,
					rawVariance,
					diocclusionBlurStrength
				};

				currTemporalAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				currCoefficientSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				rawVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				diocclusionBlurStrength->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());

				mRtao->BlendWithCurrentFrame(
					cmdList,
					mCurrFrameResource->TsppBlendCB.Resource()->GetGPUVirtualAddress(),
					aoResourcesGpuDescriptors[Rtao::Descriptor::AO::ES_AmbientCoefficient],
					localMeanVarianceResourcesGpuDescriptors[Rtao::Descriptor::LocalMeanVariance::ES_Raw],
					aoResourcesGpuDescriptors[Rtao::Descriptor::AO::ES_RayHitDistance],
					mRtao->TsppCoefficientSquaredMeanRayHitDistanceSrv(),
					temporalAOCoefficientsGpuDescriptors[temporalCurrentFrameTemporalAOCoefficientResourceIndex][Rtao::Descriptor::TemporalAOCoefficient::Uav],
					temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][Rtao::Descriptor::TemporalCache::EU_Tspp],
					temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][Rtao::Descriptor::TemporalCache::EU_CoefficientSquaredMean],
					temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][Rtao::Descriptor::TemporalCache::EU_RayHitDistance],
					varianceResourcesGpuDescriptors[Rtao::Descriptor::AOVariance::EU_Raw],
					mRtao->DisocclusionBlurStrengthUav(),
					mClientWidth, mClientHeight
				);

				currTemporalAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				currCoefficientSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				rawVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				diocclusionBlurStrength->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());
			}

			if (ShaderArgs::Rtao::Denoiser::UseSmoothingVariance) {
				const auto smoothedVariance = varianceResources[Rtao::Resource::AOVariance::E_Smoothed].get();

				smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, smoothedVariance);

				mGaussianFilter->Run(
					cmdList,
					varianceResourcesGpuDescriptors[Rtao::Descriptor::AOVariance::ES_Raw],
					varianceResourcesGpuDescriptors[Rtao::Descriptor::AOVariance::EU_Smoothed],
					GaussianFilter::Filter3x3,
					mClientWidth, mClientHeight
				);

				smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, smoothedVariance);
			}
		}
		// Applies a single pass of a Atrous wavelet transform filter.
		{
			UINT temporalCurrentFrameResourceIndex = mRtao->TemporalCurrentFrameResourceIndex();
			UINT inputAOCoefficientIndex = mRtao->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
			UINT outputAOCoefficientIndex = mRtao->MoveToNextFrameTemporalAOCoefficient();
			
			const auto outputAOCoefficient = temporalAOCoefficients[outputAOCoefficientIndex].get();
			
			outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			D3D12Util::UavBarrier(cmdList, outputAOCoefficient);
			
			mRtao->ApplyAtrousWaveletTransformFilter(
				cmdList,
				mCurrFrameResource->AtrousFilterCB.Resource()->GetGPUVirtualAddress(),
				temporalAOCoefficientsGpuDescriptors[inputAOCoefficientIndex][Rtao::Descriptor::TemporalAOCoefficient::Srv],
				mGBuffer->NormalDepthMapSrv(),
				varianceResourcesGpuDescriptors[ShaderArgs::Rtao::Denoiser::UseSmoothingVariance ?
				Rtao::Descriptor::AOVariance::ES_Smoothed : Rtao::Descriptor::AOVariance::ES_Raw],
				temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][Rtao::Descriptor::TemporalCache::ES_RayHitDistance],
				mRtao->DepthPartialDerivativeSrv(),
				temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][Rtao::Descriptor::TemporalCache::ES_Tspp],
				temporalAOCoefficientsGpuDescriptors[outputAOCoefficientIndex][Rtao::Descriptor::TemporalAOCoefficient::Uav],
				mClientWidth, mClientHeight
			);
			
			outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			D3D12Util::UavBarrier(cmdList, outputAOCoefficient);
		}
		if (ShaderArgs::Rtao::Denoiser::DisocclusionBlur) {
			UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRtao->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

			const auto aoCoefficient = temporalAOCoefficients[temporalCurrentFrameTemporalAOCoefficientResourceIndex].get();

			aoCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			D3D12Util::UavBarrier(cmdList, aoCoefficient);

			mRtao->BlurDisocclusion(
				cmdList,
				aoCoefficient,
				mGBuffer->DepthMapSrv(),
				mRtao->DisocclusionBlurStrengthSrv(),
				temporalAOCoefficientsGpuDescriptors[temporalCurrentFrameTemporalAOCoefficientResourceIndex][Rtao::Descriptor::TemporalAOCoefficient::Uav],
				mClientWidth, mClientHeight,
				ShaderArgs::Rtao::Denoiser::LowTsppBlurPasses
			);

			aoCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			D3D12Util::UavBarrier(cmdList, aoCoefficient);
		}
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}

BOOL DxRenderer::BuildRaytracedReflection() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto rrCBAddress = mCurrFrameResource->RrCB.Resource()->GetGPUVirtualAddress();

	mRr->Run(
		cmdList,
		passCBAddress,
		rrCBAddress,
		mTLAS->Result->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->PositionMapSrv(),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		mRtao->ResolvedAOCoefficientSrv(),
		mIrradianceMap->PrefilteredEnvironmentCubeMapSrv(),
		mIrradianceMap->IntegratedBrdfMapSrv(),
		mhGpuDescForTexMaps,
		mClientWidth, mClientHeight,
		ShaderArgs::RaytracedReflection::ReflectionRadius
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return true;
}
