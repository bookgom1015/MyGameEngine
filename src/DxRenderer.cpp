#include "DxRenderer.h"
#include "Logger.h"
#include "FrameResource.h"
#include "MathHelper.h"
#include "MeshImporter.h"
#include "GeometryGenerator.h"
#include "ShaderManager.h"
#include "Shadow.h"
#include "GBuffer.h"
#include "SSAO.h"
#include "Blur.h"
#include "TemporalAA.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "Bloom.h"
#include "SSR.h"
#include "BRDF.h"
#include "Samplers.h"
#include "BlurFilter.h"
#include "DebugMap.h"
#include "ImGuiManager.h"
#include "DXR_Shadow.h"
#include "DXR_GeometryBuffer.h"
#include "BlurFilterCS.h"
#include "RTAO.h"
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
#include "SVGF.h"
#include "EquirectangularConverter.h"

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
	namespace GBuffer {
		namespace Dither {
			FLOAT MaxDistance = 0.5f;
			FLOAT MinDistance = 0.1f;
		}
	}

	namespace Bloom {
		FLOAT HighlightThreshold = 0.99f;
		INT BlurCount = 3;
	}

	namespace DepthOfField {
		FLOAT FocusRange = 8.0f;
		FLOAT FocusingSpeed = 8.0f;
		FLOAT BokehRadius = 2.0f;
		FLOAT CoCMaxDevTolerance = 0.8f;
		FLOAT HighlightPower = 4.0f;
		INT SampleCount = 4;
		INT BlurCount = 1;
	}

	namespace SSR {
		FLOAT MaxDistance = 32.0f;
		INT BlurCount = 3;

		namespace View {
			FLOAT RayLength = 0.5f;
			FLOAT NoiseIntensity = 0.01f;
			INT StepCount = 16;
			INT BackStepCount = 8;
			FLOAT DepthThreshold = 1.0f;
		}
		
		namespace Screen {
			FLOAT Resolution = 0.25f;
			FLOAT Thickness = 0.5f;
		}
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

	namespace SSAO {
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

	namespace SVGF {
		BOOL QuarterResolutionAO = FALSE;

		BOOL CheckerboardGenerateRaysForEvenPixels = FALSE;
		BOOL CheckerboardSamplingEnabled = FALSE;

		namespace TemporalSupersampling {
			UINT MaxTspp = 33;

			namespace ClampCachedValues {
				BOOL UseClamping = TRUE;
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
			BOOL UseSmoothedVariance = FALSE;
			BOOL PerspectiveCorrectDepthInterpolation = TRUE;
			BOOL UseAdaptiveKernelSize = TRUE;
			BOOL KernelRadiusRotateKernelEnabled = TRUE;
			INT KernelRadiusRotateKernelNumCycles = 3;
			INT FilterMinKernelWidth = 3;
			FLOAT FilterMaxKernelWidthPercentage = 1.5f;
			FLOAT AdaptiveKernelSizeRayHitDistanceScaleFactor = 0.02f;
			FLOAT AdaptiveKernelSizeRayHitDistanceScaleExponent = 2.0f;

		}
	}

	namespace RTAO {
		FLOAT OcclusionRadius = 22.0f;
		FLOAT OcclusionFadeStart = 1.0f;
		FLOAT OcclusionFadeEnd = 22.0f;
		FLOAT OcclusionEpsilon = 0.05f;
		UINT SampleCount = 2;

		namespace Denoiser {
			BOOL UseSmoothingVariance = TRUE;
			BOOL FullscreenBlur = TRUE;
			BOOL DisocclusionBlur = TRUE;
			UINT LowTsppBlurPasses = 3;
		}
	}

	namespace RaytracedReflection {
		BOOL CheckerboardSamplingEnabled = FALSE;

		FLOAT ReflectionRadius = 22.0f;

		namespace Denoiser {
			BOOL UseSmoothingVariance = TRUE;
			BOOL FullscreenBlur = TRUE;
			BOOL DisocclusionBlur = TRUE;
			UINT LowTsppBlurPasses = 4;
		}
	}
}

DxRenderer::DxRenderer() {
	bIsCleanedUp = false;

	mMainPassCB = std::make_unique<ConstantBuffer_Pass>();

	mShaderManager = std::make_unique<ShaderManager>();

	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	FLOAT widthSquared = 32.0f * 32.0f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);

	mImGui = std::make_unique<ImGuiManager>();

	mBRDF = std::make_unique<BRDF::BRDFClass>();
	mGBuffer = std::make_unique<GBuffer::GBufferClass>();
	mShadow = std::make_unique<Shadow::ShadowClass>();
	mSSAO = std::make_unique<SSAO::SSAOClass>();
	mBlurFilter = std::make_unique<BlurFilter::BlurFilterClass>();
	mBloom = std::make_unique<Bloom::BloomClass>();
	mSSR = std::make_unique<SSR::SSRClass>();
	mDoF = std::make_unique<DepthOfField::DepthOfFieldClass>();
	mMotionBlur = std::make_unique<MotionBlur::MotionBlurClass>();
	mTAA = std::make_unique<TemporalAA::TemporalAAClass>();
	mDebugMap = std::make_unique<DebugMap::DebugMapClass>();
	mDebugCollision = std::make_unique<DebugCollision::DebugCollisionClass>();
	mGammaCorrection = std::make_unique<GammaCorrection::GammaCorrectionClass>();
	mToneMapping = std::make_unique<ToneMapping::ToneMappingClass>();
	mIrradianceMap = std::make_unique<IrradianceMap::IrradianceMapClass>();
	mMipmapGenerator = std::make_unique<MipmapGenerator::MipmapGeneratorClass>();
	mPixelation = std::make_unique<Pixelation::PixelationClass>();
	mSharpen = std::make_unique<Sharpen::SharpenClass>();
	mGaussianFilter = std::make_unique<GaussianFilter::GaussianFilterClass>();
	mTLAS = std::make_unique<AccelerationStructureBuffer>();
	mDxrShadow = std::make_unique<DXR_Shadow::DXR_ShadowClass>();
	mBlurFilterCS = std::make_unique<BlurFilterCS::BlurFilterCSClass>();
	mRTAO = std::make_unique<RTAO::RTAOClass>();
	mRR = std::make_unique<RaytracedReflection::RaytracedReflectionClass>();
	mSVGF = std::make_unique<SVGF::SVGFClass>();
	mEquirectangularConverter = std::make_unique<EquirectangularConverter::EquirectangularConverterClass>();

	auto blurWeights = Blur::CalcGaussWeights(2.5f);
	mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	bInitiatingTaa = TRUE;

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

BOOL DxRenderer::Initialize(HWND hwnd, void* glfwWnd, UINT width, UINT height) {
	Logln(std::to_string(sizeof(ConstantBuffer_Pass)));
	Logln(std::to_string(D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Pass))));
	Logln(std::to_string(sizeof(Light)));

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
	CheckReturn(mShadow->Initialize(device, shaderManager, width, height, 4096, 4096));
	CheckReturn(mSSAO->Initialize(device, cmdList, shaderManager, width, height, SSAO::Resolution::E_Quarter));
	CheckReturn(mBlurFilter->Initialize(device, shaderManager));
	CheckReturn(mBloom->Initialize(device, shaderManager, width, height, Bloom::Resolution::E_OneSixteenth));
	CheckReturn(mSSR->Initialize(device, shaderManager, width, height, SSR::Resolution::E_Quarter));
	CheckReturn(mDoF->Initialize(device, shaderManager, cmdList, width, height));
	CheckReturn(mMotionBlur->Initialize(device, shaderManager, width, height));
	CheckReturn(mTAA->Initialize(device, shaderManager, width, height));
	CheckReturn(mDebugMap->Initialize(device, shaderManager));
	CheckReturn(mBlurFilterCS->Initialize(device, shaderManager));
	CheckReturn(mDebugCollision->Initialize(device, shaderManager));
	CheckReturn(mGammaCorrection->Initialize(device, shaderManager, width, height));
	CheckReturn(mToneMapping->Initialize(device, shaderManager, width, height));
	CheckReturn(mIrradianceMap->Initialize(device, cmdList, shaderManager));
	CheckReturn(mMipmapGenerator->Initialize(device, cmdList, shaderManager));
	CheckReturn(mPixelation->Initialize(device, shaderManager, width, height));
	CheckReturn(mSharpen->Initialize(device, shaderManager, width, height));
	CheckReturn(mGaussianFilter->Initialize(device, shaderManager));
	CheckReturn(mDxrShadow->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mRTAO->Initialize(device, shaderManager, width, height));
	CheckReturn(mRR->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mSVGF->Initialize(device, shaderManager, width, height));
	CheckReturn(mEquirectangularConverter->Initialize(device, shaderManager));

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

	bInitialized = TRUE;
#ifdef _DEBUG
	WLogln(L"Succeeded to initialize d3d12 renderer \n");
#endif

	{
		Light light;
		light.Type = LightType::E_Directional;
		light.Direction = { 0.577f, -0.577f, 0.577f };
		light.LightColor = { 240.0f / 255.0f, 235.0f / 255.0f, 223.0f / 255.0f };
		light.Intensity = 1.802f;

		mLights[mLightCount] = light;
		++mLightCount;
	}
	{
		Light light;
		light.Type = LightType::E_Directional;
		light.Direction = { 0.067f, -0.701f, -0.836f };
		light.LightColor = { 149.0f / 255.0f, 142.0f/ 255.0f, 100.0f / 255.0f };
		light.Intensity = 1.534f;

		mLights[mLightCount] = light;
		++mLightCount;
	}

	return TRUE;
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

	CheckReturn(UpdateCB_Main(delta));
	CheckReturn(UpdateCB_Blur(delta));
	CheckReturn(UpdateCB_DoF(delta));
	CheckReturn(UpdateCB_Objects(delta));
	CheckReturn(UpdateCB_Materials(delta));
	if (bNeedToUpdate_Irrad) CheckReturn(UpdateCB_Irradiance(delta));
	CheckReturn(UpdateCB_DebugMap(delta));

	if (bRaytracing) {
		CheckReturn(UpdateCB_SVGF(delta));
		CheckReturn(UpdateCB_RTAO(delta));
	}
	else {
		CheckReturn(UpdateCB_SSAO(delta));
		CheckReturn(UpdateCB_SSR(delta));
	}
	
	CheckReturn(UpdateShadingObjects(delta));

	return TRUE;
}

BOOL DxRenderer::Draw() {
	CheckHRESULT(mCurrFrameResource->CmdListAlloc->Reset());
	
	// Pre-pass
	{
		CheckReturn(DrawGBuffer());
		if (bRaytracing) {
			CheckReturn(DrawDXRShadow());
			CheckReturn(DrawRTAO());
		}
		else {
			CheckReturn(DrawShadow());
			CheckReturn(DrawSSAO());
		}
	}
	// Main-pass
	{
		if (bRaytracing) {
			CheckReturn(DrawDXRBackBuffer());
			CheckReturn(CalcDepthPartialDerivative());
		}
		else {
			CheckReturn(DrawBackBuffer());
		}
	}
	// Post-pass
	{
		CheckReturn(DrawSkySphere());
	
		if (bRaytracing) { CheckReturn(BuildRaytracedReflection()); }
		else { CheckReturn(ApplySSR()); }
	
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

	return TRUE;
}

BOOL DxRenderer::OnResize(UINT width, UINT height) {
	bool bNeedToReszie = mClientWidth != width || mClientHeight != height;

	mClientWidth = width;
	mClientHeight = height;
	
	CheckReturn(LowOnResize(width, height));
	
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (UINT i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}

	CheckReturn(mSwapChainBuffer->OnResize());
	if (bNeedToReszie) {
		CheckReturn(mBRDF->OnResize(width, height));
		CheckReturn(mGBuffer->OnResize(width, height));
		CheckReturn(mDoF->OnResize(cmdList, width, height));
		CheckReturn(mMotionBlur->OnResize(width, height));
		CheckReturn(mTAA->OnResize(width, height));
		CheckReturn(mGammaCorrection->OnResize(width, height));
		CheckReturn(mToneMapping->OnResize(width, height));
		CheckReturn(mPixelation->OnResize(width, height));
		CheckReturn(mSharpen->OnResize(width, height));
		CheckReturn(mSSR->OnResize(width, height));

		CheckReturn(mDxrShadow->OnResize(cmdList, width, height));
		CheckReturn(mRTAO->OnResize(width, height));
		CheckReturn(mRR->OnResize(cmdList, width, height));
		CheckReturn(mSVGF->OnResize(width , height))
	}
	CheckReturn(mSSAO->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));


	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}
	
	return TRUE;
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

	return TRUE;
}

BOOL DxRenderer::SetEquirectangularMap(const std::string& file) {	
	mIrradianceMap->SetEquirectangularMap(mCommandQueue.Get(), file);

	return TRUE;
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
		+ SSAO::NumRenderTargets
		+ DepthOfField::NumRenderTargets
		+ Bloom::NumRenderTargets
		+ SSR::NumRenderTargets
		+ ToneMapping::NumRenderTargets
		+ IrradianceMap::NumRenderTargets;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + Shadow::NumDepthStenciles;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = MAX_DESCRIPTOR_SIZE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvUavHeap)));

	return TRUE;
}

BOOL DxRenderer::CompileShaders() {
#ifdef _DEBUG
	WLogln(L"Compiling shaders...");
#endif 

	CheckReturn(mBRDF->CompileShaders(ShaderFilePath));
	CheckReturn(mGBuffer->CompileShaders(ShaderFilePath));
	CheckReturn(mShadow->CompileShaders(ShaderFilePath));
	CheckReturn(mSSAO->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilter->CompileShaders(ShaderFilePath));
	CheckReturn(mBloom->CompileShaders(ShaderFilePath));
	CheckReturn(mSSR->CompileShaders(ShaderFilePath));
	CheckReturn(mDoF->CompileShaders(ShaderFilePath));
	CheckReturn(mMotionBlur->CompileShaders(ShaderFilePath));
	CheckReturn(mTAA->CompileShaders(ShaderFilePath));
	CheckReturn(mDebugMap->CompileShaders(ShaderFilePath));
	CheckReturn(mDebugCollision->CompileShaders(ShaderFilePath));
	CheckReturn(mGammaCorrection->CompileShaders(ShaderFilePath));
	CheckReturn(mToneMapping->CompileShaders(ShaderFilePath));
	CheckReturn(mIrradianceMap->CompileShaders(ShaderFilePath));
	CheckReturn(mMipmapGenerator->CompileShaders(ShaderFilePath));
	CheckReturn(mPixelation->CompileShaders(ShaderFilePath));
	CheckReturn(mSharpen->CompileShaders(ShaderFilePath));
	CheckReturn(mGaussianFilter->CompileShaders(ShaderFilePath));
	CheckReturn(mDxrShadow->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilterCS->CompileShaders(ShaderFilePath));
	CheckReturn(mRTAO->CompileShaders(ShaderFilePath));
	CheckReturn(mRR->CompileShaders(ShaderFilePath));
	CheckReturn(mSVGF->CompileShaders(ShaderFilePath));
	CheckReturn(mEquirectangularConverter->CompileShaders(ShaderFilePath));

#ifdef _DEBUG
	WLogln(L"Finished compiling shaders \n");
#endif 

	return TRUE;
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

	return TRUE;
}

BOOL DxRenderer::BuildFrameResources() {
	for (INT i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 1, MaxLights /* Shadows */,
			32 /* Objects */, 32 /* Materials */));
		CheckReturn(mFrameResources.back()->Initialize());
	}

	return TRUE;
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

	hCpu.Offset(-1, descSize);
	hGpu.Offset(-1, descSize);
	hCpuDsv.Offset(-1, dsvDescSize);
	hCpuRtv.Offset(-1, rtvDescSize);

	mImGui->BuildDescriptors(hCpu, hGpu, descSize);
	mSwapChainBuffer->BuildDescriptors(hCpu, hGpu, descSize);
	mBRDF->BuildDescriptors(hCpu, hGpu, descSize);
	mGBuffer->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mShadow->BuildDescriptors(hCpu, hGpu, hCpuDsv, descSize, dsvDescSize);
	mSSAO->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mBloom->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSSR->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mDoF->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mMotionBlur->BuildDescriptors(hCpu, hGpu, descSize);
	mTAA->BuildDescriptors(hCpu, hGpu, descSize);
	mGammaCorrection->BuildDescriptors(hCpu, hGpu, descSize);
	mToneMapping->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mIrradianceMap->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mPixelation->BuildDescriptors(hCpu, hGpu, descSize);
	mSharpen->BuildDescriptors(hCpu, hGpu, descSize);

	mDxrShadow->BuildDescriptors(hCpu, hGpu, descSize);
	mRTAO->BuildDescriptors(hCpu, hGpu, descSize);
	mRR->BuildDesscriptors(hCpu, hGpu, descSize);
	mSVGF->BuildDescriptors(hCpu, hGpu, descSize);

	mhCpuDescForTexMaps = hCpu.Offset(1, descSize);
	mhGpuDescForTexMaps = hGpu.Offset(1, descSize);
}

BOOL DxRenderer::BuildRootSignatures() {
#if _DEBUG
	WLogln(L"Building root-signatures...");
#endif

	auto staticSamplers = Samplers::GetStaticSamplers();
	CheckReturn(mBRDF->BuildRootSignature(staticSamplers));
	CheckReturn(mGBuffer->BuildRootSignature(staticSamplers));
	CheckReturn(mShadow->BuildRootSignature(staticSamplers));
	CheckReturn(mSSAO->BuildRootSignature(staticSamplers));
	CheckReturn(mBlurFilter->BuildRootSignature(staticSamplers));
	CheckReturn(mBloom->BuildRootSignature(staticSamplers));
	CheckReturn(mSSR->BuildRootSignature(staticSamplers));
	CheckReturn(mDoF->BuildRootSignature(staticSamplers));
	CheckReturn(mMotionBlur->BuildRootSignature(staticSamplers));
	CheckReturn(mTAA->BuildRootSignature(staticSamplers));
	CheckReturn(mDebugMap->BuildRootSignature(staticSamplers));
	CheckReturn(mDebugCollision->BuildRootSignature());
	CheckReturn(mGammaCorrection->BuildRootSignature(staticSamplers));
	CheckReturn(mToneMapping->BuildRootSignature(staticSamplers));
	CheckReturn(mIrradianceMap->BuildRootSignature(staticSamplers));
	CheckReturn(mMipmapGenerator->BuildRootSignature(staticSamplers));
	CheckReturn(mPixelation->BuildRootSignature(staticSamplers));
	CheckReturn(mSharpen->BuildRootSignature(staticSamplers));
	CheckReturn(mGaussianFilter->BuildRootSignature(staticSamplers));
	CheckReturn(mDxrShadow->BuildRootSignatures(staticSamplers, DXR_GeometryBuffer::GeometryBufferCount));
	CheckReturn(mBlurFilterCS->BuildRootSignature(staticSamplers));
	CheckReturn(mRTAO->BuildRootSignatures(staticSamplers));
	CheckReturn(mRR->BuildRootSignatures(staticSamplers));
	CheckReturn(mSVGF->BuildRootSignatures(staticSamplers));
	CheckReturn(mEquirectangularConverter->BuildRootSignature(staticSamplers));

#if _DEBUG
	WLogln(L"Finished building root-signatures \n");
#endif

	return TRUE;
}

BOOL DxRenderer::BuildPSOs() {
#ifdef _DEBUG
	WLogln(L"Building pipeline state objects...");
#endif

	CheckReturn(mBRDF->BuildPSO());
	CheckReturn(mGBuffer->BuildPSO());
	CheckReturn(mShadow->BuildPSO());
	CheckReturn(mSSAO->BuildPSO());
	CheckReturn(mBloom->BuildPSO());
	CheckReturn(mBlurFilter->BuildPSO());
	CheckReturn(mSSR->BuildPSO());
	CheckReturn(mDoF->BuildPSO());
	CheckReturn(mMotionBlur->BuildPSO());
	CheckReturn(mTAA->BuildPSO());
	CheckReturn(mDebugMap->BuildPSO());
	CheckReturn(mDebugCollision->BuildPSO());
	CheckReturn(mGammaCorrection->BuildPSO());
	CheckReturn(mToneMapping->BuildPSO());
	CheckReturn(mIrradianceMap->BuildPSO());
	CheckReturn(mMipmapGenerator->BuildPSO());
	CheckReturn(mPixelation->BuildPSO());
	CheckReturn(mSharpen->BuildPSO());
	CheckReturn(mGaussianFilter->BuildPSO());	
	CheckReturn(mDxrShadow->BuildPSO());
	CheckReturn(mBlurFilterCS->BuildPSO());
	CheckReturn(mRTAO->BuildPSO());
	CheckReturn(mRR->BuildPSO());
	CheckReturn(mSVGF->BuildPSO());
	CheckReturn(mEquirectangularConverter->BuildPSO());

#ifdef _DEBUG
	WLogln(L"Finished building pipeline state objects \n");
#endif

	return TRUE;
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

	return TRUE;
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

	return TRUE;
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
		mEquirectangularConverter.get(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Irradiance.Resource()->GetGPUVirtualAddress(),
		mMipmapGenerator.get()
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

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Main(FLOAT delta) {
	// Shadow
	{
		mMainPassCB->LightCount = mLightCount;

		for (UINT i = 0; i < mLightCount; ++i) {
			if (i >= MaxLights) continue;

			auto& light = mLights[i];

			if (light.Type == LightType::E_Directional) {
				XMVECTOR lightDir = XMLoadFloat3(&light.Direction);
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
				XMStoreFloat4x4(&light.ShadowTransform0, XMMatrixTranspose(S));

				XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);
				XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(lightView), lightView);
				XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(lightProj), lightProj);
				XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

				XMStoreFloat4x4(&light.ViewProj, XMMatrixTranspose(viewProj));
				XMStoreFloat3(&light.Position, lightPos);
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].View, XMMatrixTranspose(lightView));
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].InvView, XMMatrixTranspose(invView));
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].Proj, XMMatrixTranspose(lightProj));
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].InvProj, XMMatrixTranspose(invProj));
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].ViewProj, XMMatrixTranspose(viewProj));
				//XMStoreFloat4x4(&mMainPassCB->Lights[i].InvViewProj, XMMatrixTranspose(invViewProj));
				//XMStoreFloat3(&mMainPassCB->Lights[i].Position, lightPos);
			}
			else if (light.Type == LightType::E_Spot) {

			}
			else if (light.Type == LightType::E_Point) {
				auto proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
				auto pos = XMLoadFloat3(&light.Position);
				pos.m128_f32[3] = 1.0f;
				
				// Positive +X
				{
					auto target = pos + XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
					auto view_px = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
					auto vp_px = view_px * proj;
					XMStoreFloat4x4(&light.ShadowTransform0, XMMatrixTranspose(vp_px));
				}
				// Positive -X
				{
					auto target = pos + XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);
					auto view_nx = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
					auto vp_nx = view_nx * proj;
					XMStoreFloat4x4(&light.ShadowTransform1, XMMatrixTranspose(vp_nx));
				}
				// Positive +Y
				{
					auto target = pos + XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
					auto view_py = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)));
					auto vp_py = view_py * proj;
					XMStoreFloat4x4(&light.ShadowTransform2, XMMatrixTranspose(vp_py));
				}
				// Positive -Y
				{
					auto target = pos + XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
					auto view_ny = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));
					auto vp_ny = view_ny * proj;
					XMStoreFloat4x4(&light.ShadowTransform3, XMMatrixTranspose(vp_ny));
				}
				// Positive +Z
				{
					auto target = pos + XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
					auto view_pz = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
					auto vp_pz = view_pz * proj;
					XMStoreFloat4x4(&light.ShadowTransform4, XMMatrixTranspose(vp_pz));
				}
				// Positive -Z
				{
					auto target = pos + XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
					auto view_nz = XMMatrixTranspose(XMMatrixLookAtLH(pos, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
					auto vp_nz = view_nz * proj;
					XMStoreFloat4x4(&light.ShadowTransform5, XMMatrixTranspose(vp_nz));
				}
			}

			mMainPassCB->Lights[i] = light;
		}
	}
	// Main
	{
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

		auto& currCB = mCurrFrameResource->CB_Pass;
		currCB.CopyData(0, *mMainPassCB);
	}

	return TRUE;
}

BOOL DxRenderer::UpdateCB_SSAO(FLOAT delta) {
	ConstantBuffer_SSAO ssaoCB;
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

	mSSAO->GetOffsetVectors(ssaoCB.OffsetVectors);

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 2.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	ssaoCB.SampleCount = ShaderArgs::SSAO::SampleCount;

	auto& currCB = mCurrFrameResource->CB_SSAO;
	currCB.CopyData(0, ssaoCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Blur(FLOAT delta) {
	ConstantBuffer_Blur blurCB;
	blurCB.Proj = mMainPassCB->Proj;
	blurCB.BlurWeights[0] = mBlurWeights[0];
	blurCB.BlurWeights[1] = mBlurWeights[1];
	blurCB.BlurWeights[2] = mBlurWeights[2];
	blurCB.BlurRadius = 5;

	auto& currCB = mCurrFrameResource->CB_Blur;
	currCB.CopyData(0, blurCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_DoF(FLOAT delta) {
	ConstantBuffer_DoF dofCB;
	dofCB.Proj = mMainPassCB->Proj;
	dofCB.InvProj = mMainPassCB->InvProj;
	dofCB.FocusRange = ShaderArgs::DepthOfField::FocusRange;
	dofCB.FocusingSpeed = ShaderArgs::DepthOfField::FocusingSpeed;
	dofCB.DeltaTime = delta;

	auto& currCB = mCurrFrameResource->CB_DoF;
	currCB.CopyData(0, dofCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_SSR(FLOAT delta) {
	ConstantBuffer_SSR ssrCB;
	ssrCB.View = mMainPassCB->View;
	ssrCB.InvView = mMainPassCB->InvView;
	ssrCB.Proj = mMainPassCB->Proj;
	ssrCB.InvProj = mMainPassCB->InvProj;
	XMStoreFloat3(&ssrCB.EyePosW, mCamera->GetPosition());
	ssrCB.MaxDistance = ShaderArgs::SSR::MaxDistance;
	ssrCB.RayLength = ShaderArgs::SSR::View::RayLength;
	ssrCB.NoiseIntensity = ShaderArgs::SSR::View::NoiseIntensity;
	ssrCB.StepCount = ShaderArgs::SSR::View::StepCount;
	ssrCB.BackStepCount = ShaderArgs::SSR::View::BackStepCount;
	ssrCB.DepthThreshold = ShaderArgs::SSR::View::DepthThreshold;
	ssrCB.Thickness = ShaderArgs::SSR::Screen::Thickness;
	ssrCB.Resolution = ShaderArgs::SSR::Screen::Resolution;

	auto& currCB = mCurrFrameResource->CB_SSR;
	currCB.CopyData(0, ssrCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Objects(FLOAT delta) {
	auto& currCB = mCurrFrameResource->CB_Object;
	for (auto& e : mRitems) {
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ConstantBuffer_Object objCB;
			objCB.PrevWorld = e->PrevWolrd;
			XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objCB.TexTransform, XMMatrixTranspose(texTransform));
			objCB.Center = { e->AABB.Center.x, e->AABB.Center.y, e->AABB.Center.z, 1.0f };
			objCB.Extents = { e->AABB.Extents.x, e->AABB.Extents.y, e->AABB.Extents.z, 0.0f };

			e->PrevWolrd = objCB.World;

			currCB.CopyData(e->ObjCBIndex, objCB);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;

		}
	}

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Materials(FLOAT delta) {
	auto& currCB = mCurrFrameResource->CB_Material;
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		auto* mat = e.second.get();
		//if (mat->NumFramesDirty > 0) {
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			ConstantBuffer_Material matCB;
			matCB.DiffuseSrvIndex = mat->DiffuseSrvHeapIndex;
			matCB.Albedo = mat->Albedo;
			matCB.Roughness = mat->Roughness;
			matCB.Metalic = mat->Metailic;
			matCB.Specular = mat->Specular;
			XMStoreFloat4x4(&matCB.MatTransform, XMMatrixTranspose(matTransform));

			currCB.CopyData(mat->MatCBIndex, matCB);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Irradiance(FLOAT delta) {
	ConstantBuffer_Irradiance irradCB;
		
	XMStoreFloat4x4(&irradCB.Proj, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f)));

	// Positive +X
	XMStoreFloat4x4(
		&irradCB.View[0],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive -X
	XMStoreFloat4x4(
		&irradCB.View[1],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive +Y
	XMStoreFloat4x4(
		&irradCB.View[2],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)
		))
	);
	// Positive -Y
	XMStoreFloat4x4(
		&irradCB.View[3],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
		))
	);
	// Positive +Z
	XMStoreFloat4x4(
		&irradCB.View[4],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	// Positive -Z
	XMStoreFloat4x4(
		&irradCB.View[5],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		))
	);
	
	auto& currCB = mCurrFrameResource->CB_Irradiance;
	currCB.CopyData(0, irradCB);

	bNeedToUpdate_Irrad = FALSE;

	return TRUE;
}

BOOL DxRenderer::UpdateCB_SVGF(FLOAT delta) {
	// Calculate local mean/variance
	{
		ConstantBuffer_CalcLocalMeanVariance calcLocalMeanVarCB;

		ShaderArgs::SVGF::CheckerboardGenerateRaysForEvenPixels = !ShaderArgs::SVGF::CheckerboardGenerateRaysForEvenPixels;

		calcLocalMeanVarCB.TextureDim = { mClientWidth, mClientHeight };
		calcLocalMeanVarCB.KernelWidth = 9;
		calcLocalMeanVarCB.KernelRadius = 9 >> 1;

		calcLocalMeanVarCB.CheckerboardSamplingEnabled = ShaderArgs::SVGF::CheckerboardSamplingEnabled;
		calcLocalMeanVarCB.EvenPixelActivated = ShaderArgs::SVGF::CheckerboardGenerateRaysForEvenPixels;
		calcLocalMeanVarCB.PixelStepY = ShaderArgs::SVGF::CheckerboardSamplingEnabled ? 2 : 1;

		auto& currCB = mCurrFrameResource->CB_CalcLocalMeanVar;
		currCB.CopyData(0, calcLocalMeanVarCB);
	}
	// Temporal supersampling reverse reproject
	{
		ConstantBuffer_CrossBilateralFilter filterCB;
		filterCB.DepthSigma = 1.0f;
		filterCB.DepthNumMantissaBits = D3D12Util::NumMantissaBitsInFloatFormat(16);

		auto& currCB = mCurrFrameResource->CB_CrossBilateralFilter;
		currCB.CopyData(0, filterCB);
	}
	// Temporal supersampling blend with current frame
	{
		ConstantBuffer_TemporalSupersamplingBlendWithCurrentFrame tsppBlendCB;
		tsppBlendCB.StdDevGamma = ShaderArgs::SVGF::TemporalSupersampling::ClampCachedValues::StdDevGamma;
		tsppBlendCB.ClampCachedValues = ShaderArgs::SVGF::TemporalSupersampling::ClampCachedValues::UseClamping;
		tsppBlendCB.ClampingMinStdDevTolerance = ShaderArgs::SVGF::TemporalSupersampling::ClampCachedValues::MinStdDevTolerance;

		tsppBlendCB.ClampDifferenceToTsppScale = ShaderArgs::SVGF::TemporalSupersampling::ClampDifferenceToTsppScale;
		tsppBlendCB.ForceUseMinSmoothingFactor = false;
		tsppBlendCB.MinSmoothingFactor = 1.0f / ShaderArgs::SVGF::TemporalSupersampling::MaxTspp;
		tsppBlendCB.MinTsppToUseTemporalVariance = ShaderArgs::SVGF::TemporalSupersampling::MinTsppToUseTemporalVariance;

		tsppBlendCB.BlurStrengthMaxTspp = ShaderArgs::SVGF::TemporalSupersampling::LowTsppMaxTspp;
		tsppBlendCB.BlurDecayStrength = ShaderArgs::SVGF::TemporalSupersampling::LowTsppDecayConstant;
		tsppBlendCB.CheckerboardEnabled = ShaderArgs::SVGF::CheckerboardSamplingEnabled;
		tsppBlendCB.CheckerboardEvenPixelActivated = ShaderArgs::SVGF::CheckerboardGenerateRaysForEvenPixels;

		auto& currCB = mCurrFrameResource->CB_TSPPBlend;
		currCB.CopyData(0, tsppBlendCB);
	}
	// Atrous wavelet transform filter
	{
		ConstantBuffer_AtrousWaveletTransformFilter atrousFilterCB;

		// Adaptive kernel radius rotation.
		FLOAT kernelRadiusLerfCoef = 0;
		if (ShaderArgs::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelEnabled) {
			static UINT frameID = 0;
			UINT i = frameID++ % ShaderArgs::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles;
			kernelRadiusLerfCoef = i / static_cast<FLOAT>(ShaderArgs::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles);
		}

		atrousFilterCB.TextureDim = XMUINT2(mClientWidth, mClientHeight);
		atrousFilterCB.DepthWeightCutoff = ShaderArgs::SVGF::AtrousWaveletTransformFilter::DepthWeightCutoff;
		atrousFilterCB.UsingBilateralDownsamplingBuffers = ShaderArgs::SVGF::QuarterResolutionAO;

		atrousFilterCB.UseAdaptiveKernelSize = ShaderArgs::SVGF::AtrousWaveletTransformFilter::UseAdaptiveKernelSize;
		atrousFilterCB.KernelRadiusLerfCoef = kernelRadiusLerfCoef;
		atrousFilterCB.MinKernelWidth = ShaderArgs::SVGF::AtrousWaveletTransformFilter::FilterMinKernelWidth;
		atrousFilterCB.MaxKernelWidth = static_cast<UINT>((ShaderArgs::SVGF::AtrousWaveletTransformFilter::FilterMaxKernelWidthPercentage / 100) * mClientWidth);

		atrousFilterCB.PerspectiveCorrectDepthInterpolation = ShaderArgs::SVGF::AtrousWaveletTransformFilter::PerspectiveCorrectDepthInterpolation;
		atrousFilterCB.MinVarianceToDenoise = ShaderArgs::SVGF::AtrousWaveletTransformFilter::MinVarianceToDenoise;

		atrousFilterCB.ValueSigma = ShaderArgs::SVGF::AtrousWaveletTransformFilter::ValueSigma;
		atrousFilterCB.DepthSigma = ShaderArgs::SVGF::AtrousWaveletTransformFilter::DepthSigma;
		atrousFilterCB.NormalSigma = ShaderArgs::SVGF::AtrousWaveletTransformFilter::NormalSigma;
		atrousFilterCB.FovY = mCamera->FovY();

		atrousFilterCB.DepthNumMantissaBits = D3D12Util::NumMantissaBitsInFloatFormat(16);

		auto& currCB = mCurrFrameResource->CB_AtrousFilter;
		currCB.CopyData(0, atrousFilterCB);
	}

	return TRUE;
}

BOOL DxRenderer::UpdateCB_RTAO(FLOAT delta) {
	static UINT count = 0;
	static auto prev = mMainPassCB->View;

	ConstantBuffer_RTAO rtaoCB;
	rtaoCB.View = mMainPassCB->View;
	rtaoCB.InvView = mMainPassCB->InvView;
	rtaoCB.Proj = mMainPassCB->Proj;
	rtaoCB.InvProj = mMainPassCB->InvProj;

	// Coordinates given in view space.
	rtaoCB.OcclusionRadius = ShaderArgs::RTAO::OcclusionRadius;
	rtaoCB.OcclusionFadeStart = ShaderArgs::RTAO::OcclusionFadeStart;
	rtaoCB.OcclusionFadeEnd = ShaderArgs::RTAO::OcclusionFadeEnd;
	rtaoCB.SurfaceEpsilon = ShaderArgs::RTAO::OcclusionEpsilon;

	rtaoCB.FrameCount = count++;
	rtaoCB.SampleCount = ShaderArgs::RTAO::SampleCount;

	prev = mMainPassCB->View;

	auto& currCB = mCurrFrameResource->CB_RTAO;
	currCB.CopyData(0, rtaoCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_DebugMap(FLOAT delta) {
	ConstantBuffer_DebugMap debugMapCB;

	for (UINT i = 0; i < DebugMap::MapSize; ++i) {
		auto desc = mDebugMap->SampleDesc(i);
		debugMapCB.SampleDescs[i] = desc;
	}

	auto& currCB = mCurrFrameResource->CB_DebugMap;
	currCB.CopyData(0, debugMapCB);

	return TRUE;
}

BOOL DxRenderer::AddBLAS(ID3D12GraphicsCommandList4* const cmdList, MeshGeometry* const geo) {
	std::unique_ptr<AccelerationStructureBuffer> blas = std::make_unique<AccelerationStructureBuffer>();

	CheckReturn(blas->BuildBLAS(md3dDevice.Get(), cmdList, geo));

	mBLASRefs[geo->Name] = blas.get();
	mBLASes.emplace_back(std::move(blas));
	
	bNeedToRebuildTLAS = true;
	bNeedToRebuildShaderTables = true;

	return TRUE;
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

	return TRUE;
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

	return TRUE;
}

BOOL DxRenderer::BuildShaderTables() {
	const auto& opaques = mRitemRefs[RenderType::E_Opaque];
	const auto objCBAddress = mCurrFrameResource->CB_Object.Resource()->GetGPUVirtualAddress();
	const auto matCBAddress = mCurrFrameResource->CB_Material.Resource()->GetGPUVirtualAddress();
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object));
	UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Material));

	UINT numRitems = static_cast<UINT>(opaques.size());
	CheckReturn(mDxrShadow->BuildShaderTables(numRitems));
	CheckReturn(mRTAO->BuildShaderTables(numRitems));
	CheckReturn(mRR->BuildShaderTables(
		opaques, 
		objCBAddress, 
		matCBAddress, 
		objCBByteSize,
		matCBByteSize
	));	

	return TRUE;
}

BOOL DxRenderer::DrawShadow() {
	const auto cmdList = mCommandList.Get();

	if (!bShadowEnabled) {
		if (!bShadowMapCleanedUp) {

		}
		
		return TRUE;
	}
	bShadowMapCleanedUp = FALSE;

	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	for (UINT i = 0; i < mLightCount; ++i) {
		if (i >= MaxLights) continue;

		mShadow->Run(
			cmdList,
			mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
			mCurrFrameResource->CB_Object.Resource()->GetGPUVirtualAddress(),
			mCurrFrameResource->CB_Material.Resource()->GetGPUVirtualAddress(),
			D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object)),
			D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Material)),
			mGBuffer->PositionMapSrv(),
			mhGpuDescForTexMaps,
			mRitemRefs[RenderType::E_Opaque],
			mLights[i].Type == LightType::E_Point,
			i
		);
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawGBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Copy the previous frame normal and depth values to the cached map.
	{
		const auto nd = mGBuffer->NormalDepthMapResource();
		const auto prevNd = mGBuffer->PrevNormalDepthMapResource();

		nd->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		prevNd->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

		cmdList->CopyResource(prevNd->Resource(), nd->Resource());

		nd->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		prevNd->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	mGBuffer->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Object.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Material.Resource()->GetGPUVirtualAddress(),
		D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object)),
		D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Material)),
		mhGpuDescForTexMaps,
		mRitemRefs[RenderType::E_Opaque],
		ShaderArgs::GBuffer::Dither::MaxDistance,
		ShaderArgs::GBuffer::Dither::MinDistance
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawSSAO() {
	const auto cmdList = mCommandList.Get();

	if (!bSsaoEnabled) {
		if (!bSsaoMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			const auto aoCoeffMap = mSSAO->AOCoefficientMapResource(0);
			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cmdList->ClearRenderTargetView(mSSAO->AOCoefficientMapRtv(0), SSAO::AOCoefficientMapClearValues, 0, nullptr);

			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CheckHRESULT(cmdList->Close());
			mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

			bSsaoMapCleanedUp = TRUE;
		}

		return TRUE;
	}
	bSsaoMapCleanedUp = FALSE;

	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	auto ssaoCBAddr = mCurrFrameResource->CB_SSAO.Resource()->GetGPUVirtualAddress();
	mSSAO->Run(
		cmdList,
		ssaoCBAddr,
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv()
	);

	const auto blurCBAddr = mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress();
	mBlurFilter->Run(
		cmdList,
		blurCBAddr,
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mSSAO->AOCoefficientMapResource(0),
		mSSAO->AOCoefficientMapResource(1),
		mSSAO->AOCoefficientMapRtv(0),
		mSSAO->AOCoefficientMapSrv(0),
		mSSAO->AOCoefficientMapRtv(1),
		mSSAO->AOCoefficientMapSrv(1),
		BlurFilter::FilterType::R16,
		3
	);	

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawBackBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mBRDF->CalcReflectanceWithoutSpecIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapRtv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->RMSMapSrv(),
		mGBuffer->PositionMapSrv(),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		mSSAO->AOCoefficientMapSrv(0),
		mShadow->Srv(Shadow::Descriptor::ESI_Shadow),
		BRDF::Render::E_Raster
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::IntegrateSpecIrrad() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto aoCoeffDesc = bRaytracing ? mRTAO->ResolvedAOCoefficientSrv() : mSSAO->AOCoefficientMapSrv(0);
	auto reflectionDesc = bRaytracing ? mRR->ResolvedReflectionSrv() : mSSR->SSRMapSrv(0);

	mBRDF->IntegrateSpecularIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
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

	return TRUE;
}

BOOL DxRenderer::DrawSkySphere() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mIrradianceMap->DrawSkySphere(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mToneMapping->InterMediateMapRtv(),
		DepthStencilView(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Object.Resource()->GetGPUVirtualAddress(),
		D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object)),
		mSkySphere
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawEquirectangulaToCube() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
		
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mIrradianceMap->DrawCubeMap(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		ShaderArgs::IrradianceMap::MipLevel
	);
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplyTAA() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mTAA->Run(
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

	return TRUE;
}

BOOL DxRenderer::ApplySSR() {
	const auto cmdList = mCommandList.Get();

	if (bSsrEnabled) {
		bSsrMapCleanedUp = FALSE;

		CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

		const auto descHeap = mCbvSrvUavHeap.Get();
		cmdList->SetDescriptorHeaps(1, &descHeap);

		auto ssrCBAddr = mCurrFrameResource->CB_SSR.Resource()->GetGPUVirtualAddress();

		mSSR->Run(
			cmdList,
			ssrCBAddr,
			mToneMapping->InterMediateMapResource(),
			mToneMapping->InterMediateMapSrv(),
			mGBuffer->PositionMapSrv(),
			mGBuffer->NormalMapSrv(),
			mGBuffer->DepthMapSrv(),
			mGBuffer->RMSMapSrv()
		);

		auto blurCBAddr = mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress();
		mBlurFilter->Run(
			cmdList,
			blurCBAddr,
			mSSR->SSRMapResource(0),
			mSSR->SSRMapResource(1),
			mSSR->SSRMapRtv(0),
			mSSR->SSRMapSrv(0),
			mSSR->SSRMapRtv(1),
			mSSR->SSRMapSrv(1),
			BlurFilter::FilterType::R16G16B16A16,
			ShaderArgs::SSR::BlurCount
		);

		CheckHRESULT(cmdList->Close());
		mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(&cmdList));
	}
	else {
		if (!bSsrMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			cmdList->ClearRenderTargetView(mSSR->SSRMapRtv(0), SSR::ClearValues, 0, nullptr);

			CheckHRESULT(cmdList->Close());
			mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(&cmdList));

			bSsrMapCleanedUp = TRUE;
		};
	}

	return TRUE;
}

BOOL DxRenderer::ApplyBloom() {
	const auto cmdList= mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	const auto bloomMap0 = mBloom->BloomMapResource(0);
	const auto bloomMap1 = mBloom->BloomMapResource(1);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mBloom->ExtractHighlight(
		cmdList,
		backBufferSrv,
		ShaderArgs::Bloom::HighlightThreshold
	);
	
	auto blurPassCBAddress = mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress();
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
	
	mBloom->ApplyBloom(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mToneMapping->InterMediateMapRtv()
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplyDepthOfField() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferRtv = mToneMapping->InterMediateMapRtv();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	const auto dofCBAddr = mCurrFrameResource->CB_DoF.Resource()->GetGPUVirtualAddress();
	mDoF->CalcFocalDist(
		cmdList,
		mScreenViewport,
		mScissorRect,
		dofCBAddr,
		mGBuffer->DepthMapSrv()
	);

	mDoF->CalcCoC(
		cmdList,
		mScreenViewport,
		mScissorRect,
		dofCBAddr,
		mGBuffer->DepthMapSrv()
	);

	mDoF->ApplyDoF(
		cmdList,
		mScreenViewport,
		mScissorRect,
		backBuffer,
		backBufferRtv,
		ShaderArgs::DepthOfField::BokehRadius,
		ShaderArgs::DepthOfField::CoCMaxDevTolerance,
		ShaderArgs::DepthOfField::HighlightPower,
		ShaderArgs::DepthOfField::SampleCount
	);

	mDoF->BlurDoF(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress(),
		backBuffer,
		backBufferRtv,
		backBufferSrv,
		ShaderArgs::DepthOfField::BlurCount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
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
		mSwapChainBuffer->CurrentBackBufferRtv(),
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

	return TRUE;
}

BOOL DxRenderer::ResolveToneMapping() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	return TRUE;
}

BOOL DxRenderer::ApplyGammaCorrection() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	return TRUE;
}

BOOL DxRenderer::ApplySharpen() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	return TRUE;
}

BOOL DxRenderer::ApplyPixelation() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	return TRUE;
}

BOOL DxRenderer::DrawDebuggingInfo() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mDebugMap->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mCurrFrameResource->CB_DebugMap.Resource()->GetGPUVirtualAddress(),
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
			mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
			mCurrFrameResource->CB_Object.Resource()->GetGPUVirtualAddress(),
			D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object)),
			mRitemRefs[RenderType::E_Opaque]
		);
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
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
		if (mode) { if (!mDebugMap->AddDebugMap(handle, type)) mode = FALSE; }
		else { mDebugMap->RemoveDebugMap(handle); }
	};
	static const auto BuildDebugMapWithSampleDesc = [&](
			BOOL& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, DebugMap::SampleMask::Type type, DebugMapSampleDesc desc) {
		if (mode) { if (!mDebugMap->AddDebugMap(handle, type, desc)) mode = FALSE; }
		else { mDebugMap->RemoveDebugMap(handle); }
	};

	// Main Panel
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
					if (ImGui::TreeNode("Shadow")) {
						auto debug = mShadow->DebugShadowMap();
						if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(debug))) {
							BuildDebugMap(
								*debug,
								mShadow->Srv(Shadow::Descriptor::ESI_Shadow),
								DebugMap::SampleMask::RRR);
						}

						ImGui::TreePop();
					}
					if (ImGui::Checkbox("SSAO", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSAO]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSAO],
							mSSAO->AOCoefficientMapSrv(0),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::Checkbox("SSR", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSR]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSR],
							mSSR->SSRMapSrv(0),
							DebugMap::SampleMask::RGB);
					}

					ImGui::TreePop();
				}								
				if (ImGui::TreeNode("Raytracing")) {
					if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DxrShadow]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_DxrShadow],
							mDxrShadow->Descriptor(),
							DebugMap::SampleMask::RRR);
					}
					if (ImGui::TreeNode("RTAO")) {
						auto index = mRTAO->TemporalCurrentFrameResourceIndex();

						if (ImGui::Checkbox("AO Coefficients", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_AOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_AOCoeff],
								mRTAO->AOResourceGpuDescriptors()[RTAO::Descriptor::AO::ES_AmbientCoefficient],
								DebugMap::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Temporal AO Coefficients",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff],
								mRTAO->TemporalAOCoefficientGpuDescriptors()[index][RTAO::Descriptor::TemporalAOCoefficient::Srv],
								DebugMap::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Tspp", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Tspp]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 153.0f / 255.0f, 18.0f / 255.0f, 15.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = 22.0f;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_Tspp],
								mRTAO->TemporalCacheGpuDescriptors()[index][RTAO::Descriptor::TemporalCache::ES_Tspp],
								DebugMap::SampleMask::UINT,
								desc);
						}
						if (ImGui::Checkbox("Ray Hit Distance", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 15.0f / 255.0f, 18.0f / 255.0f, 153.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::RTAO::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RayHitDist],
								mRTAO->AOResourceGpuDescriptors()[RTAO::Descriptor::AO::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Temporal Ray Hit Distance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 12.0f / 255.0f, 64.0f / 255.0f, 18.0f / 255.0f, 1.0f };
							desc.MaxColor = { 180.0f / 255.0f, 197.0f / 255.0f, 231.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::RTAO::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist],
								mRTAO->TemporalCacheGpuDescriptors()[index][RTAO::Descriptor::TemporalCache::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}

						ImGui::TreePop(); // ImGui::TreeNode("RTAO")
					}
					if (ImGui::TreeNode("Raytraced Reflection")) {
						auto index = mRR->TemporalCurrentFrameResourceIndex();

						if (ImGui::Checkbox("Reflection", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_Reflection]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_RR_Reflection],
								mRR->ReflectionMapSrv(),
								DebugMap::SampleMask::RGB);
						}
						if (ImGui::Checkbox("Temporal Reflection", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_TemporalReflection]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_RR_TemporalReflection],
								mRR->ResolvedReflectionSrv(),
								DebugMap::SampleMask::RGB);
						}
						if (ImGui::Checkbox("Tspp", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_Tspp]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 153.0f / 255.0f, 18.0f / 255.0f, 15.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = 22.0f;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_Tspp],
								mRR->TemporalCacheGpuDescriptors()[index][RaytracedReflection::Descriptor::TemporalCache::ES_Tspp],
								DebugMap::SampleMask::UINT,
								desc);
						}
						if (ImGui::Checkbox("Ray Hit Distance", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_RayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 15.0f / 255.0f, 18.0f / 255.0f, 153.0f / 255.0f, 1.0f };
							desc.MaxColor = { 170.0f / 255.0f, 220.0f / 255.0f, 200.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::RaytracedReflection::ReflectionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_RayHitDist],
								mRR->ReflectionGpuDescriptors()[RaytracedReflection::Descriptor::Reflection::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Temporal Ray Hit Distance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_TemporalRayHitDist]))) {
							DebugMapSampleDesc desc;
							desc.MinColor = { 12.0f / 255.0f, 64.0f / 255.0f, 18.0f / 255.0f, 1.0f };
							desc.MaxColor = { 180.0f / 255.0f, 197.0f / 255.0f, 231.0f / 255.0f, 1.0f };
							desc.Denominator = ShaderArgs::RaytracedReflection::ReflectionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_TemporalRayHitDist],
								mRR->TemporalCacheGpuDescriptors()[index][RaytracedReflection::Descriptor::TemporalCache::ES_RayHitDistance],
								DebugMap::SampleMask::FLOAT,
								desc);
						}

						ImGui::TreePop(); // ImGui::TreeNode("Raytraced Reflection")
					}
				
					ImGui::TreePop(); // ImGui::TreeNode("RTAO")
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
	;// Sub Panel
	{
		ImGui::Begin("Sub Panel");
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Pre Pass")) {
			if (ImGui::TreeNode("SSAO")) {
				ImGui::SliderInt("Sample Count", &ShaderArgs::SSAO::SampleCount, 1, 32);
				ImGui::SliderInt("Number of Blurs", &ShaderArgs::SSAO::BlurCount, 0, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("SVGF")) {
				ImGui::Checkbox("Clamp Cached Values", reinterpret_cast<bool*>(&ShaderArgs::SVGF::TemporalSupersampling::ClampCachedValues::UseClamping));

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("RTAO")) {
				ImGui::SliderInt("Sample Count", reinterpret_cast<int*>(&ShaderArgs::RTAO::SampleCount), 1, 4);
				ImGui::Checkbox("Use Smoothing Variance", reinterpret_cast<bool*>(&ShaderArgs::RTAO::Denoiser::UseSmoothingVariance));
				ImGui::Checkbox("Disocclusion Blur", reinterpret_cast<bool*>(&ShaderArgs::RTAO::Denoiser::DisocclusionBlur));
				ImGui::Checkbox("Fullscreen Blur", reinterpret_cast<bool*>(&ShaderArgs::RTAO::Denoiser::FullscreenBlur));
				ImGui::SliderInt("Low Tspp Blur Passes", reinterpret_cast<int*>(&ShaderArgs::RTAO::Denoiser::LowTsppBlurPasses), 1, 8);

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("Main Pass")) {
			if (ImGui::TreeNode("Dithering Transparency")) {
				ImGui::SliderFloat("Max Distance", &ShaderArgs::GBuffer::Dither::MaxDistance, 0.1f, 10.0f);
				ImGui::SliderFloat("Min Distance", &ShaderArgs::GBuffer::Dither::MinDistance, 0.01f, 1.0f);

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
				ImGui::SliderFloat("CoC Max Deviation Tolerance", &ShaderArgs::DepthOfField::CoCMaxDevTolerance, 0.1f, 0.9f);
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
				ImGui::RadioButton("Screen Space", reinterpret_cast<INT*>(&mSSR->StateType), SSR::PipelineState::E_ScreenSpace); ImGui::SameLine();
				ImGui::RadioButton("View Space", reinterpret_cast<INT*>(&mSSR->StateType), SSR::PipelineState::E_ViewSpace);

				ImGui::SliderFloat("Max Distance", &ShaderArgs::SSR::MaxDistance, 1.0f, 100.0f);
				ImGui::SliderInt("Blur Count", &ShaderArgs::SSR::BlurCount, 0, 8);

				ImGui::Text("View");
				ImGui::SliderFloat("Ray Length", &ShaderArgs::SSR::View::RayLength, 1.0f, 64.0f);
				ImGui::SliderFloat("Noise Intensity", &ShaderArgs::SSR::View::NoiseIntensity, 0.1f, 0.001f);
				ImGui::SliderInt("Step Count", &ShaderArgs::SSR::View::StepCount, 1, 32);
				ImGui::SliderInt("Back Step Count", &ShaderArgs::SSR::View::BackStepCount, 1, 16);
				ImGui::SliderFloat("Depth Threshold", &ShaderArgs::SSR::View::DepthThreshold, 0.1f, 10.0f);

				ImGui::Text("Screen");
				ImGui::SliderFloat("Thickness", &ShaderArgs::SSR::Screen::Thickness, 0.01f, 1.0f);
				ImGui::SliderFloat("Resolution", &ShaderArgs::SSR::Screen::Resolution, 0.0f, 1.0f);

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
			if (ImGui::TreeNode("Raytraced Reflection")) {
				ImGui::Checkbox("Use Smoothing Variance", reinterpret_cast<bool*>(&ShaderArgs::RaytracedReflection::Denoiser::UseSmoothingVariance));
				ImGui::Checkbox("Fullscreen Blur", reinterpret_cast<bool*>(&ShaderArgs::RaytracedReflection::Denoiser::FullscreenBlur));
				ImGui::Checkbox("Disocclusion Blur", reinterpret_cast<bool*>(&ShaderArgs::RaytracedReflection::Denoiser::DisocclusionBlur));
				ImGui::SliderInt("Low Tspp Blur Passes", reinterpret_cast<int*>(&ShaderArgs::RaytracedReflection::Denoiser::LowTsppBlurPasses), 1, 8);

				ImGui::TreePop();
			}
		}

		ImGui::End();
	}
	;// Light Panel
	{
		ImGui::Begin("Light Panel");
		ImGui::NewLine();

		for (UINT i = 0; i < mLightCount; ++i) {
			auto& light = mLights[i];
			if (light.Type == LightType::E_Directional) {
				if (ImGui::TreeNode((std::to_string(i) + " Directional Light").c_str())) {
					ImGui::ColorPicker3("Light Color", reinterpret_cast<FLOAT*>(&light.LightColor));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0, 100.0f);
					if (ImGui::SliderFloat3("Direction", reinterpret_cast<FLOAT*>(&light.Direction), -1.0f, 1.0f)) {
						XMVECTOR dir = XMLoadFloat3(&light.Direction);
						XMVECTOR normalized = XMVector3Normalize(dir);
						XMStoreFloat3(&light.Direction, normalized);
					}

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Point) {
				if (ImGui::TreeNode((std::to_string(i) + " Point Light").c_str())) {
					ImGui::ColorPicker3("Light Color", reinterpret_cast<FLOAT*>(&light.LightColor));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0, 100.0f);
					ImGui::SliderFloat3("Position", reinterpret_cast<FLOAT*>(&light.Position), -100.0f, 100.0f);
					ImGui::SliderFloat("Falloff Start", &light.FalloffStart, 0, 100.0f);
					ImGui::SliderFloat("Falloff End", &light.FalloffEnd, 0, 100.0f);

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Spot) {
				if (ImGui::TreeNode((std::to_string(i) + " Spot Light").c_str())) {
					ImGui::ColorPicker3("Light Color", reinterpret_cast<FLOAT*>(&light.LightColor));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0, 100.0f);
					ImGui::SliderFloat("Spot Power", &light.SpotPower, 0, 10.0f);
					ImGui::SliderFloat3("Position", reinterpret_cast<FLOAT*>(&light.Position), -100.0f, 100.0f);
					if (ImGui::SliderFloat3("Direction", reinterpret_cast<FLOAT*>(&light.Direction), -1.0f, 1.0f)) {
						XMVECTOR dir = XMLoadFloat3(&light.Direction);
						XMVECTOR normalized = XMVector3Normalize(dir);
						XMStoreFloat3(&light.Direction, normalized);
					}
					ImGui::SliderFloat("Falloff Start", &light.FalloffStart, 0, 100.0f);
					ImGui::SliderFloat("Falloff End", &light.FalloffEnd, 0, 100.0f);

					ImGui::TreePop();
				}
			}
		}

		if (mLightCount < MaxLights) {
			if (ImGui::Button("Directional")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Directional;
				light.Intensity = 1.0f;
				light.Direction = { 0.0f, -1.0f, 0.0f };
				light.LightColor = { 1.0f, 1.0f, 1.0f };
				++mLightCount;
			}
			ImGui::SameLine();
			if (ImGui::Button("Point")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Point;
				light.Intensity = 1.0f;
				light.Position = { 0.0f, 0.0f, 0.0f };
				light.LightColor = { 1.0f, 1.0f, 1.0f };
				light.FalloffStart = 5.0f;
				light.FalloffEnd = 10.0f;
				++mLightCount;
			}
			ImGui::SameLine();
			if (ImGui::Button("Spot")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Spot;
				light.Intensity = 1.0f;
				light.SpotPower = 1.0f;
				light.Position = { 0.0f, 0.0f, 0.0f };
				light.Direction = { 0.0f, -1.0f, 0.0f };
				light.LightColor = { 1.0f, 1.0f, 1.0f };
				light.FalloffStart = 5.0f;
				light.FalloffEnd = 10.0f;
				++mLightCount;
			}
		}

		ImGui::End();
	}
	// Reference Panel
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
			ImGui::SliderFloat("Roughness", &mat->Roughness, 0.01f, 1.0f);
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

	return TRUE;
}

BOOL DxRenderer::DrawDXRShadow() {
	const auto cmdList = mCommandList.Get();	
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	mDxrShadow->Run(
		cmdList,
		mTLAS->Result->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mGBuffer->PositionMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mClientWidth, mClientHeight
	);
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawDXRBackBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mBRDF->CalcReflectanceWithoutSpecIrrad(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mToneMapping->InterMediateMapRtv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->RMSMapSrv(),
		mGBuffer->PositionMapSrv(),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		mRTAO->ResolvedAOCoefficientSrv(),
		mDxrShadow->Descriptor(),
		BRDF::Render::E_Raytrace
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::CalcDepthPartialDerivative() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mSVGF->RunCalculatingDepthPartialDerivative(
		cmdList,
		mGBuffer->DepthMapSrv(),
		mClientWidth, mClientHeight
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawRTAO() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Calculate ambient occlusion.
	{
		mRTAO->RunCalculatingAmbientOcclusion(
			cmdList,
			mTLAS->Result->GetGPUVirtualAddress(),
			mCurrFrameResource->CB_RTAO.Resource()->GetGPUVirtualAddress(),
			mGBuffer->PositionMapSrv(),
			mGBuffer->NormalDepthMapSrv(),
			mGBuffer->DepthMapSrv(),
			mClientWidth, mClientHeight
		);
	}
	// Denosing(Spatio-Temporal Variance Guided Filtering)
	{
		const auto& aoResources = mRTAO->AOResources();
		const auto& aoResourcesGpuDescriptors = mRTAO->AOResourceGpuDescriptors();
		const auto& temporalCaches = mRTAO->TemporalCaches();
		const auto& temporalCachesGpuDescriptors = mRTAO->TemporalCacheGpuDescriptors();
		const auto& temporalAOCoefficients = mRTAO->TemporalAOCoefficients();
		const auto& temporalAOCoefficientsGpuDescriptors = mRTAO->TemporalAOCoefficientGpuDescriptors();

		// Temporal supersampling 
		{
			// Stage 1: Reverse reprojection
			{
				UINT temporalPreviousFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
				UINT temporalCurrentFrameResourcIndex = mRTAO->MoveToNextFrame();

				UINT temporalPreviousFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
				UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->MoveToNextFrameTemporalAOCoefficient();

				const auto currTsppMap = temporalCaches[temporalCurrentFrameResourcIndex][RTAO::Resource::TemporalCache::E_Tspp].get();

				currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, currTsppMap);

				// Retrieves values from previous frame via reverse reprojection.				
				mSVGF->ReverseReprojectPreviousFrame(
					cmdList,
					mCurrFrameResource->CB_CrossBilateralFilter.Resource()->GetGPUVirtualAddress(),
					mGBuffer->NormalDepthMapSrv(),
					mGBuffer->ReprojNormalDepthMapSrv(),
					mGBuffer->PrevNormalDepthMapSrv(),
					mGBuffer->VelocityMapSrv(),
					temporalAOCoefficientsGpuDescriptors[temporalPreviousFrameTemporalAOCoefficientResourceIndex][RTAO::Descriptor::TemporalAOCoefficient::Srv],
					temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][RTAO::Descriptor::TemporalCache::ES_Tspp],
					temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][RTAO::Descriptor::TemporalCache::ES_CoefficientSquaredMean],
					temporalCachesGpuDescriptors[temporalPreviousFrameResourceIndex][RTAO::Descriptor::TemporalCache::ES_RayHitDistance],
					temporalCachesGpuDescriptors[temporalCurrentFrameResourcIndex][RTAO::Descriptor::TemporalCache::EU_Tspp],
					mClientWidth, mClientHeight,
					SVGF::Value::E_Contrast);
					
					currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarrier(cmdList, currTsppMap
				);
			}
			// Stage 2: Blending current frame value with the reprojected cached value.
			{
				// Calculate local mean and variance for clamping during the blending operation.
				mSVGF->RunCalculatingLocalMeanVariance(
					cmdList,
					mCurrFrameResource->CB_CalcLocalMeanVar.Resource()->GetGPUVirtualAddress(),
					aoResourcesGpuDescriptors[RTAO::Descriptor::AO::ES_AmbientCoefficient],
					mClientWidth, mClientHeight,
					ShaderArgs::SVGF::CheckerboardSamplingEnabled
				);
				// Interpolate the variance for the inactive cells from the valid checkerboard cells.
				if (ShaderArgs::SVGF::CheckerboardSamplingEnabled) {
					mSVGF->FillInCheckerboard(
						cmdList,
						mCurrFrameResource->CB_CalcLocalMeanVar.Resource()->GetGPUVirtualAddress(),
						mClientWidth, mClientHeight
					);
				}

				// Blends reprojected values with current frame values.
				// Inactive pixels are filtered from active neighbors on checkerboard sampling before the blending operation.
				{
					UINT temporalCurrentFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
					UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

					const auto currTemporalAOCoefficient = temporalAOCoefficients[temporalCurrentFrameTemporalAOCoefficientResourceIndex].get();
					const auto currTemporalSupersampling = temporalCaches[temporalCurrentFrameResourceIndex][RTAO::Resource::TemporalCache::E_Tspp].get();
					const auto currCoefficientSquaredMean = temporalCaches[temporalCurrentFrameResourceIndex][RTAO::Resource::TemporalCache::E_CoefficientSquaredMean].get();
					const auto currRayHitDistance = temporalCaches[temporalCurrentFrameResourceIndex][RTAO::Resource::TemporalCache::E_RayHitDistance].get();

					std::vector<GpuResource*> resources = {
						currTemporalAOCoefficient,
						currTemporalSupersampling,
						currCoefficientSquaredMean,
						currRayHitDistance,
					};

					currTemporalAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currCoefficientSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());

					mSVGF->BlendWithCurrentFrame(
						cmdList,
						mCurrFrameResource->CB_TSPPBlend.Resource()->GetGPUVirtualAddress(),
						aoResourcesGpuDescriptors[RTAO::Descriptor::AO::ES_AmbientCoefficient],
						aoResourcesGpuDescriptors[RTAO::Descriptor::AO::ES_RayHitDistance],
						temporalAOCoefficientsGpuDescriptors[temporalCurrentFrameTemporalAOCoefficientResourceIndex][RTAO::Descriptor::TemporalAOCoefficient::Uav],
						temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][RTAO::Descriptor::TemporalCache::EU_Tspp],
						temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][RTAO::Descriptor::TemporalCache::EU_CoefficientSquaredMean],
						temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][RTAO::Descriptor::TemporalCache::EU_RayHitDistance],
						mClientWidth, mClientHeight,
						SVGF::Value::E_Contrast
					);

					currTemporalAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currCoefficientSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());
				}

				if (ShaderArgs::RTAO::Denoiser::UseSmoothingVariance) {
					const auto& varianceResources = mSVGF->VarianceResources();
					const auto& varianceResourcesGpuDescriptors = mSVGF->VarianceResourcesGpuDescriptors();

					const auto smoothedVariance = varianceResources[SVGF::Resource::Variance::E_Smoothed].get();

					smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					D3D12Util::UavBarrier(cmdList, smoothedVariance);

					mGaussianFilter->Run(
						cmdList,
						varianceResourcesGpuDescriptors[SVGF::Descriptor::Variance::ES_Raw],
						varianceResourcesGpuDescriptors[SVGF::Descriptor::Variance::EU_Smoothed],
						GaussianFilter::Filter3x3,
						mClientWidth, mClientHeight
					);

					smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarrier(cmdList, smoothedVariance);
				}
			}
		}
		// Filtering
		{
			// Stage 1: Applies a single pass of a Atrous wavelet transform filter.
			if (ShaderArgs::RTAO::Denoiser::FullscreenBlur) {
				UINT temporalCurrentFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
				UINT inputAOCoefficientIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
				UINT outputAOCoefficientIndex = mRTAO->MoveToNextFrameTemporalAOCoefficient();

				const auto outputAOCoefficient = temporalAOCoefficients[outputAOCoefficientIndex].get();

				outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, outputAOCoefficient);

				const FLOAT rayHitDistToKernelWidthScale = 22 / ShaderArgs::RTAO::OcclusionRadius *
					ShaderArgs::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleFactor;
				const FLOAT rayHitDistToKernelSizeScaleExp = D3D12Util::Lerp(
					1,
					ShaderArgs::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleExponent,
					D3D12Util::RelativeCoef(ShaderArgs::RTAO::OcclusionRadius, 4, 22)
				);

				mSVGF->ApplyAtrousWaveletTransformFilter(
					cmdList,
					mCurrFrameResource->CB_AtrousFilter.Resource()->GetGPUVirtualAddress(),
					temporalAOCoefficientsGpuDescriptors[inputAOCoefficientIndex][RTAO::Descriptor::TemporalAOCoefficient::Srv],
					mGBuffer->NormalDepthMapSrv(),
					temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][RTAO::Descriptor::TemporalCache::ES_RayHitDistance],
					temporalCachesGpuDescriptors[temporalCurrentFrameResourceIndex][RTAO::Descriptor::TemporalCache::ES_Tspp],
					temporalAOCoefficientsGpuDescriptors[outputAOCoefficientIndex][RTAO::Descriptor::TemporalAOCoefficient::Uav],
					mClientWidth, mClientHeight,
					rayHitDistToKernelWidthScale,
					rayHitDistToKernelSizeScaleExp,
					SVGF::Value::E_Contrast,
					ShaderArgs::RTAO::Denoiser::UseSmoothingVariance
				);

				outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, outputAOCoefficient);
			}
			// Stage 2: 3x3 multi-pass disocclusion blur (with more relaxed depth-aware constraints for such pixels).
			if (ShaderArgs::RTAO::Denoiser::DisocclusionBlur) {
				UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

				const auto aoCoefficient = temporalAOCoefficients[temporalCurrentFrameTemporalAOCoefficientResourceIndex].get();

				aoCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, aoCoefficient);

				mSVGF->BlurDisocclusion(
					cmdList,
					aoCoefficient,
					mGBuffer->DepthMapSrv(),
					temporalAOCoefficientsGpuDescriptors[temporalCurrentFrameTemporalAOCoefficientResourceIndex][RTAO::Descriptor::TemporalAOCoefficient::Uav],
					mClientWidth, mClientHeight,
					ShaderArgs::RTAO::Denoiser::LowTsppBlurPasses,
					SVGF::Value::E_Contrast
				);

				aoCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, aoCoefficient);
			}
		}
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::BuildRaytracedReflection() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Calculate raytraced reflection.
	{
		mRR->CalcReflection(
			cmdList,
			mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
			mTLAS->Result->GetGPUVirtualAddress(),
			mToneMapping->InterMediateMapSrv(),
			mGBuffer->NormalMapSrv(),
			mGBuffer->DepthMapSrv(),
			mGBuffer->RMSMapSrv(),
			mGBuffer->PositionMapSrv(),
			mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
			mIrradianceMap->PrefilteredEnvironmentCubeMapSrv(),
			mIrradianceMap->IntegratedBrdfMapSrv(),
			mhGpuDescForTexMaps,
			mClientWidth, mClientHeight,
			ShaderArgs::RaytracedReflection::ReflectionRadius
		);
	}
	// Denosing(Spatio-Temporal Variance Guided Filtering)
	{
		const auto& reflections = mRR->Reflections();
		const auto& reflectionGpuDescriptors = mRR->ReflectionGpuDescriptors();
		const auto& temporalCaches = mRR->TemporalCaches();
		const auto& temporalCacheGpuDescriptors = mRR->TemporalCacheGpuDescriptors();
		const auto& temporalReflections = mRR->TemporalReflections();
		const auto& temporalReflectionGpuDescriptors = mRR->TemporalReflectionGpuDescriptors();

		// Temporal supersampling 
		{
			// Stage 1: Reverse reprojection
			{
				UINT temporalPreviousFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
				UINT temporalCurrentFrameResourcIndex = mRR->MoveToNextFrame();
			
				UINT temporalPreviousFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->MoveToNextFrameTemporalReflection();
			
				const auto currTsppMap = temporalCaches[temporalCurrentFrameResourcIndex][RaytracedReflection::Resource::TemporalCache::E_Tspp].get();
			
				currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, currTsppMap);
			
				// Retrieves values from previous frame via reverse reprojection.				
				mSVGF->ReverseReprojectPreviousFrame(
					cmdList,
					mCurrFrameResource->CB_CrossBilateralFilter.Resource()->GetGPUVirtualAddress(),
					mGBuffer->NormalDepthMapSrv(),
					mGBuffer->ReprojNormalDepthMapSrv(),
					mGBuffer->PrevNormalDepthMapSrv(),
					mGBuffer->VelocityMapSrv(),
					temporalReflectionGpuDescriptors[temporalPreviousFrameTemporalReflectionResourceIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Srv],
					temporalCacheGpuDescriptors[temporalPreviousFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::ES_Tspp],
					temporalCacheGpuDescriptors[temporalPreviousFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::ES_ReflectionSquaredMean],
					temporalCacheGpuDescriptors[temporalPreviousFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::ES_RayHitDistance],
					temporalCacheGpuDescriptors[temporalCurrentFrameResourcIndex][RaytracedReflection::Descriptor::TemporalCache::EU_Tspp],
					mClientWidth, mClientHeight,
					SVGF::Value::E_Color_HDR
				);
			
				currTsppMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, currTsppMap);
			}
			// Stage 2: Blending current frame value with the reprojected cached value.
			{
				// Calculate local mean and variance for clamping during the blending operation.
				mSVGF->RunCalculatingLocalMeanVariance(
					cmdList,
					mCurrFrameResource->CB_CalcLocalMeanVar.Resource()->GetGPUVirtualAddress(),
					reflectionGpuDescriptors[RaytracedReflection::Descriptor::Reflection::ES_Reflection],
					mClientWidth, mClientHeight,
					ShaderArgs::RaytracedReflection::CheckerboardSamplingEnabled
				);

				// Blends reprojected values with current frame values.
				// Inactive pixels are filtered from active neighbors on checkerboard sampling before the blending operation.
				{
					UINT temporalCurrentFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
					UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				
					const auto currTemporalReflection = temporalReflections[temporalCurrentFrameTemporalReflectionResourceIndex].get();
					const auto currTemporalSupersampling = temporalCaches[temporalCurrentFrameResourceIndex][RaytracedReflection::Resource::TemporalCache::E_Tspp].get();
					const auto currReflectionSquaredMean = temporalCaches[temporalCurrentFrameResourceIndex][RaytracedReflection::Resource::TemporalCache::E_ReflectionSquaredMean].get();
					const auto currRayHitDistance = temporalCaches[temporalCurrentFrameResourceIndex][RaytracedReflection::Resource::TemporalCache::E_RayHitDistance].get();
				
					std::vector<GpuResource*> resources = {
						currTemporalReflection,
						currTemporalSupersampling,
						currReflectionSquaredMean,
						currRayHitDistance,
					};
				
					currTemporalReflection->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currReflectionSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());
				
					mSVGF->BlendWithCurrentFrame(
						cmdList,
						mCurrFrameResource->CB_TSPPBlend.Resource()->GetGPUVirtualAddress(),
						reflectionGpuDescriptors[RaytracedReflection::Descriptor::Reflection::ES_Reflection],
						reflectionGpuDescriptors[RaytracedReflection::Descriptor::Reflection::ES_RayHitDistance],
						temporalReflectionGpuDescriptors[temporalCurrentFrameTemporalReflectionResourceIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Uav],
						temporalCacheGpuDescriptors[temporalCurrentFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::EU_Tspp],
						temporalCacheGpuDescriptors[temporalCurrentFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::EU_ReflectionSquaredMean],
						temporalCacheGpuDescriptors[temporalCurrentFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::EU_RayHitDistance],
						mClientWidth, mClientHeight,
						SVGF::Value::E_Color_HDR
					);
				
					currTemporalReflection->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currTemporalSupersampling->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currReflectionSquaredMean->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					currRayHitDistance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());
				}
				if (ShaderArgs::RaytracedReflection::Denoiser::UseSmoothingVariance) {
					const auto& varianceResources = mSVGF->VarianceResources();
					const auto& varianceResourcesGpuDescriptors = mSVGF->VarianceResourcesGpuDescriptors();

					const auto smoothedVariance = varianceResources[SVGF::Resource::Variance::E_Smoothed].get();

					smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					D3D12Util::UavBarrier(cmdList, smoothedVariance);

					mGaussianFilter->Run(
						cmdList,
						varianceResourcesGpuDescriptors[SVGF::Descriptor::Variance::ES_Raw],
						varianceResourcesGpuDescriptors[SVGF::Descriptor::Variance::EU_Smoothed],
						GaussianFilter::Filter3x3,
						mClientWidth, mClientHeight
					);

					smoothedVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					D3D12Util::UavBarrier(cmdList, smoothedVariance);
				}
			}
		}
		// Filtering
		{
			// Stage 1: Applies a single pass of a Atrous wavelet transform filter.
			if (ShaderArgs::RaytracedReflection::Denoiser::FullscreenBlur) {
				UINT temporalCurrentFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
				UINT inputReflectionIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				UINT outputReflectionIndex = mRR->MoveToNextFrameTemporalReflection();

				const auto outputReflection = temporalReflections[outputReflectionIndex].get();

				outputReflection->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, outputReflection);

				const FLOAT rayHitDistToKernelWidthScale = 22 / ShaderArgs::RaytracedReflection::ReflectionRadius *
					ShaderArgs::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleFactor;
				const FLOAT rayHitDistToKernelSizeScaleExp = D3D12Util::Lerp(
					1,
					ShaderArgs::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleExponent,
					D3D12Util::RelativeCoef(ShaderArgs::RaytracedReflection::ReflectionRadius, 4, 22)
				);

				mSVGF->ApplyAtrousWaveletTransformFilter(
					cmdList,
					mCurrFrameResource->CB_AtrousFilter.Resource()->GetGPUVirtualAddress(),
					temporalReflectionGpuDescriptors[inputReflectionIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Srv],
					mGBuffer->NormalDepthMapSrv(),
					temporalCacheGpuDescriptors[temporalCurrentFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::ES_RayHitDistance],
					temporalCacheGpuDescriptors[temporalCurrentFrameResourceIndex][RaytracedReflection::Descriptor::TemporalCache::ES_Tspp],
					temporalReflectionGpuDescriptors[outputReflectionIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Uav],
					mClientWidth, mClientHeight,
					rayHitDistToKernelWidthScale,
					rayHitDistToKernelSizeScaleExp,
					SVGF::Value::E_Color_HDR,
					ShaderArgs::RaytracedReflection::Denoiser::UseSmoothingVariance
				);

				outputReflection->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, outputReflection);
			}
			// Stage 2: 3x3 multi-pass disocclusion blur (with more relaxed depth-aware constraints for such pixels).
			if (ShaderArgs::RaytracedReflection::Denoiser::DisocclusionBlur) {
				UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();

				const auto reflections = temporalReflections[temporalCurrentFrameTemporalReflectionResourceIndex].get();

				reflections->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, reflections);

				mSVGF->BlurDisocclusion(
					cmdList,
					reflections,
					mGBuffer->DepthMapSrv(),
					temporalReflectionGpuDescriptors[temporalCurrentFrameTemporalReflectionResourceIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Uav],
					mClientWidth, mClientHeight,
					ShaderArgs::RaytracedReflection::Denoiser::LowTsppBlurPasses,
					SVGF::Value::E_Color_HDR
				);

				reflections->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, reflections);
			}
		}
	}

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}
