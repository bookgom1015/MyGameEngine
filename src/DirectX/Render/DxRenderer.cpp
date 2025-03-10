#include "DirectX/Render/DxRenderer.h"
#include "Common/Debug/Logger.h"
#include "Common/Helper/MathHelper.h"
#include "Common/Mesh/MeshImporter.h"
#include "Common/Util/HWInfo.h"
#include "Common/Util/TaskQueue.h"
#include "Common/Shading/ShaderArgument.h"
#include "DirectX/Debug/Debug.h"
#include "DirectX/Debug/ImGuiManager.h"
#include "DirectX/Infrastructure/FrameResource.h"
#include "DirectX/Infrastructure/AccelerationStructure.h"
#include "DirectX/Infrastructure/DXR_GeometryBuffer.h"
#include "DirectX/Util/ShaderManager.h"
#include "DirectX/Util/MipmapGenerator.h"
#include "DirectX/Util/GeometryGenerator.h"
#include "DirectX/Util/EquirectangularConverter.h"
#include "DirectX/Shading/Samplers.h"
#include "DirectX/Shading/BRDF.h"
#include "DirectX/Shading/GammaCorrection.h"
#include "DirectX/Shading/IrradianceMap.h"
#include "DirectX/Shading/BlurFilter.h"
#include "DirectX/Shading/GaussianFilter.h"
#include "DirectX/Shading/Bloom.h"
#include "DirectX/Shading/DepthOfField.h"
#include "DirectX/Shading/GBuffer.h"
#include "DirectX/Shading/ZDepth.h"
#include "DirectX/Shading/Shadow.h"
#include "DirectX/Shading/SSAO.h"
#include "DirectX/Shading/SSR.h"
#include "DirectX/Shading/TemporalAA.h"
#include "DirectX/Shading/ToneMapping.h"
#include "DirectX/Shading/MotionBlur.h"
#include "DirectX/Shading/Pixelation.h"
#include "DirectX/Shading/Sharpen.h"
#include "DirectX/Shading/VolumetricLight.h"
#include "DirectX/Shading/DXR_Shadow.h"
#include "DirectX/Shading/RaytracedReflection.h"
#include "DirectX/Shading/RTAO.h"
#include "DirectX/Shading/SVGF.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

#undef max
#undef min

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";

	HWInfo::Processor ProcessorInfo;
}

DxRenderer::DxRenderer() {
	mLocker = std::make_unique<Locker<ID3D12Device5>>();

	mMainPassCB = std::make_unique<ConstantBuffer_Pass>();
	mShaderManager = std::make_unique<ShaderManager>();
	mImGui = std::make_unique<ImGuiManager>();
	mBRDF = std::make_unique<BRDF::BRDFClass>();
	mGBuffer = std::make_unique<GBuffer::GBufferClass>();
	mShadow = std::make_unique<Shadow::ShadowClass>();
	mZDepth = std::make_unique<ZDepth::ZDepthClass>();
	mSSAO = std::make_unique<SSAO::SSAOClass>();
	mBlurFilter = std::make_unique<BlurFilter::BlurFilterClass>();
	mBlurFilterCS = std::make_unique<BlurFilter::CS::BlurFilterCSClass>();
	mBloom = std::make_unique<Bloom::BloomClass>();
	mSSR = std::make_unique<SSR::SSRClass>();
	mDoF = std::make_unique<DepthOfField::DepthOfFieldClass>();
	mMotionBlur = std::make_unique<MotionBlur::MotionBlurClass>();
	mTAA = std::make_unique<TemporalAA::TemporalAAClass>();
	mDebug = std::make_unique<Debug::DebugClass>();
	mGammaCorrection = std::make_unique<GammaCorrection::GammaCorrectionClass>();
	mToneMapping = std::make_unique<ToneMapping::ToneMappingClass>();
	mIrradianceMap = std::make_unique<IrradianceMap::IrradianceMapClass>();
	mMipmapGenerator = std::make_unique<MipmapGenerator::MipmapGeneratorClass>();
	mPixelation = std::make_unique<Pixelation::PixelationClass>();
	mSharpen = std::make_unique<Sharpen::SharpenClass>();
	mGaussianFilter = std::make_unique<GaussianFilter::GaussianFilterClass>();
	mTLAS = std::make_unique<AccelerationStructureBuffer>();
	mDxrShadow = std::make_unique<DXR_Shadow::DXR_ShadowClass>();
	mRTAO = std::make_unique<RTAO::RTAOClass>();
	mRR = std::make_unique<RaytracedReflection::RaytracedReflectionClass>();
	mSVGF = std::make_unique<SVGF::SVGFClass>();
	mEquirectangularConverter = std::make_unique<EquirectangularConverter::EquirectangularConverterClass>();
	mVolumetricLight = std::make_unique<VolumetricLight::VolumetricLightClass>();

	mSceneBounds.Center = XMFLOAT3(0.f, 0.f, 0.f);
	const FLOAT widthSquared = 32.f * 32.f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);

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

BOOL DxRenderer::Initialize(HWND hwnd, void* const glfwWnd, UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;

	CheckReturn(LowInitialize(hwnd, width, height));
	CheckReturn(HWInfo::GetProcessorInfo(ProcessorInfo));

	auto device = md3dDevice.Get();
	mGraphicsMemory = std::make_unique<GraphicsMemory>(device);
	mLocker->PutIn(device);

	const auto shaderManager = mShaderManager.get();
	const auto locker = mLocker.get();

	ProcessorInfo.Logical = 1;

#ifdef _DEBUG
	WLogln(L"Initializing shading components...");
#endif

	CheckReturn(mShaderManager->Initialize());
	CheckReturn(mImGui->Initialize(mhMainWnd, device, mCbvSrvUavHeap.Get(), SwapChainBufferCount, SwapChainBuffer::BackBufferFormat));

	{
		TaskQueue taskQueue;
		taskQueue.AddTask([&] { return mBRDF->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mGBuffer->Initialize(locker, width, height, shaderManager, mDepthStencilBuffer->Resource(), mDepthStencilBuffer->Dsv()); });
		taskQueue.AddTask([&] { return mShadow->Initialize(locker, shaderManager, width, height, 2048, 2048); });
		taskQueue.AddTask([&] { return mZDepth->Initialize(locker, 2048, 2048); });
		taskQueue.AddTask([&] { return mSSAO->Initialize(locker, shaderManager, width, height, SSAO::Resolution::E_Quarter); });
		taskQueue.AddTask([&] { return mBlurFilter->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mBlurFilterCS->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mBloom->Initialize(locker, shaderManager, width, height, Bloom::Resolution::E_OneSixteenth); });
		taskQueue.AddTask([&] { return mSSR->Initialize(locker, shaderManager, width, height, SSR::Resolution::E_Quarter); });
		taskQueue.AddTask([&] { return mDoF->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mMotionBlur->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mTAA->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mDebug->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mGammaCorrection->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mToneMapping->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mIrradianceMap->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mMipmapGenerator->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mPixelation->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mSharpen->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mGaussianFilter->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mDxrShadow->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mRTAO->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mRR->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mSVGF->Initialize(locker, shaderManager, width, height); });
		taskQueue.AddTask([&] { return mEquirectangularConverter->Initialize(locker, shaderManager); });
		taskQueue.AddTask([&] { return mVolumetricLight->Initialize(locker, shaderManager, width, height, 160, 90, 128); });
		CheckReturn(taskQueue.Run(ProcessorInfo.Logical));
	}

#ifdef _DEBUG
	WLogln(L"Finished initializing shading components \n");
#endif

	CheckReturn(CompileShaders());
	CheckReturn(BuildGeometries());

	CheckReturn(BuildFrameResources());

	CheckReturn(BuildDescriptors());

	CheckReturn(BuildRootSignatures());
	CheckReturn(BuildPSOs());

	BuildRenderItems();

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.f, ((offset.y - 0.5f) / height) * 2.f);
	}
	{
		const auto blurSize = BlurFilter::CalcSize(2.5f);
		std::vector<FLOAT> blurWeights(blurSize, 0.f);

		CheckReturn(BlurFilter::CalcGaussWeights(2.5f, blurWeights.data()));
		mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
		mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
		mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);
	}

	bInitialized = TRUE;
#ifdef _DEBUG
	WLogln(L"Succeeded to initialize d3d12 renderer \n");
#endif

	// Directional light 1
	{
		Light light;
		light.Type = LightType::E_Directional;
		light.Direction = { 0.577f, -0.577f, 0.577f };
		light.Color = { 240.f / 255.f, 235.f / 255.f, 223.f / 255.f };
		light.Intensity = 1.802f;

		mLights[mLightCount] = light;
		mZDepth->AddLight(light.Type, mLightCount++);
	}
	// Directional light 2
	{
		Light light;
		light.Type = LightType::E_Directional;
		light.Direction = { 0.067f, -0.701f, -0.836f };
		light.Color = { 149.f / 255.f, 142.f/ 255.f, 100.f / 255.f };
		light.Intensity = 1.534f;

		mLights[mLightCount] = light;
		mZDepth->AddLight(light.Type, mLightCount++);
	}

	return TRUE;
}

void DxRenderer::CleanUp() {
	if (!bInitialized) return;

	FlushCommandQueue();
	
	mImGui->CleanUp();
	mShaderManager->CleanUp();
	LowCleanUp();

	bIsCleanedUp = TRUE;
}

BOOL DxRenderer::PrepareUpdate() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckReturn(mSSAO->PrepareUpdate(cmdList));
	CheckReturn(mDoF->PrepareUpdate(cmdList));
	CheckReturn(mVolumetricLight->PrepareUpdate(cmdList));

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	return TRUE;
}

BOOL DxRenderer::Update(FLOAT delta) {
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		const HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		if (eventHandle == NULL) return FALSE;

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
	CheckReturn(UpdateCB_Debug(delta));

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
	CheckReturn(PrePass());
	// Main-pass
	CheckReturn(MainPass());
	// Post-pass
	CheckReturn(PostPass());
	
	CheckReturn(DrawDebuggingInfo());
	if (bShowImGui) CheckReturn(DrawImGui());
	
	CheckHRESULT(mSwapChain->Present(0, AllowTearing() ? DXGI_PRESENT_ALLOW_TEARING : 0));
	mSwapChainBuffer->NextBackBuffer();
	
	mCurrFrameResource->Fence = static_cast<UINT>(IncreaseFence());
	mCommandQueue->Signal(mFence.Get(), GetCurrentFence());

	return TRUE;
}

BOOL DxRenderer::OnResize(UINT width, UINT height) {
	const BOOL bNeedToReszie = mClientWidth != width || mClientHeight != height;

	mClientWidth = width;
	mClientHeight = height;
	
	CheckReturn(LowOnResize(width, height));

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (UINT i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}


	CheckReturn(mSwapChainBuffer->OnResize());
	if (bNeedToReszie) {
		CheckReturn(mBRDF->OnResize(width, height));
		CheckReturn(mGBuffer->OnResize(width, height));
		CheckReturn(mDoF->OnResize(width, height));
		CheckReturn(mMotionBlur->OnResize(width, height));
		CheckReturn(mTAA->OnResize(width, height));
		CheckReturn(mGammaCorrection->OnResize(width, height));
		CheckReturn(mToneMapping->OnResize(width, height));
		CheckReturn(mPixelation->OnResize(width, height));
		CheckReturn(mSharpen->OnResize(width, height));
		CheckReturn(mSSR->OnResize(width, height));
		CheckReturn(mDxrShadow->OnResize(width, height));
		CheckReturn(mRTAO->OnResize(width, height));
		CheckReturn(mRR->OnResize(width, height));
		CheckReturn(mSVGF->OnResize(width , height))
	}
	CheckReturn(mSSAO->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.f, ((offset.y - 0.5f) / height) * 2.f);
	}
	
	return TRUE;
}

void* DxRenderer::AddModel(const std::string& file, const Transform& trans, RenderType::Type type) {
	if (mGeometries.count(file) == 0) CheckReturn(AddGeometry(file));
	return AddRenderItem(file, trans, type);
}

void DxRenderer::RemoveModel(void* const model) {}

void DxRenderer::UpdateModel(void* const model, const Transform& trans) {
	RenderItem* const ritem = reinterpret_cast<RenderItem*>(model);
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
			XMVectorSet(0.f, 0.f, 0.f, 1.f), 
			trans.Rotation, 
			trans.Position
		)
	);
	ptr->NumFramesDirty = gNumFrameResources << 1;
}

void DxRenderer::SetModelVisibility(void* const model, BOOL visible) {

}

void DxRenderer::SetModelPickable(void* const model, BOOL pickable) {
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
	const auto& P = mCamera->Proj();

	// Compute picking ray in vew space.
	const FLOAT vx = (2.f * x / mClientWidth - 1.f) / P(0, 0);
	const FLOAT vy = (-2.f * y / mClientHeight + 1.f) / P(1, 1);

	// Ray definition in view space.
	auto origin = XMVectorSet(0.f, 0.f, 0.f, 1.f);
	auto dir = XMVectorSet(vx, vy, 1.f, 0.f);

	const auto V = XMLoadFloat4x4(&mCamera->View());
	const auto InvView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	FLOAT closestT = std::numeric_limits<FLOAT>().max();
	for (auto ri : mRitemRefs[RenderType::E_Opaque]) {
		if (!ri->Pickable) continue;

		const auto W = XMLoadFloat4x4(&ri->World);
		const auto InvWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

		// Transform ray to vi space of the mesh.
		const auto ToLocal = XMMatrixMultiply(InvView, InvWorld);

		const auto originL = XMVector3TransformCoord(origin, ToLocal);
		auto dirL = XMVector3TransformNormal(dir, ToLocal);

		// Make the ray direction unit length for the intersection tests.
		dirL = XMVector3Normalize(dirL);
		FLOAT tmin = 0.f;
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
		+ Shadow::NumRenderTargets
		+ SSAO::NumRenderTargets
		+ DepthOfField::NumRenderTargets
		+ Bloom::NumRenderTargets
		+ SSR::NumRenderTargets
		+ ToneMapping::NumRenderTargets
		+ IrradianceMap::NumRenderTargets;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1
		+ Shadow::NumDepthStenciles;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

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

	TaskQueue taskQueue;
	taskQueue.AddTask([&] { return mBRDF->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mGBuffer->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mShadow->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mSSAO->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mBlurFilter->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mBloom->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mSSR->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mDoF->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mMotionBlur->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mTAA->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mDebug->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mGammaCorrection->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mToneMapping->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mIrradianceMap->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mMipmapGenerator->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mPixelation->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mSharpen->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mGaussianFilter->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mDxrShadow->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mBlurFilterCS->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mRTAO->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mRR->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mSVGF->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mEquirectangularConverter->CompileShaders(ShaderFilePath); });
	taskQueue.AddTask([&] { return mVolumetricLight->CompileShaders(ShaderFilePath); });
	CheckReturn(taskQueue.Run(ProcessorInfo.Logical));

#ifdef _DEBUG
	WLogln(L"Finished compiling shaders \n");
#endif 

	return TRUE;
}

BOOL DxRenderer::BuildGeometries() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.f, 32, 32);
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.f, 1.f, 1.f, 1);

	UINT numVertices = 0;
	UINT numIndices = 0;

	UINT startIndexLocation = 0;
	UINT baseVertexLocation = 0;

	// 1.
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = startIndexLocation;
	sphereSubmesh.BaseVertexLocation = baseVertexLocation;
	{
		const auto numIdx = static_cast<UINT>(sphere.GetIndices16().size());
		const auto numVert = static_cast<UINT>(sphere.Vertices.size());

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
		const auto numIdx = static_cast<UINT>(box.GetIndices16().size());
		const auto numVert = static_cast<UINT>(box.Vertices.size());

		boxSubmesh.IndexCount = numIdx;

		numIndices += numIdx;
		numVertices += numVert;

		startIndexLocation += numIdx;
		baseVertexLocation += numVert;
	}

	std::vector<Vertex> vertices(numVertices);

	for (size_t i = 0, end = sphere.Vertices.size(); i < end; ++i) {
		const auto index = i + sphereSubmesh.BaseVertexLocation;
		vertices[index].Position = sphere.Vertices[i].Position;
		vertices[index].Normal = sphere.Vertices[i].Normal;
		vertices[index].TexCoord = sphere.Vertices[i].TexC;
	}
	for (size_t i = 0, end = box.Vertices.size(); i < end; ++i) {
		const auto index = i + boxSubmesh.BaseVertexLocation;
		vertices[index].Position = box.Vertices[i].Position;
		vertices[index].Normal = box.Vertices[i].Normal;
		vertices[index].TexCoord = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices(numIndices);
	for (size_t i = 0, end = sphere.GetIndices16().size(); i < end; ++i) {
		const auto index = i + sphereSubmesh.StartIndexLocation;
		indices[index] = sphere.GetIndices16()[i];
	}
	for (size_t i = 0, end = box.GetIndices16().size(); i < end; ++i) {
		const auto index = i + boxSubmesh.StartIndexLocation;
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
			md3dDevice.Get(), 
			1, 
			MaxLights,	// Shadows
			32,			// Objects
			32));		// Materials
		CheckReturn(mFrameResources.back()->Initialize());
	}

	return TRUE;
}

BOOL DxRenderer::BuildDescriptors() {
	auto& hCpu = mhCpuCbvSrvUav;
	auto& hGpu = mhGpuCbvSrvUav;
	auto& hCpuDsv = mhCpuDsv;
	auto& hCpuRtv = mhCpuRtv;

	const auto descSize = GetCbvSrvUavDescriptorSize();
	const auto rtvDescSize = GetRtvDescriptorSize();
	const auto dsvDescSize = GetDsvDescriptorSize();

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (INT i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}

	hCpu.Offset(-1, descSize);
	hGpu.Offset(-1, descSize);
	hCpuDsv.Offset(-1, dsvDescSize);
	hCpuRtv.Offset(-1, rtvDescSize);

	mImGui->AllocateDescriptors(hCpu, hGpu, descSize);
	mSwapChainBuffer->AllocateDescriptors(hCpu, hGpu, descSize);
	mBRDF->AllocateDescriptors(hCpu, hGpu, descSize);
	mGBuffer->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mShadow->AllocateDescriptors(hCpu, hGpu, hCpuDsv, hCpuRtv, descSize, dsvDescSize, rtvDescSize);
	mZDepth->AllocateDescriptors(hCpu, hGpu, hCpuDsv, descSize, dsvDescSize);
	mSSAO->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mBloom->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSSR->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mDoF->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mMotionBlur->AllocateDescriptors(hCpu, hGpu, descSize);
	mTAA->AllocateDescriptors(hCpu, hGpu, descSize);
	mGammaCorrection->AllocateDescriptors(hCpu, hGpu, descSize);
	mToneMapping->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mIrradianceMap->AllocateDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mPixelation->AllocateDescriptors(hCpu, hGpu, descSize);
	mSharpen->AllocateDescriptors(hCpu, hGpu, descSize);
	mDxrShadow->AllocateDescriptors(hCpu, hGpu, descSize);
	mRTAO->AllocateDescriptors(hCpu, hGpu, descSize);
	mRR->AllocateDesscriptors(hCpu, hGpu, descSize);
	mSVGF->AllocateDescriptors(hCpu, hGpu, descSize);
	mVolumetricLight->AllocateDescriptors(hCpu, hGpu, descSize);

	TaskQueue taskQueue;
	taskQueue.AddTask([&] { return mImGui->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mSwapChainBuffer->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mBRDF->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mGBuffer->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mShadow->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mSSAO->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mBloom->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mSSR->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mDoF->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mMotionBlur->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mTAA->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mGammaCorrection->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mToneMapping->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mIrradianceMap->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mPixelation->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mSharpen->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mDxrShadow->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mRTAO->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mRR->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mSVGF->BuildDescriptors(); });
	taskQueue.AddTask([&] { return mVolumetricLight->BuildDescriptors(); });
	CheckReturn(taskQueue.Run(ProcessorInfo.Logical));

	mhCpuDescForTexMaps = hCpu.Offset(1, descSize);
	mhGpuDescForTexMaps = hGpu.Offset(1, descSize);

	return TRUE;
}

BOOL DxRenderer::BuildRootSignatures() {
#if _DEBUG
	WLogln(L"Building root-signatures...");
#endif

	auto staticSamplers = Samplers::GetStaticSamplers();

	TaskQueue taskQueue;
	taskQueue.AddTask([&] { return mBRDF->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mGBuffer->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mShadow->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mSSAO->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mBlurFilter->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mBloom->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mSSR->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mDoF->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mMotionBlur->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mTAA->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mDebug->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mGammaCorrection->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mToneMapping->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mIrradianceMap->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mMipmapGenerator->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mPixelation->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mSharpen->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mGaussianFilter->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mDxrShadow->BuildRootSignatures(staticSamplers, DXR_GeometryBuffer::GeometryBufferCount); });
	taskQueue.AddTask([&] { return mBlurFilterCS->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mRTAO->BuildRootSignatures(staticSamplers); });
	taskQueue.AddTask([&] { return mRR->BuildRootSignatures(staticSamplers); });
	taskQueue.AddTask([&] { return mSVGF->BuildRootSignatures(staticSamplers); });
	taskQueue.AddTask([&] { return mEquirectangularConverter->BuildRootSignature(staticSamplers); });
	taskQueue.AddTask([&] { return mVolumetricLight->BuildRootSignature(staticSamplers); });
	CheckReturn(taskQueue.Run(ProcessorInfo.Logical));

#if _DEBUG
	WLogln(L"Finished building root-signatures \n");
#endif

	return TRUE;
}

BOOL DxRenderer::BuildPSOs() {
#ifdef _DEBUG
	WLogln(L"Building pipeline state objects...");
#endif

	TaskQueue taskQueue;
	taskQueue.AddTask([&] { return mBRDF->BuildPSO(); });
	taskQueue.AddTask([&] { return mGBuffer->BuildPSO(); });
	taskQueue.AddTask([&] { return mShadow->BuildPSO(); });
	taskQueue.AddTask([&] { return mSSAO->BuildPSO(); });
	taskQueue.AddTask([&] { return mBloom->BuildPSO(); });
	taskQueue.AddTask([&] { return mBlurFilter->BuildPSO(); });
	taskQueue.AddTask([&] { return mSSR->BuildPSO(); });
	taskQueue.AddTask([&] { return mDoF->BuildPSO(); });
	taskQueue.AddTask([&] { return mMotionBlur->BuildPSO(); });
	taskQueue.AddTask([&] { return mTAA->BuildPSO(); });
	taskQueue.AddTask([&] { return mDebug->BuildPSO(); });
	taskQueue.AddTask([&] { return mGammaCorrection->BuildPSO(); });
	taskQueue.AddTask([&] { return mToneMapping->BuildPSO(); });
	taskQueue.AddTask([&] { return mIrradianceMap->BuildPSO(); });
	taskQueue.AddTask([&] { return mMipmapGenerator->BuildPSO(); });
	taskQueue.AddTask([&] { return mPixelation->BuildPSO(); });
	taskQueue.AddTask([&] { return mSharpen->BuildPSO(); });
	taskQueue.AddTask([&] { return mGaussianFilter->BuildPSO(); });
	taskQueue.AddTask([&] { return mDxrShadow->BuildPSO(); });
	taskQueue.AddTask([&] { return mBlurFilterCS->BuildPSO(); });
	taskQueue.AddTask([&] { return mRTAO->BuildPSO(); });
	taskQueue.AddTask([&] { return mRR->BuildPSO(); });
	taskQueue.AddTask([&] { return mSVGF->BuildPSO(); });
	taskQueue.AddTask([&] { return mEquirectangularConverter->BuildPSO(); });
	taskQueue.AddTask([&] { return mVolumetricLight->BuildPSO(); });
	CheckReturn(taskQueue.Run(ProcessorInfo.Logical));

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
		XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(1000.f, 1000.f, 1000.f));
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

	const XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	const XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

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
	const UINT index = AddTexture(file, material);
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
			XMVectorSet(0.f, 0.f, 0.f, 1.f),
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

	const auto index = filename.rfind(L'.');
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
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
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
		md3dDevice.Get(),
		mCommandQueue.Get(),
		mCbvSrvUavHeap.Get(),
		cmdList,
		mEquirectangularConverter.get(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->CB_Irradiance.Resource()->GetGPUVirtualAddress(),
		mMipmapGenerator.get()
	));
	
	if (bNeedToRebuildTLAS) {
		bNeedToRebuildTLAS = FALSE;
		CheckReturn(BuildTLAS(cmdList));
	}
	else {
		CheckReturn(UpdateTLAS(cmdList));
	}
	
	if (bNeedToRebuildShaderTables) {
		bNeedToRebuildShaderTables = FALSE;
		BuildShaderTables();
	}
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));
	CheckReturn(FlushCommandQueue());

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Main(FLOAT delta) {
	// Transform NDC space [-1 , +1]^2 to texture space [0, 1]^2
	const XMMATRIX T(
		0.5f, 0.f,  0.f, 0.f,
		0.f, -0.5f, 0.f, 0.f,
		0.f,  0.f,  1.f, 0.f,
		0.5f, 0.5f, 0.f, 1.f
	);

	// Shadow
	{
		mMainPassCB->LightCount = mLightCount;

		for (UINT i = 0; i < mLightCount; ++i) {
			auto& light = mLights[i];

			if (light.Type == LightType::E_Directional) {
				const XMVECTOR lightDir = XMLoadFloat3(&light.Direction);
				const XMVECTOR lightPos = -2.f * mSceneBounds.Radius * lightDir;
				const XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
				const XMVECTOR lightUp = UnitVector::UpVector;
				const XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

				// Transform bounding sphere to light space.
				XMFLOAT3 sphereCenterLS;
				XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

				// Ortho frustum in light space encloses scene.
				const FLOAT l = sphereCenterLS.x - mSceneBounds.Radius;
				const FLOAT b = sphereCenterLS.y - mSceneBounds.Radius;
				const FLOAT n = sphereCenterLS.z - mSceneBounds.Radius;
				const FLOAT r = sphereCenterLS.x + mSceneBounds.Radius;
				const FLOAT t = sphereCenterLS.y + mSceneBounds.Radius;
				const FLOAT f = sphereCenterLS.z + mSceneBounds.Radius;

				const XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

				const XMMATRIX viewProj = XMMatrixMultiply(lightView, lightProj);
				XMStoreFloat4x4(&light.Mat0, XMMatrixTranspose(viewProj));

				const XMMATRIX S = lightView * lightProj * T;
				XMStoreFloat4x4(&light.Mat1, XMMatrixTranspose(S));

				XMStoreFloat3(&light.Position, lightPos);
			}
			else if (light.Type == LightType::E_Point || light.Type == LightType::E_Spot || light.Type == LightType::E_Tube) {
				const auto proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.f, 1.f, 50.f);
				const auto pos = XMLoadFloat3(&light.Position);
				
				// Positive +X
				{
					const auto target = pos + XMVectorSet(1.f, 0.f, 0.f, 0.f);
					const auto view_px = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 1.f, 0.f, 0.f));
					const auto vp_px = view_px * proj;
					XMStoreFloat4x4(&light.Mat0, XMMatrixTranspose(vp_px));
				}
				// Positive -X
				{
					const auto target = pos + XMVectorSet(-1.f, 0.f, 0.f, 0.f);
					const auto view_nx = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 1.f, 0.f, 0.f));
					const auto vp_nx = view_nx * proj;
					XMStoreFloat4x4(&light.Mat1, XMMatrixTranspose(vp_nx));
				}
				// Positive +Y
				{
					const auto target = pos + XMVectorSet(0.f, 1.f, 0.f, 0.f);
					const auto view_py = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 0.f, -1.f, 0.f));
					const auto vp_py = view_py * proj;
					XMStoreFloat4x4(&light.Mat2, XMMatrixTranspose(vp_py));
				}
				// Positive -Y
				{
					const auto target = pos + XMVectorSet(0.f, -1.f, 0.f, 0.f);
					const auto view_ny = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 0.f, 1.f, 0.f));
					const auto vp_ny = view_ny * proj;
					XMStoreFloat4x4(&light.Mat3, XMMatrixTranspose(vp_ny));
				}
				// Positive +Z
				{
					const auto target = pos + XMVectorSet(0.f, 0.f, 1.f, 0.f);
					const auto view_pz = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 1.f, 0.f, 0.f));
					const auto vp_pz = view_pz * proj;
					XMStoreFloat4x4(&light.Mat4, XMMatrixTranspose(vp_pz));
				}
				// Positive -Z
				{
					const auto target = pos + XMVectorSet(0.f, 0.f, -1.f, 0.f);
					const auto view_nz = XMMatrixLookAtLH(pos, target, XMVectorSet(0.f, 1.f, 0.f, 0.f));
					const auto vp_nz = view_nz * proj;
					XMStoreFloat4x4(&light.Mat5, XMMatrixTranspose(vp_nz));
				}
			}
			else if (light.Type == LightType::E_Rect) {
				const XMVECTOR lightDir = XMLoadFloat3(&light.Direction);
				const XMVECTOR lightUp = MathHelper::CalcUpVector(light.Direction);
				const XMVECTOR lightRight = XMVector3Cross(lightUp, lightDir);
				XMStoreFloat3(&light.Up, lightUp);
				XMStoreFloat3(&light.Right, lightRight);

				const XMVECTOR lightCenter = XMLoadFloat3(&light.Center);
				const FLOAT halfSizeX = light.Size.x * 0.5f;
				const FLOAT halfSizeY = light.Size.y * 0.5f;
				const XMVECTOR lightPos0 = lightCenter + lightUp * halfSizeY + lightRight * halfSizeX;
				const XMVECTOR lightPos1 = lightCenter + lightUp * halfSizeY - lightRight * halfSizeX;
				const XMVECTOR lightPos2 = lightCenter - lightUp * halfSizeY - lightRight * halfSizeX;
				const XMVECTOR lightPos3 = lightCenter - lightUp * halfSizeY + lightRight * halfSizeX;
				XMStoreFloat3(&light.Position,  lightPos0);
				XMStoreFloat3(&light.Position1, lightPos1);
				XMStoreFloat3(&light.Position2, lightPos2);
				XMStoreFloat3(&light.Position3, lightPos3);

				const XMVECTOR targetPos = lightCenter + lightDir;
				const XMMATRIX lightView = XMMatrixLookAtLH(lightCenter, targetPos, lightUp);
			}

			mMainPassCB->Lights[i] = light;
		}
	}
	// Main
	{
		const XMMATRIX view = XMLoadFloat4x4(&mCamera->View());
		const XMMATRIX proj = XMLoadFloat4x4(&mCamera->Proj());
		const XMMATRIX viewProj = XMMatrixMultiply(view, proj);

		const XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
		const XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		const XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		const XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);

		const size_t offsetIndex = static_cast<size_t>(GetCurrentFence()) % mFittedToBakcBufferHaltonSequence.size();

		mMainPassCB->PrevViewProj = mMainPassCB->ViewProj;
		XMStoreFloat4x4(&mMainPassCB->View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&mMainPassCB->InvView, XMMatrixTranspose(invView));
		XMStoreFloat4x4(&mMainPassCB->Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&mMainPassCB->InvProj, XMMatrixTranspose(invProj));
		XMStoreFloat4x4(&mMainPassCB->ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&mMainPassCB->InvViewProj, XMMatrixTranspose(invViewProj));
		XMStoreFloat4x4(&mMainPassCB->ViewProjTex, XMMatrixTranspose(viewProjTex));
		XMStoreFloat3(&mMainPassCB->EyePosW, mCamera->Position());
		mMainPassCB->JitteredOffset = bTaaEnabled ? mFittedToBakcBufferHaltonSequence[offsetIndex] : XMFLOAT2(0.f, 0.f);

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

	const XMMATRIX P = XMLoadFloat4x4(&mCamera->Proj());
	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	const XMMATRIX T(
		0.5f, 0.f,  0.f, 0.f,
		0.f, -0.5f, 0.f, 0.f,
		0.f,  0.f,  1.f, 0.f,
		0.5f, 0.5f, 0.f, 1.f
	);
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSSAO->GetOffsetVectors(ssaoCB.OffsetVectors);

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 2.f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	ssaoCB.SampleCount = ShaderArgument::SSAO::SampleCount;

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
	dofCB.FocusRange = ShaderArgument::DepthOfField::FocusRange;
	dofCB.FocusingSpeed = ShaderArgument::DepthOfField::FocusingSpeed;
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
	XMStoreFloat3(&ssrCB.EyePosW, mCamera->Position());
	ssrCB.MaxDistance = ShaderArgument::SSR::MaxDistance;
	ssrCB.RayLength = ShaderArgument::SSR::View::RayLength;
	ssrCB.NoiseIntensity = ShaderArgument::SSR::View::NoiseIntensity;
	ssrCB.StepCount = ShaderArgument::SSR::View::StepCount;
	ssrCB.BackStepCount = ShaderArgument::SSR::View::BackStepCount;
	ssrCB.DepthThreshold = ShaderArgument::SSR::View::DepthThreshold;
	ssrCB.Thickness = ShaderArgument::SSR::Screen::Thickness;
	ssrCB.Resolution = ShaderArgument::SSR::Screen::Resolution;

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
			const XMMATRIX world = XMLoadFloat4x4(&e->World);
			const XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ConstantBuffer_Object objCB;
			objCB.PrevWorld = e->PrevWolrd;
			XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objCB.TexTransform, XMMatrixTranspose(texTransform));
			objCB.Center = { e->AABB.Center.x, e->AABB.Center.y, e->AABB.Center.z, 1.f };
			objCB.Extents = { e->AABB.Extents.x, e->AABB.Extents.y, e->AABB.Extents.z, 0.f };

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
			const XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

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
		
	XMStoreFloat4x4(&irradCB.Proj, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.f, 0.1f, 10.f)));

	// Positive +X
	XMStoreFloat4x4(
		&irradCB.View[0],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.f, 0.f, 0.f, 1.f),
			XMVectorSet(1.f, 0.f, 0.f, 0.f),
			XMVectorSet(0.f, 1.f, 0.f, 0.f)
		))
	);
	// Positive -X
	XMStoreFloat4x4(
		&irradCB.View[1],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet( 0.f, 0.f, 0.f, 1.f),
			XMVectorSet(-1.f, 0.f, 0.f, 0.f),
			XMVectorSet( 0.f, 1.f, 0.f, 0.f)
		))
	);
	// Positive +Y
	XMStoreFloat4x4(
		&irradCB.View[2],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.f, 0.f,  0.f, 1.f),
			XMVectorSet(0.f, 1.f,  0.f, 0.f),
			XMVectorSet(0.f, 0.f, -1.f, 0.f)
		))
	);
	// Positive -Y
	XMStoreFloat4x4(
		&irradCB.View[3],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.f,  0.f, 0.f, 1.f),
			XMVectorSet(0.f, -1.f, 0.f, 0.f),
			XMVectorSet(0.f,  0.f, 1.f, 0.f)
		))
	);
	// Positive +Z
	XMStoreFloat4x4(
		&irradCB.View[4],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.f, 0.f, 0.f, 1.f),
			XMVectorSet(0.f, 0.f, 1.f, 0.f),
			XMVectorSet(0.f, 1.f, 0.f, 0.f)
		))
	);
	// Positive -Z
	XMStoreFloat4x4(
		&irradCB.View[5],
		XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(0.f, 0.f,  0.f, 1.f),
			XMVectorSet(0.f, 0.f, -1.f, 0.f),
			XMVectorSet(0.f, 1.f,  0.f, 0.f)
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

		ShaderArgument::SVGF::CheckerboardGenerateRaysForEvenPixels = !ShaderArgument::SVGF::CheckerboardGenerateRaysForEvenPixels;

		calcLocalMeanVarCB.TextureDim = { mClientWidth, mClientHeight };
		calcLocalMeanVarCB.KernelWidth = 9;
		calcLocalMeanVarCB.KernelRadius = 9 >> 1;

		calcLocalMeanVarCB.CheckerboardSamplingEnabled = ShaderArgument::SVGF::CheckerboardSamplingEnabled;
		calcLocalMeanVarCB.EvenPixelActivated = ShaderArgument::SVGF::CheckerboardGenerateRaysForEvenPixels;
		calcLocalMeanVarCB.PixelStepY = ShaderArgument::SVGF::CheckerboardSamplingEnabled ? 2 : 1;

		auto& currCB = mCurrFrameResource->CB_CalcLocalMeanVar;
		currCB.CopyData(0, calcLocalMeanVarCB);
	}
	// Temporal supersampling reverse reproject
	{
		ConstantBuffer_CrossBilateralFilter filterCB;
		filterCB.DepthSigma = 1.f;
		filterCB.DepthNumMantissaBits = D3D12Util::NumMantissaBitsInFloatFormat(16);

		auto& currCB = mCurrFrameResource->CB_CrossBilateralFilter;
		currCB.CopyData(0, filterCB);
	}
	// Temporal supersampling blend with current frame
	{
		ConstantBuffer_TemporalSupersamplingBlendWithCurrentFrame tsppBlendCB;
		tsppBlendCB.StdDevGamma = ShaderArgument::SVGF::TemporalSupersampling::ClampCachedValues::StdDevGamma;
		tsppBlendCB.ClampCachedValues = ShaderArgument::SVGF::TemporalSupersampling::ClampCachedValues::UseClamping;
		tsppBlendCB.ClampingMinStdDevTolerance = ShaderArgument::SVGF::TemporalSupersampling::ClampCachedValues::MinStdDevTolerance;

		tsppBlendCB.ClampDifferenceToTsppScale = ShaderArgument::SVGF::TemporalSupersampling::ClampDifferenceToTsppScale;
		tsppBlendCB.ForceUseMinSmoothingFactor = FALSE;
		tsppBlendCB.MinSmoothingFactor = 1.f / ShaderArgument::SVGF::TemporalSupersampling::MaxTspp;
		tsppBlendCB.MinTsppToUseTemporalVariance = ShaderArgument::SVGF::TemporalSupersampling::MinTsppToUseTemporalVariance;

		tsppBlendCB.BlurStrengthMaxTspp = ShaderArgument::SVGF::TemporalSupersampling::LowTsppMaxTspp;
		tsppBlendCB.BlurDecayStrength = ShaderArgument::SVGF::TemporalSupersampling::LowTsppDecayConstant;
		tsppBlendCB.CheckerboardEnabled = ShaderArgument::SVGF::CheckerboardSamplingEnabled;
		tsppBlendCB.CheckerboardEvenPixelActivated = ShaderArgument::SVGF::CheckerboardGenerateRaysForEvenPixels;

		auto& currCB = mCurrFrameResource->CB_TSPPBlend;
		currCB.CopyData(0, tsppBlendCB);
	}
	// Atrous wavelet transform filter
	{
		ConstantBuffer_AtrousWaveletTransformFilter atrousFilterCB;

		// Adaptive kernel radius rotation.
		FLOAT kernelRadiusLerfCoef = 0;
		if (ShaderArgument::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelEnabled) {
			static UINT frameID = 0;
			UINT i = frameID++ % ShaderArgument::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles;
			kernelRadiusLerfCoef = i / static_cast<FLOAT>(ShaderArgument::SVGF::AtrousWaveletTransformFilter::KernelRadiusRotateKernelNumCycles);
		}

		atrousFilterCB.TextureDim = XMUINT2(mClientWidth, mClientHeight);
		atrousFilterCB.DepthWeightCutoff = ShaderArgument::SVGF::AtrousWaveletTransformFilter::DepthWeightCutoff;
		atrousFilterCB.UsingBilateralDownsamplingBuffers = ShaderArgument::SVGF::QuarterResolutionAO;

		atrousFilterCB.UseAdaptiveKernelSize = ShaderArgument::SVGF::AtrousWaveletTransformFilter::UseAdaptiveKernelSize;
		atrousFilterCB.KernelRadiusLerfCoef = kernelRadiusLerfCoef;
		atrousFilterCB.MinKernelWidth = ShaderArgument::SVGF::AtrousWaveletTransformFilter::FilterMinKernelWidth;
		atrousFilterCB.MaxKernelWidth = static_cast<UINT>((ShaderArgument::SVGF::AtrousWaveletTransformFilter::FilterMaxKernelWidthPercentage / 100.f) * mClientWidth);

		atrousFilterCB.PerspectiveCorrectDepthInterpolation = ShaderArgument::SVGF::AtrousWaveletTransformFilter::PerspectiveCorrectDepthInterpolation;
		atrousFilterCB.MinVarianceToDenoise = ShaderArgument::SVGF::AtrousWaveletTransformFilter::MinVarianceToDenoise;

		atrousFilterCB.ValueSigma = ShaderArgument::SVGF::AtrousWaveletTransformFilter::ValueSigma;
		atrousFilterCB.DepthSigma = ShaderArgument::SVGF::AtrousWaveletTransformFilter::DepthSigma;
		atrousFilterCB.NormalSigma = ShaderArgument::SVGF::AtrousWaveletTransformFilter::NormalSigma;
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
	rtaoCB.OcclusionRadius = ShaderArgument::RTAO::OcclusionRadius;
	rtaoCB.OcclusionFadeStart = ShaderArgument::RTAO::OcclusionFadeStart;
	rtaoCB.OcclusionFadeEnd = ShaderArgument::RTAO::OcclusionFadeEnd;
	rtaoCB.SurfaceEpsilon = ShaderArgument::RTAO::OcclusionEpsilon;

	rtaoCB.FrameCount = count++;
	rtaoCB.SampleCount = ShaderArgument::RTAO::SampleCount;

	prev = mMainPassCB->View;

	auto& currCB = mCurrFrameResource->CB_RTAO;
	currCB.CopyData(0, rtaoCB);

	return TRUE;
}

BOOL DxRenderer::UpdateCB_Debug(FLOAT delta) {
	ConstantBuffer_Debug debugMapCB;

	for (UINT i = 0; i < Debug::MapSize; ++i) {
		const auto desc = mDebug->SampleDesc(i);
		debugMapCB.SampleDescs[i] = desc;
	}

	auto& currCB = mCurrFrameResource->CB_Debug;
	currCB.CopyData(0, debugMapCB);

	return TRUE;
}

BOOL DxRenderer::AddBLAS(ID3D12GraphicsCommandList4* const cmdList, MeshGeometry* const geo) {
	std::unique_ptr<AccelerationStructureBuffer> blas = std::make_unique<AccelerationStructureBuffer>();

	CheckReturn(blas->BuildBLAS(md3dDevice.Get(), cmdList, geo));

	mBLASRefs[geo->Name] = blas.get();
	mBLASes.emplace_back(std::move(blas));
	
	bNeedToRebuildTLAS = TRUE;
	bNeedToRebuildShaderTables = TRUE;

	return TRUE;
}

BOOL DxRenderer::BuildTLAS(ID3D12GraphicsCommandList4* const cmdList) {
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

	const auto& opaques = mRitemRefs[RenderType::E_Opaque];

	for (const auto ri : opaques) {
		auto iter = std::find(opaques.begin(), opaques.end(), ri);

		const UINT hitGroupIndex = static_cast<UINT>(std::distance(opaques.begin(), iter));

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

		const UINT hitGroupIndex = static_cast<UINT>(std::distance(opaques.begin(), iter));

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
	const UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Object));
	const UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ConstantBuffer_Material));

	const UINT numRitems = static_cast<UINT>(opaques.size());
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

BOOL DxRenderer::PrePass() {
	CheckReturn(DrawGBuffer());
	if (bRaytracing) {
		CheckReturn(DrawDXRShadow());
		CheckReturn(DrawRTAO());
	}
	else {
		CheckReturn(DrawShadow());
		CheckReturn(DrawSSAO());
	}

	return TRUE;
}

BOOL DxRenderer::MainPass() {
	if (bRaytracing) {
		CheckReturn(DrawDXRBackBuffer());
		CheckReturn(CalcDepthPartialDerivative());
	}
	else {
		CheckReturn(DrawBackBuffer());
	}

	return TRUE;
}

BOOL DxRenderer::PostPass() {
	CheckReturn(DrawSkySphere());

	if (bRaytracing) { CheckReturn(BuildRaytracedReflection()); }
	else { CheckReturn(ApplySSR()); }

	CheckReturn(IntegrateSpecIrrad());

	CheckReturn(ApplyVolumetricLight());
	if (bBloomEnabled) CheckReturn(ApplyBloom());
	if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());

	CheckReturn(ResolveToneMapping());
	if (bGammaCorrectionEnabled) CheckReturn(ApplyGammaCorrection());

	if (bTaaEnabled) CheckReturn(ApplyTAA());
	if (bSharpenEnabled) CheckReturn(ApplySharpen());
	if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
	if (bPixelationEnabled) CheckReturn(ApplyPixelation());

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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	for (UINT i = 0; i < mLightCount; ++i) {
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
			mZDepth->ZDepthMap(i),
			mZDepth->FaceIDCubeMap(i),
			mLights[i].Type,
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
	
	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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
		ShaderArgument::GBuffer::Dither::MaxDistance,
		ShaderArgument::GBuffer::Dither::MinDistance
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
	
	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	const auto ssaoCBAddr = mCurrFrameResource->CB_SSAO.Resource()->GetGPUVirtualAddress();
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto aoCoeffDesc = bRaytracing ? mRTAO->ResolvedAOCoefficientSrv() : mSSAO->AOCoefficientMapSrv(0);
	const auto reflectionDesc = bRaytracing ? mRR->ResolvedReflectionSrv() : mSSR->SSRMapSrv(0);

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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

BOOL DxRenderer::ApplyTAA() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mTAA->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		mSwapChainBuffer->CurrentBackBufferSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgument::TemporalAA::ModulationFactor
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

		const auto ssrCBAddr = mCurrFrameResource->CB_SSR.Resource()->GetGPUVirtualAddress();

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

		const auto blurCBAddr = mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress();
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
			ShaderArgument::SSR::BlurCount
		);

		CheckHRESULT(cmdList->Close());
		mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(&cmdList));
	}
	else {
		if (!bSsrMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			cmdList->ClearRenderTargetView(mSSR->SSRMapRtv(0), SSR::SSRMapClearValues, 0, nullptr);

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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	auto backBufferSrv = mToneMapping->InterMediateMapSrv();

	const auto bloomMap0 = mBloom->BloomMapResource(0);
	const auto bloomMap1 = mBloom->BloomMapResource(1);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mBloom->ExtractHighlight(
		cmdList,
		backBufferSrv,
		ShaderArgument::Bloom::HighlightThreshold
	);
	
	const auto blurPassCBAddress = mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress();
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
		ShaderArgument::Bloom::BlurCount
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
	
	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mToneMapping->InterMediateMapResource();
	const auto backBufferRtv = mToneMapping->InterMediateMapRtv();
	const auto backBufferSrv = mToneMapping->InterMediateMapSrv();

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
		ShaderArgument::DepthOfField::BokehRadius,
		ShaderArgument::DepthOfField::CoCMaxDevTolerance,
		ShaderArgument::DepthOfField::HighlightPower,
		ShaderArgument::DepthOfField::SampleCount
	);

	mDoF->BlurDoF(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mCurrFrameResource->CB_Blur.Resource()->GetGPUVirtualAddress(),
		backBuffer,
		backBufferRtv,
		backBufferSrv,
		ShaderArgument::DepthOfField::BlurCount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplyMotionBlur() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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
		ShaderArgument::MotionBlur::Intensity,
		ShaderArgument::MotionBlur::Limit,
		ShaderArgument::MotionBlur::DepthBias,
		ShaderArgument::MotionBlur::SampleCount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ResolveToneMapping() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	if (bToneMappingEnabled) {
		mToneMapping->Resolve(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBuffer(),
			mSwapChainBuffer->CurrentBackBufferRtv(),
			ShaderArgument::ToneMapping::Exposure
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mGammaCorrection->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgument::GammaCorrection::Gamma
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplySharpen() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mSharpen->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgument::Sharpen::Amount
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplyPixelation() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mPixelation->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		ShaderArgument::Pixelization::PixelSize
	);

	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::ApplyVolumetricLight() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mVolumetricLight->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mToneMapping->InterMediateMapResource(),
		mToneMapping->InterMediateMapRtv(),
		mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
		mZDepth->ZDepthSrvs(),
		mZDepth->ZDepthCubeSrvs(),
		mZDepth->FaceIDCubeSrvs(),
		mGBuffer->PositionMapSrv(),
		mCamera->NearZ(),
		mCamera->FarZ(),
		ShaderArgument::VolumetricLight::DepthExponent,
		ShaderArgument::VolumetricLight::UniformDensity,
		ShaderArgument::VolumetricLight::AnisotropicCoefficient,
		ShaderArgument::VolumetricLight::DensityScale
	);
	
	CheckHRESULT(cmdList->Close());
	mCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&cmdList));

	return TRUE;
}

BOOL DxRenderer::DrawDXRShadow() {
	const auto cmdList = mCommandList.Get();	
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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
	ID3D12DescriptorHeap* const descriptorHeaps[] = { pDescHeap };
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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
				const UINT temporalPreviousFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
				const UINT temporalCurrentFrameResourcIndex = mRTAO->MoveToNextFrame();

				const UINT temporalPreviousFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
				const UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->MoveToNextFrameTemporalAOCoefficient();

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
					ShaderArgument::SVGF::CheckerboardSamplingEnabled
				);
				// Interpolate the variance for the inactive cells from the valid checkerboard cells.
				if (ShaderArgument::SVGF::CheckerboardSamplingEnabled) {
					mSVGF->FillInCheckerboard(
						cmdList,
						mCurrFrameResource->CB_CalcLocalMeanVar.Resource()->GetGPUVirtualAddress(),
						mClientWidth, mClientHeight
					);
				}

				// Blends reprojected values with current frame values.
				// Inactive pixels are filtered from active neighbors on checkerboard sampling before the blending operation.
				{
					const UINT temporalCurrentFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
					const UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

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

				if (ShaderArgument::RTAO::Denoiser::UseSmoothingVariance) {
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
			if (ShaderArgument::RTAO::Denoiser::FullscreenBlur) {
				const UINT temporalCurrentFrameResourceIndex = mRTAO->TemporalCurrentFrameResourceIndex();
				const UINT inputAOCoefficientIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();
				const UINT outputAOCoefficientIndex = mRTAO->MoveToNextFrameTemporalAOCoefficient();

				const auto outputAOCoefficient = temporalAOCoefficients[outputAOCoefficientIndex].get();

				outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, outputAOCoefficient);

				const FLOAT rayHitDistToKernelWidthScale = 22 / ShaderArgument::RTAO::OcclusionRadius *
					ShaderArgument::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleFactor;
				const FLOAT rayHitDistToKernelSizeScaleExp = D3D12Util::Lerp(
					1,
					ShaderArgument::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleExponent,
					D3D12Util::RelativeCoef(ShaderArgument::RTAO::OcclusionRadius, 4, 22)
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
					ShaderArgument::RTAO::Denoiser::UseSmoothingVariance
				);

				outputAOCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, outputAOCoefficient);
			}
			// Stage 2: 3x3 multi-pass disocclusion blur (with more relaxed depth-aware constraints for such pixels).
			if (ShaderArgument::RTAO::Denoiser::DisocclusionBlur) {
				const UINT temporalCurrentFrameTemporalAOCoefficientResourceIndex = mRTAO->TemporalCurrentFrameTemporalAOCoefficientResourceIndex();

				const auto aoCoefficient = temporalAOCoefficients[temporalCurrentFrameTemporalAOCoefficientResourceIndex].get();

				aoCoefficient->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, aoCoefficient);

				mSVGF->BlurDisocclusion(
					cmdList,
					aoCoefficient,
					mGBuffer->DepthMapSrv(),
					mGBuffer->RMSMapSrv(),
					temporalAOCoefficientsGpuDescriptors[temporalCurrentFrameTemporalAOCoefficientResourceIndex][RTAO::Descriptor::TemporalAOCoefficient::Uav],
					mClientWidth, mClientHeight,
					ShaderArgument::RTAO::Denoiser::LowTsppBlurPasses,
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
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
			ShaderArgument::RaytracedReflection::ReflectionRadius
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
				const UINT temporalPreviousFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
				const UINT temporalCurrentFrameResourcIndex = mRR->MoveToNextFrame();
			
				const UINT temporalPreviousFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				const UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->MoveToNextFrameTemporalReflection();
			
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
					ShaderArgument::RaytracedReflection::CheckerboardSamplingEnabled
				);

				// Blends reprojected values with current frame values.
				// Inactive pixels are filtered from active neighbors on checkerboard sampling before the blending operation.
				{
					const UINT temporalCurrentFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
					const UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				
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
				if (ShaderArgument::RaytracedReflection::Denoiser::UseSmoothingVariance) {
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
			if (ShaderArgument::RaytracedReflection::Denoiser::FullscreenBlur) {
				const UINT temporalCurrentFrameResourceIndex = mRR->TemporalCurrentFrameResourceIndex();
				const UINT inputReflectionIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();
				const UINT outputReflectionIndex = mRR->MoveToNextFrameTemporalReflection();

				const auto outputReflection = temporalReflections[outputReflectionIndex].get();

				outputReflection->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, outputReflection);

				const FLOAT rayHitDistToKernelWidthScale = 22 / ShaderArgument::RaytracedReflection::ReflectionRadius *
					ShaderArgument::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleFactor;
				const FLOAT rayHitDistToKernelSizeScaleExp = D3D12Util::Lerp(
					1,
					ShaderArgument::SVGF::AtrousWaveletTransformFilter::AdaptiveKernelSizeRayHitDistanceScaleExponent,
					D3D12Util::RelativeCoef(ShaderArgument::RaytracedReflection::ReflectionRadius, 4, 22)
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
					ShaderArgument::RaytracedReflection::Denoiser::UseSmoothingVariance
				);

				outputReflection->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				D3D12Util::UavBarrier(cmdList, outputReflection);
			}
			// Stage 2: 3x3 multi-pass disocclusion blur (with more relaxed depth-aware constraints for such pixels).
			if (ShaderArgument::RaytracedReflection::Denoiser::DisocclusionBlur) {
				const UINT temporalCurrentFrameTemporalReflectionResourceIndex = mRR->TemporalCurrentFrameTemporalReflectionResourceIndex();

				const auto reflections = temporalReflections[temporalCurrentFrameTemporalReflectionResourceIndex].get();

				reflections->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				D3D12Util::UavBarrier(cmdList, reflections);

				mSVGF->BlurDisocclusion(
					cmdList,
					reflections,
					mGBuffer->DepthMapSrv(),
					mGBuffer->RMSMapSrv(),
					temporalReflectionGpuDescriptors[temporalCurrentFrameTemporalReflectionResourceIndex][RaytracedReflection::Descriptor::TemporalReflection::E_Uav],
					mClientWidth, mClientHeight,
					ShaderArgument::RaytracedReflection::Denoiser::LowTsppBlurPasses,
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

BOOL DxRenderer::DrawDebuggingInfo() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mDebug->DebugTexMap(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBuffer(),
		mCurrFrameResource->CB_Debug.Resource()->GetGPUVirtualAddress(),
		mSwapChainBuffer->CurrentBackBufferRtv(),
		DepthStencilView()
	);

	if (ShaderArgument::IrradianceMap::ShowIrradianceCubeMap) {
		D3D12_GPU_DESCRIPTOR_HANDLE si_input = {};

		switch (mDebug->DebugCubeMapType) {
		case IrradianceMap::CubeMap::E_DiffuseIrradianceCube:
			si_input = mIrradianceMap->DiffuseIrradianceCubeMapSrv();
			break;
		case IrradianceMap::CubeMap::E_EnvironmentCube:
			si_input = mIrradianceMap->EnvironmentCubeMapSrv();
			break;
		case IrradianceMap::CubeMap::E_PrefilteredIrradianceCube:
			si_input = mIrradianceMap->PrefilteredEnvironmentCubeMapSrv();
			break;
		case IrradianceMap::CubeMap::E_Equirectangular:
			si_input = mIrradianceMap->EquirectangularMapSrv();
			break;
		}

		mDebug->DebugCubeMap(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBuffer(),
			mSwapChainBuffer->CurrentBackBufferRtv(),
			mCurrFrameResource->CB_Pass.Resource()->GetGPUVirtualAddress(),
			si_input,
			ShaderArgument::IrradianceMap::MipLevel
		);
	}

	if (ShaderArgument::Debug::ShowCollisionBox) {
		mDebug->ShowCollsion(
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

	ID3D12DescriptorHeap* const descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	const auto backBufferRtv = mSwapChainBuffer->CurrentBackBufferRtv();
	cmdList->OMSetRenderTargets(1, &backBufferRtv, TRUE, nullptr);

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static const auto BuildDebugMap = [&](BOOL& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, Debug::SampleMask::Type type) {
		if (mode) { if (!mDebug->AddDebugMap(handle, type)) mode = FALSE; }
		else { mDebug->RemoveDebugMap(handle); }
	};
	static const auto BuildDebugMapWithSampleDesc = [&](
		BOOL& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, Debug::SampleMask::Type type, DebugSampleDesc desc) {
			if (mode) { if (!mDebug->AddDebugMap(handle, type, desc)) mode = FALSE; }
			else { mDebug->RemoveDebugMap(handle); }
	};

	// Main Panel
	{
		ImGui::Begin("Main Panel");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Debug")) {
			if (ImGui::TreeNode("Texture Maps")) {
				if (ImGui::TreeNode("G-Buffer")) {
					if (ImGui::Checkbox("Albedo", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Albedo]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Albedo],
							mGBuffer->AlbedoMapSrv(),
							Debug::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Normal", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Normal]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Normal],
							mGBuffer->NormalMapSrv(),
							Debug::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Depth", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Depth]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Depth],
							mGBuffer->DepthMapSrv(),
							Debug::SampleMask::RRR);
					}
					if (ImGui::Checkbox("RoughnessMetalicSpecular", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RMS]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_RMS],
							mGBuffer->RMSMapSrv(),
							Debug::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Velocity", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Velocity]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Velocity],
							mGBuffer->VelocityMapSrv(),
							Debug::SampleMask::RG);
					}
					ImGui::TreePop();
				} // ImGui::TreeNode("G-Buffer")
				if (ImGui::TreeNode("Irradiance")) {
					if (ImGui::Checkbox("Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Equirectangular]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_Equirectangular],
							mIrradianceMap->EquirectangularMapSrv(),
							Debug::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Temporary Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular],
							mIrradianceMap->TemporaryEquirectangularMapSrv(),
							Debug::SampleMask::RGB);
					}
					if (ImGui::Checkbox("Diffuse Irradiance Equirectangular Map",
						reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect],
							mIrradianceMap->DiffuseIrradianceEquirectMapSrv(),
							Debug::SampleMask::RGB);
					}
					ImGui::TreePop(); // ImGui::TreeNode("Irradiance")
				}
				if (ImGui::Checkbox("Bloom", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Bloom]))) {
					BuildDebugMap(
						mDebugMapStates[DebugMapLayout::E_Bloom],
						mBloom->BloomMapSrv(0),
						Debug::SampleMask::RGB);
				}

				if (ImGui::TreeNode("Rasterization")) {
					if (ImGui::TreeNode("Shadow")) {
						auto debug = mShadow->DebugShadowMap();
						if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(debug))) {
							BuildDebugMap(
								*debug,
								mShadow->Srv(Shadow::Descriptor::ESI_Shadow),
								Debug::SampleMask::RRR);
						}

						ImGui::TreePop();
					}
					if (ImGui::Checkbox("SSAO", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSAO]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSAO],
							mSSAO->AOCoefficientMapSrv(0),
							Debug::SampleMask::RRR);
					}
					if (ImGui::Checkbox("SSR", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_SSR]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_SSR],
							mSSR->SSRMapSrv(0),
							Debug::SampleMask::RGB);
					}

					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Raytracing")) {
					if (ImGui::Checkbox("Shadow", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_DxrShadow]))) {
						BuildDebugMap(
							mDebugMapStates[DebugMapLayout::E_DxrShadow],
							mDxrShadow->Descriptor(),
							Debug::SampleMask::RRR);
					}
					if (ImGui::TreeNode("RTAO")) {
						auto index = mRTAO->TemporalCurrentFrameResourceIndex();

						if (ImGui::Checkbox("AO Coefficients", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_AOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_AOCoeff],
								mRTAO->AOResourceGpuDescriptors()[RTAO::Descriptor::AO::ES_AmbientCoefficient],
								Debug::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Temporal AO Coefficients",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_TemporalAOCoeff],
								mRTAO->TemporalAOCoefficientGpuDescriptors()[index][RTAO::Descriptor::TemporalAOCoefficient::Srv],
								Debug::SampleMask::RRR);
						}
						if (ImGui::Checkbox("Tspp", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_Tspp]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 153.f / 255.f, 18.f / 255.f, 15.f / 255.f, 1.f };
							desc.MaxColor = { 170.f / 255.f, 220.f / 255.f, 200.f / 255.f, 1.f };
							desc.Denominator = 22.f;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_Tspp],
								mRTAO->TemporalCacheGpuDescriptors()[index][RTAO::Descriptor::TemporalCache::ES_Tspp],
								Debug::SampleMask::UINT,
								desc);
						}
						if (ImGui::Checkbox("Ray Hit Distance", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RayHitDist]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 15.f / 255.f, 18.f / 255.f, 153.f / 255.f, 1.f };
							desc.MaxColor = { 170.f / 255.f, 220.f / 255.f, 200.f / 255.f, 1.f };
							desc.Denominator = ShaderArgument::RTAO::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RayHitDist],
								mRTAO->AOResourceGpuDescriptors()[RTAO::Descriptor::AO::ES_RayHitDistance],
								Debug::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Temporal Ray Hit Distance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 12.f / 255.f, 64.f / 255.f, 18.f / 255.f, 1.f };
							desc.MaxColor = { 180.f / 255.f, 197.f / 255.f, 231.f / 255.f, 1.f };
							desc.Denominator = ShaderArgument::RTAO::OcclusionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_TemporalRayHitDist],
								mRTAO->TemporalCacheGpuDescriptors()[index][RTAO::Descriptor::TemporalCache::ES_RayHitDistance],
								Debug::SampleMask::FLOAT,
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
								Debug::SampleMask::RGB);
						}
						if (ImGui::Checkbox("Temporal Reflection", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_TemporalReflection]))) {
							BuildDebugMap(
								mDebugMapStates[DebugMapLayout::E_RR_TemporalReflection],
								mRR->ResolvedReflectionSrv(),
								Debug::SampleMask::RGB);
						}
						if (ImGui::Checkbox("Tspp", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_Tspp]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 153.f / 255.f, 18.f / 255.f, 15.f / 255.f, 1.f };
							desc.MaxColor = { 170.f / 255.f, 220.f / 255.f, 200.f / 255.f, 1.f };
							desc.Denominator = 22.f;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_Tspp],
								mRR->TemporalCacheGpuDescriptors()[index][RaytracedReflection::Descriptor::TemporalCache::ES_Tspp],
								Debug::SampleMask::UINT,
								desc);
						}
						if (ImGui::Checkbox("Ray Hit Distance", reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_RayHitDist]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 15.f / 255.f, 18.f / 255.f, 153.f / 255.f, 1.f };
							desc.MaxColor = { 170.f / 255.f, 220.f / 255.f, 200.f / 255.f, 1.f };
							desc.Denominator = ShaderArgument::RaytracedReflection::ReflectionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_RayHitDist],
								mRR->ReflectionGpuDescriptors()[RaytracedReflection::Descriptor::Reflection::ES_RayHitDistance],
								Debug::SampleMask::FLOAT,
								desc);
						}
						if (ImGui::Checkbox("Temporal Ray Hit Distance",
							reinterpret_cast<bool*>(&mDebugMapStates[DebugMapLayout::E_RR_TemporalRayHitDist]))) {
							DebugSampleDesc desc;
							desc.MinColor = { 12.f / 255.f, 64.f / 255.f, 18.f / 255.f, 1.f };
							desc.MaxColor = { 180.f / 255.f, 197.f / 255.f, 231.f / 255.f, 1.f };
							desc.Denominator = ShaderArgument::RaytracedReflection::ReflectionRadius;

							BuildDebugMapWithSampleDesc(
								mDebugMapStates[DebugMapLayout::E_RR_TemporalRayHitDist],
								mRR->TemporalCacheGpuDescriptors()[index][RaytracedReflection::Descriptor::TemporalCache::ES_RayHitDistance],
								Debug::SampleMask::FLOAT,
								desc);
						}

						ImGui::TreePop(); // ImGui::TreeNode("Raytraced Reflection")
					}

					ImGui::TreePop(); // ImGui::TreeNode("RTAO")
				}

				ImGui::TreePop(); // ImGui::TreeNode("Texture Maps")
			}

			ImGui::Checkbox("Show Collision Box", reinterpret_cast<bool*>(&ShaderArgument::Debug::ShowCollisionBox));
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
			ImGui::Checkbox("Show Irradiance CubeMap", reinterpret_cast<bool*>(&ShaderArgument::IrradianceMap::ShowIrradianceCubeMap));
			if (ShaderArgument::IrradianceMap::ShowIrradianceCubeMap) {
				ImGui::RadioButton(
					"Environment CubeMap",
					reinterpret_cast<INT*>(&mDebug->DebugCubeMapType),
					IrradianceMap::CubeMap::E_EnvironmentCube);
				ImGui::RadioButton(
					"Equirectangular Map",
					reinterpret_cast<INT*>(&mDebug->DebugCubeMapType),
					IrradianceMap::CubeMap::E_Equirectangular);
				ImGui::RadioButton(
					"Diffuse Irradiance CubeMap",
					reinterpret_cast<INT*>(&mDebug->DebugCubeMapType),
					IrradianceMap::CubeMap::E_DiffuseIrradianceCube);
				ImGui::RadioButton(
					"Prefiltered Irradiance CubeMap",
					reinterpret_cast<INT*>(&mDebug->DebugCubeMapType),
					IrradianceMap::CubeMap::E_PrefilteredIrradianceCube);
				ImGui::SliderFloat("Mip Level", &ShaderArgument::IrradianceMap::MipLevel, 0.f, IrradianceMap::MaxMipLevel - 1);
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
				ImGui::SliderInt("Sample Count", &ShaderArgument::SSAO::SampleCount, 1, 32);
				ImGui::SliderInt("Number of Blurs", &ShaderArgument::SSAO::BlurCount, 0, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("SVGF")) {
				ImGui::Checkbox("Clamp Cached Values", reinterpret_cast<bool*>(&ShaderArgument::SVGF::TemporalSupersampling::ClampCachedValues::UseClamping));

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("RTAO")) {
				ImGui::SliderInt("Sample Count", reinterpret_cast<int*>(&ShaderArgument::RTAO::SampleCount), 1, 4);
				ImGui::Checkbox("Use Smoothing Variance", reinterpret_cast<bool*>(&ShaderArgument::RTAO::Denoiser::UseSmoothingVariance));
				ImGui::Checkbox("Disocclusion Blur", reinterpret_cast<bool*>(&ShaderArgument::RTAO::Denoiser::DisocclusionBlur));
				ImGui::Checkbox("Fullscreen Blur", reinterpret_cast<bool*>(&ShaderArgument::RTAO::Denoiser::FullscreenBlur));
				ImGui::SliderInt("Low Tspp Blur Passes", reinterpret_cast<int*>(&ShaderArgument::RTAO::Denoiser::LowTsppBlurPasses), 1, 8);

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("Main Pass")) {
			if (ImGui::TreeNode("Dithering Transparency")) {
				ImGui::SliderFloat("Max Distance", &ShaderArgument::GBuffer::Dither::MaxDistance, 0.1f, 10.f);
				ImGui::SliderFloat("Min Distance", &ShaderArgument::GBuffer::Dither::MinDistance, 0.01f, 1.f);

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("Post Pass")) {
			if (ImGui::TreeNode("TAA")) {
				ImGui::SliderFloat("Modulation Factor", &ShaderArgument::TemporalAA::ModulationFactor, 0.01f, 0.99f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Motion Blur")) {
				ImGui::SliderFloat("Intensity", &ShaderArgument::MotionBlur::Intensity, 0.01f, 0.1f);
				ImGui::SliderFloat("Limit", &ShaderArgument::MotionBlur::Limit, 0.001f, 0.01f);
				ImGui::SliderFloat("Depth Bias", &ShaderArgument::MotionBlur::DepthBias, 0.001f, 0.01f);
				ImGui::SliderInt("Sample Count", &ShaderArgument::MotionBlur::SampleCount, 1, 32);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Depth of Field")) {
				ImGui::SliderFloat("Focus Range", &ShaderArgument::DepthOfField::FocusRange, 0.1f, 100.f);
				ImGui::SliderFloat("Focusing Speed", &ShaderArgument::DepthOfField::FocusingSpeed, 1.f, 10.f);
				ImGui::SliderFloat("Bokeh Radius", &ShaderArgument::DepthOfField::BokehRadius, 1.f, 8.f);
				ImGui::SliderFloat("CoC Max Deviation Tolerance", &ShaderArgument::DepthOfField::CoCMaxDevTolerance, 0.1f, 0.9f);
				ImGui::SliderFloat("Highlight Power", &ShaderArgument::DepthOfField::HighlightPower, 1.f, 32.f);
				ImGui::SliderInt("Sample Count", &ShaderArgument::DepthOfField::SampleCount, 1, 8);
				ImGui::SliderInt("Blur Count", reinterpret_cast<INT*>(&ShaderArgument::DepthOfField::BlurCount), 0, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Bloom")) {
				ImGui::SliderInt("Blur Count", &ShaderArgument::Bloom::BlurCount, 0, 8);
				ImGui::SliderFloat("Highlight Threshold", &ShaderArgument::Bloom::HighlightThreshold, 0.1f, 0.99f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("SSR")) {
				ImGui::RadioButton("Screen Space", reinterpret_cast<INT*>(&mSSR->StateType), SSR::PipelineState::EG_ScreenSpace); ImGui::SameLine();
				ImGui::RadioButton("View Space", reinterpret_cast<INT*>(&mSSR->StateType), SSR::PipelineState::EG_ViewSpace);

				ImGui::SliderFloat("Max Distance", &ShaderArgument::SSR::MaxDistance, 1.f, 100.f);
				ImGui::SliderInt("Blur Count", &ShaderArgument::SSR::BlurCount, 0, 8);

				ImGui::Text("View");
				ImGui::SliderFloat("Ray Length", &ShaderArgument::SSR::View::RayLength, 1.f, 64.f);
				ImGui::SliderFloat("Noise Intensity", &ShaderArgument::SSR::View::NoiseIntensity, 0.1f, 0.001f);
				ImGui::SliderInt("Step Count", &ShaderArgument::SSR::View::StepCount, 1, 32);
				ImGui::SliderInt("Back Step Count", &ShaderArgument::SSR::View::BackStepCount, 1, 16);
				ImGui::SliderFloat("Depth Threshold", &ShaderArgument::SSR::View::DepthThreshold, 0.1f, 10.f);

				ImGui::Text("Screen");
				ImGui::SliderFloat("Thickness", &ShaderArgument::SSR::Screen::Thickness, 0.01f, 1.f);
				ImGui::SliderFloat("Resolution", &ShaderArgument::SSR::Screen::Resolution, 0.f, 1.f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Tone Mapping")) {
				ImGui::SliderFloat("Exposure", &ShaderArgument::ToneMapping::Exposure, 0.1f, 10.f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Gamma Correction")) {
				ImGui::SliderFloat("Gamma", &ShaderArgument::GammaCorrection::Gamma, 0.1f, 10.f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Pixelization")) {
				ImGui::SliderFloat("Pixel Size", &ShaderArgument::Pixelization::PixelSize, 1.f, 20.f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Sharpen")) {
				ImGui::SliderFloat("Amount", &ShaderArgument::Sharpen::Amount, 0.f, 10.f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Raytraced Reflection")) {
				ImGui::Checkbox("Use Smoothing Variance", reinterpret_cast<bool*>(&ShaderArgument::RaytracedReflection::Denoiser::UseSmoothingVariance));
				ImGui::Checkbox("Fullscreen Blur", reinterpret_cast<bool*>(&ShaderArgument::RaytracedReflection::Denoiser::FullscreenBlur));
				ImGui::Checkbox("Disocclusion Blur", reinterpret_cast<bool*>(&ShaderArgument::RaytracedReflection::Denoiser::DisocclusionBlur));
				ImGui::SliderInt("Low Tspp Blur Passes", reinterpret_cast<int*>(&ShaderArgument::RaytracedReflection::Denoiser::LowTsppBlurPasses), 1, 8);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Volumetric Light")) {
				ImGui::SliderFloat("Depth Exponent", &ShaderArgument::VolumetricLight::DepthExponent, 1.f, 16.f);
				ImGui::SliderFloat("Uniform Density", &ShaderArgument::VolumetricLight::UniformDensity, 0.f, 1.f);
				ImGui::SliderFloat("Anisotropic Coefficient", &ShaderArgument::VolumetricLight::AnisotropicCoefficient, -0.5f, 0.5f);
				ImGui::SliderFloat("Density Scale", &ShaderArgument::VolumetricLight::DensityScale, 0.01f, 1.f);

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
					ImGui::ColorPicker3("Color", reinterpret_cast<FLOAT*>(&light.Color));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 100.f);
					if (ImGui::SliderFloat3("Direction", reinterpret_cast<FLOAT*>(&light.Direction), -1.f, 1.f)) {
						XMVECTOR dir = XMLoadFloat3(&light.Direction);
						XMVECTOR normalized = XMVector3Normalize(dir);
						XMStoreFloat3(&light.Direction, normalized);
					}

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Point) {
				if (ImGui::TreeNode((std::to_string(i) + " Point Light").c_str())) {
					ImGui::ColorPicker3("Color", reinterpret_cast<FLOAT*>(&light.Color));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1000.f);
					ImGui::SliderFloat3("Position", reinterpret_cast<FLOAT*>(&light.Position), -100.f, 100.f, "%.3f");
					ImGui::SliderFloat("Radius", &light.Radius, 0.f, 100.f);
					ImGui::SliderFloat("Attenuation Radius", &light.AttenuationRadius, 0.f, 100.f);

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Spot) {
				if (ImGui::TreeNode((std::to_string(i) + " Spot Light").c_str())) {
					ImGui::ColorPicker3("Color", reinterpret_cast<FLOAT*>(&light.Color));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1000.f);
					ImGui::SliderFloat3("Position", reinterpret_cast<FLOAT*>(&light.Position), -100.f, 100.f);
					if (ImGui::SliderFloat3("Direction", reinterpret_cast<FLOAT*>(&light.Direction), -1.f, 1.f)) {
						XMVECTOR dir = XMLoadFloat3(&light.Direction);
						XMVECTOR normalized = XMVector3Normalize(dir);
						XMStoreFloat3(&light.Direction, normalized);
					}
					ImGui::SliderFloat("Inner Cone Angle", &light.InnerConeAngle, 0.f, 80.f);
					ImGui::SliderFloat("Outer Cone Angle", &light.OuterConeAngle, 0.f, 80.f);
					ImGui::SliderFloat("Attenuation Radius", &light.AttenuationRadius, 0.f, 100.f);

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Tube) {
				if (ImGui::TreeNode((std::to_string(i) + " Tube Light").c_str())) {
					ImGui::ColorPicker3("Color", reinterpret_cast<FLOAT*>(&light.Color));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1000.f);
					ImGui::SliderFloat3("Position0", reinterpret_cast<FLOAT*>(&light.Position), -100.f, 100.f, "%.3f");
					ImGui::SliderFloat3("Position1", reinterpret_cast<FLOAT*>(&light.Position1), -100.f, 100.f, "%.3f");
					ImGui::SliderFloat("Radius", &light.Radius, 0.f, 100.f);
					ImGui::SliderFloat("Attenuation Radius", &light.AttenuationRadius, 0.f, 100.f);

					ImGui::TreePop();
				}
			}
			else if (light.Type == LightType::E_Rect) {
				if (ImGui::TreeNode((std::to_string(i) + " Rect Light").c_str())) {
					ImGui::ColorPicker3("Color", reinterpret_cast<FLOAT*>(&light.Color));
					ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1000.f);
					ImGui::SliderFloat3("Center", reinterpret_cast<FLOAT*>(&light.Center), -100.f, 100.f, "%.3f");
					if (ImGui::SliderFloat3("Direction", reinterpret_cast<FLOAT*>(&light.Direction), -1.f, 1.f)) {
						XMVECTOR dir = XMLoadFloat3(&light.Direction);
						XMVECTOR normalized = XMVector3Normalize(dir);
						XMStoreFloat3(&light.Direction, normalized);
					}
					ImGui::SliderFloat2("Size", reinterpret_cast<FLOAT*>(&light.Size), 0.f, 100.f);
					ImGui::SliderFloat("Attenuation Radius", &light.AttenuationRadius, 0.f, 100.f);

					ImGui::TreePop();
				}
			}
		}

		if (mLightCount < MaxLights) {
			if (ImGui::Button("Directional")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Directional;
				light.Color = { 1.f, 1.f, 1.f };
				light.Intensity = 1.f;
				light.Direction = { 0.f, -1.f, 0.f };

				mZDepth->AddLight(light.Type, mLightCount++);
			}
			ImGui::SameLine();
			if (ImGui::Button("Point")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Point;
				light.Color = { 1.f, 1.f, 1.f };
				light.Intensity = 1.f;
				light.AttenuationRadius = 50.f;
				light.Position = { 0.f, 0.f, 0.f };
				light.Radius = 1.f;

				mZDepth->AddLight(light.Type, mLightCount++);
			}
			ImGui::SameLine();
			if (ImGui::Button("Spot")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Spot;
				light.Color = { 1.f, 1.f, 1.f };
				light.Intensity = 1.f;
				light.AttenuationRadius = 50.f;
				light.Position = { 0.f, 0.f, 0.f };
				light.Direction = { 0.f, -1.f, 0.f };
				light.InnerConeAngle = 1.f;
				light.OuterConeAngle = 45.f;

				mZDepth->AddLight(light.Type, mLightCount++);
			}
			if (ImGui::Button("Tube")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Tube;
				light.Color = { 1.f, 1.f, 1.f };
				light.Intensity = 1.f;
				light.AttenuationRadius = 50.f;
				light.Position = { 0.f, 0.f, 0.f };
				light.Position1 = { 1.f, 0.f, 0.f };
				light.Radius = 1.f;

				mZDepth->AddLight(light.Type, mLightCount++);
			}
			ImGui::SameLine();
			if (ImGui::Button("Rect")) {
				auto& light = mLights[mLightCount];
				light.Type = LightType::E_Rect;
				light.Color = { 1.f, 1.f, 1.f };
				light.Intensity = 1.f;
				light.AttenuationRadius = 50.f;
				light.Center = { 0.f,  2.f,  0.f };
				light.Position = { -0.5f, 2.f, -0.5f };
				light.Position1 = { 0.5f, 2.f, -0.5f };
				light.Position2 = { 0.5f, 2.f,  0.5f };
				light.Position3 = { -0.5f, 2.f,  0.5f };
				light.Direction = { 0.f, -1.f,  0.f };
				light.Size = { 1.f, 1.f };

				mZDepth->AddLight(light.Type, mLightCount++);
			}
		}

		ImGui::End();
	}
	;// Reference Panel
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
			ImGui::SliderFloat("Roughness", &mat->Roughness, 0.f, 1.f);
			ImGui::SliderFloat("Metalic", &mat->Metailic, 0.f, 1.f);
			ImGui::SliderFloat("Specular", &mat->Specular, 0.f, 1.f);
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