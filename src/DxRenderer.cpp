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

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

#undef max
#undef min

using namespace DirectX;

namespace {
	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";
}

namespace ShaderArgs {
	namespace Bloom {
		float HighlightThreshold = 0.99f;
		int BlurCount = 3;
	}

	namespace DepthOfField {
		float FocusRange = 8.0f;
		float FocusingSpeed = 8.0f;
		float BokehRadius = 2.0f;
		float CocThreshold = 0.3f;
		float CocDiffThreshold = 0.8f;
		float HighlightPower = 4.0f;
		int SampleCount = 4;
		int BlurCount = 1;
	}

	namespace Ssr {
		float MaxDistance = 20.0f;
		float RayLength = 0.5f;
		float NoiseIntensity = 0.01f;
		int StepCount = 16;
		int BackStepCount = 8;
		int BlurCount = 3;
		float DepthThreshold = 1.0f;
	}

	namespace MotionBlur {
		float Intensity = 0.01f;
		float Limit = 0.005f;
		float DepthBias = 0.05f;
		int SampleCount = 10;
	}

	namespace TemporalAA {
		float ModulationFactor = 0.8f;
	}

	namespace Ssao {
		int BlurCount = 3;
	}

	namespace ToneMapping {
		float Exposure = 1.4f;
	}

	namespace GammaCorrection {
		float Gamma = 2.2f;
	}

	namespace DxrShadowMap {
		int BlurCount = 3;
	}

	namespace Debug {
		bool ShowCollisionBox = false;
	}

	namespace Light {
		float AmbientLight[3] = { 0.164f, 0.2f, 0.235f };

		namespace DirectionalLight {
			float Strength[3] = { 0.9568f, 0.9411f, 0.8941f };
			float Multiplier = 2.885f;
		}
	}

	namespace IrradianceMap {
		bool ShowIrradianceCubeMap = false;
		float MipLevel = 0.0f;
	}
}

DxRenderer::DxRenderer() {
	bIsCleanedUp = false;

	mMainPassCB = std::make_unique<PassConstants>();
	mShadowPassCB = std::make_unique<PassConstants>();

	mShaderManager = std::make_unique<ShaderManager>();

	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float widthSquared = 32.0f * 32.0f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);
	mLightDir = { 0.57735f, -0.57735f, 0.57735f };

	mPickedRitem = nullptr;

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

	mTLAS = std::make_unique<AccelerationStructureBuffer>();
	mDxrShadowMap = std::make_unique<DxrShadowMap::DxrShadowMapClass>();
	mDxrGeometryBuffer = std::make_unique<DxrGeometryBuffer::DxrGeometryBufferClass>();
	mBlurFilterCS = std::make_unique<BlurFilterCS::BlurFilterCSClass>();
	mRtao = std::make_unique<Rtao::RtaoClass>();

	auto blurWeights = Blur::CalcGaussWeights(2.5f);
	mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	bShadowMapCleanedUp = false;
	bSsaoMapCleanedUp = false;
	bSsrMapCleanedUp = false;

	bInitiatingTaa = true;

	mCurrDescriptorIndex = 0;

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

bool DxRenderer::Initialize(HWND hwnd, GLFWwindow* glfwWnd, UINT width, UINT height) {
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
	CheckReturn(mGBuffer->Initialize(device, width, height, shaderManager, 
		mDepthStencilBuffer->Resource(), mDepthStencilBuffer->Dsv(), DepthStencilBuffer::Format));
	CheckReturn(mShadowMap->Initialize(device, shaderManager, 2048, 2048));
	CheckReturn(mSsao->Initialize(device, cmdList, shaderManager, width, height, 1));
	CheckReturn(mBlurFilter->Initialize(device, shaderManager));
	CheckReturn(mBloom->Initialize(device, shaderManager, width, height, 4));
	CheckReturn(mSsr->Initialize(device, shaderManager, width, height, 2));
	CheckReturn(mDof->Initialize(device, shaderManager, cmdList, width, height, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mMotionBlur->Initialize(device, shaderManager, width, height, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mTaa->Initialize(device, shaderManager, width, height, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mDebugMap->Initialize(device, shaderManager, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mDxrShadowMap->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mBlurFilterCS->Initialize(device, shaderManager));
	CheckReturn(mRtao->Initialize(device, cmdList, shaderManager, width, height));
	CheckReturn(mDebugCollision->Initialize(device, shaderManager, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mGammaCorrection->Initialize(device, shaderManager, width, height, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mToneMapping->Initialize(device, shaderManager, width, height, SwapChainBuffer::BackBufferFormat));
	CheckReturn(mIrradianceMap->Initialize(device, cmdList, shaderManager));
	CheckReturn(mMipmapGenerator->Initialize(device, cmdList, shaderManager));
#ifdef _DEBUG
	WLogln(L"Finished initializing shading components \n");
#endif

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	CheckReturn(CompileShaders());
	CheckReturn(BuildGeometries());

	CheckReturn(BuildFrameResources());

	BuildDescriptors();

	CheckReturn(BuildRootSignatures());
	CheckReturn(BuildPSOs());
	CheckReturn(BuildShaderTables());

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
	FlushCommandQueue();

	mImGui->CleanUp();
	mShaderManager->CleanUp();
	LowCleanUp();

	bIsCleanedUp = true;
}

bool DxRenderer::Update(float delta) {
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
	CheckReturn(UpdateSsaoPassCB(delta));
	CheckReturn(UpdateBlurPassCB(delta));
	CheckReturn(UpdateDofCB(delta));
	CheckReturn(UpdateSsrCB(delta));
	CheckReturn(UpdateObjectCBs(delta));
	CheckReturn(UpdateMaterialCBs(delta));
	CheckReturn(UpdateConvEquirectToCubeCB(delta));

	CheckReturn(UpdateShadingObjects(delta));

	return true;
}

bool DxRenderer::Draw() {
	CheckHRESULT(mCurrFrameResource->CmdListAlloc->Reset());

	// Pre-pass and main-pass
	{
		if (bRaytracing) {
			CheckReturn(DrawGBuffer());
			CheckReturn(DrawDxrShadowMap());
			CheckReturn(DrawDxrBackBuffer());
			CheckReturn(DrawRtao());
		}
		else {
			CheckReturn(DrawGBuffer());
			CheckReturn(DrawShadowMap());
			CheckReturn(DrawSsao());
			CheckReturn(DrawBackBuffer());
		}
	}
	// Post-pass
	{
		CheckReturn(DrawSkySphere());
		CheckReturn(BuildSsr());
		CheckReturn(IntegrateSpecIrrad());
		if (bBloomEnabled) CheckReturn(ApplyBloom());
		
		CheckReturn(ResolveToneMapping());
		if (bGammaCorrectionEnabled) CheckReturn(ApplyGammaCorrection());

		if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
		if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
		if (bTaaEnabled) CheckReturn(ApplyTAA());
	}

	if (ShaderArgs::IrradianceMap::ShowIrradianceCubeMap) CheckReturn(DrawEquirectangulaToCube());
	CheckReturn(DrawDebuggingInfo());
	if (bShowImGui) CheckReturn(DrawImGui());

	CheckHRESULT(mSwapChain->Present(0, 0));
	mSwapChainBuffer->NextBackBuffer();

	mCurrFrameResource->Fence = static_cast<UINT>(IncreaseFence());
	mCommandQueue->Signal(mFence.Get(), GetCurrentFence());

	return true;
}

bool DxRenderer::OnResize(UINT width, UINT height) {
	mClientWidth = width;
	mClientHeight = height;
	
	CheckReturn(LowOnResize(width, height));
	
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (int i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i)->Resource();
	}

	CheckReturn(mBRDF->OnResize(width, height));
	CheckReturn(mSwapChainBuffer->OnResize());
	CheckReturn(mGBuffer->OnResize(width, height));
	CheckReturn(mSsao->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));
	CheckReturn(mSsr->OnResize(width, height));
	CheckReturn(mDof->OnResize(cmdList, width, height));
	CheckReturn(mMotionBlur->OnResize(width, height));
	CheckReturn(mTaa->OnResize(width, height));
	CheckReturn(mGammaCorrection->OnResize(width, height));
	CheckReturn(mToneMapping->OnResize(width, height));

	CheckReturn(mDxrShadowMap->OnResize(cmdList, width, height));
	CheckReturn(mRtao->OnResize(cmdList, width, height));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
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

void DxRenderer::SetModelVisibility(void* model, bool visible) {

}

void DxRenderer::SetModelPickable(void* model, bool pickable) {
	auto begin = mRitems.begin();
	auto end = mRitems.end();
	const auto& iter = std::find_if(begin, end, [&](std::unique_ptr<RenderItem>& p) {
		return p.get() == model;
	});
	if (iter != end) iter->get()->Pickable = pickable;
}

bool DxRenderer::SetCubeMap(const std::string& file) {

	return true;
}

bool DxRenderer::SetEquirectangularMap(const std::string& file) {	
	mIrradianceMap->SetEquirectangularMap(mCommandQueue.Get(), file);

	return true;
}

void DxRenderer::Pick(float x, float y) {
	const auto& P = mCamera->GetProj();

	// Compute picking ray in vew space.
	float vx = (2.0f * x / mClientWidth - 1.0f) / P(0, 0);
	float vy = (-2.0f * y / mClientHeight + 1.0f) / P(1, 1);

	// Ray definition in view space.
	auto origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	auto dir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	const auto V = XMLoadFloat4x4(&mCamera->GetView());
	const auto InvView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	float closestT = std::numeric_limits<float>().max();
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
		float tmin = 0.0f;
		if (ri->AABB.Intersects(originL, dirL, tmin) && tmin < closestT) {
			closestT = tmin;
			mPickedRitem = ri;
		}
	}
}

bool DxRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors =
		SwapChainBufferCount
		+ GBuffer::NumRenderTargets
		+ Ssao::NumRenderTargets
		+ TemporalAA::NumRenderTargets
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

bool DxRenderer::CompileShaders() {
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

	CheckReturn(mDxrShadowMap->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilterCS->CompileShaders(ShaderFilePath));
	CheckReturn(mRtao->CompileShaders(ShaderFilePath));

#ifdef _DEBUG
	WLogln(L"Finished compiling shaders \n");
#endif 

	return true;
}

bool DxRenderer::BuildGeometries() {
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
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

bool DxRenderer::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
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
	for (int i = 0; i < SwapChainBufferCount; ++i) {
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
	mTaa->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mGammaCorrection->BuildDescriptors(hCpu, hGpu, descSize);
	mToneMapping->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mIrradianceMap->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);

	mDxrShadowMap->BuildDescriptors(hCpu, hGpu, descSize);
	mDxrGeometryBuffer->BuildDescriptors(hCpu, hGpu, descSize);
	mRtao->BuildDescriptors(hCpu, hGpu, descSize);

	mhCpuDescForTexMaps = hCpu;
	mhGpuDescForTexMaps = hGpu;
}

bool DxRenderer::BuildRootSignatures() {
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

	CheckReturn(mDxrShadowMap->BuildRootSignatures(staticSamplers, DxrGeometryBuffer::GeometryBufferCount));
	CheckReturn(mBlurFilterCS->BuildRootSignature(staticSamplers));
	//CheckReturn(mRtao->BuildRootSignatures(staticSamplers));

#if _DEBUG
	WLogln(L"Finished building root-signatures \n");
#endif

	return true;
}

bool DxRenderer::BuildPSOs() {
#ifdef _DEBUG
	WLogln(L"Building pipeline state objects...");
#endif

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,			0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
	CheckReturn(mBRDF->BuildPso());
	CheckReturn(mGBuffer->BuildPso(inputLayoutDesc));
	CheckReturn(mShadowMap->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mSsao->BuildPso(inputLayoutDesc));
	CheckReturn(mBloom->BuildPso(inputLayoutDesc));
	CheckReturn(mBlurFilter->BuildPso());
	CheckReturn(mSsr->BuildPso());
	CheckReturn(mDof->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mMotionBlur->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mTaa->BuildPso());
	CheckReturn(mDebugMap->BuildPso());
	CheckReturn(mDebugCollision->BuildPso());
	CheckReturn(mGammaCorrection->BuildPso());
	CheckReturn(mToneMapping->BuildPso());
	CheckReturn(mIrradianceMap->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mMipmapGenerator->BuildPso());
	
	CheckReturn(mDxrShadowMap->BuildPso());
	CheckReturn(mBlurFilterCS->BuildPso());
	//CheckReturn(mRtao->BuildPSO());

#ifdef _DEBUG
	WLogln(L"Finished building pipeline state objects \n");
#endif

	return true;
}

bool DxRenderer::BuildShaderTables() {
	CheckReturn(mDxrShadowMap->BuildShaderTables());
	//CheckReturn(mRtao->BuildShaderTables());

	return true;
}

void DxRenderer::BuildRenderItems() {
	{
		auto skyRitem = std::make_unique<RenderItem>();
		skyRitem->ObjCBIndex = static_cast<int>(mRitems.size());
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
		boxRitem->ObjCBIndex = static_cast<int>(mRitems.size());
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

bool DxRenderer::AddGeometry(const std::string& file) {
	Mesh mesh;
	Material mat;
	CheckReturn(MeshImporter::LoadObj(file, mesh, mat));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = file;

	const auto& vertices = mesh.Vertices;
	const auto& indices = mesh.Indices;

	const size_t vertexSize = sizeof(Vertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	mGeometries[file] = std::move(geo);

	if (mMaterials.count(file) == 0) CheckReturn(AddMaterial(file, mat));

	return true;
}

bool DxRenderer::AddMaterial(const std::string& file, const Material& material) {
	UINT index = AddTexture(file, material);
	if (index == -1) ReturnFalse("Failed to create texture");

	auto matData = std::make_unique<MaterialData>();
	matData->MatCBIndex = static_cast<int>(mMaterials.size());
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
	ritem->ObjCBIndex = static_cast<int>(mRitems.size());
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

bool DxRenderer::AddBLAS(ID3D12GraphicsCommandList4*const cmdList, MeshGeometry*const geo) {
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(Vertex));
	geometryDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
	geometryDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.Transform3x4 = 0;
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the BLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geometryDesc;
	inputs.NumDescs = 1;
	inputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
	
	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
	
	std::unique_ptr<AccelerationStructureBuffer> blas = std::make_unique<AccelerationStructureBuffer>();
	
	// Create the BLAS scratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CheckReturn(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, blas->Scratch.GetAddressOf()));
	
	// Create the BLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckReturn(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, blas->Result.GetAddressOf()));
	
	// Describe and build the bottom level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = blas->Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = blas->Result->GetGPUVirtualAddress();
	
	cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	
	D3D12Util::UavBarrier(cmdList, blas->Result.Get());
	mBLASs[geo->Name] = std::move(blas);

	return true;
}

bool DxRenderer::BuildTLASs(ID3D12GraphicsCommandList4*const cmdList) {
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

	const auto opaques = mRitemRefs[RenderType::E_Opaque];

	for (const auto ri : opaques) {
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.InstanceID = instanceDescs.size();
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.InstanceMask = 0xFF;
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 4; ++c) {
				instanceDesc.Transform[r][c] = ri->World.m[c][r];
			}
		}
		instanceDesc.AccelerationStructure = mBLASs[ri->Geometry->Name]->Result->GetGPUVirtualAddress();
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
		instanceDescs.push_back(instanceDesc);
	}

	// Create the TLAS instance buffer
	D3D12BufferCreateInfo instanceBufferInfo;
	instanceBufferInfo.Size = instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
	instanceBufferInfo.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	instanceBufferInfo.Flags = D3D12_RESOURCE_FLAG_NONE;
	instanceBufferInfo.State = D3D12_RESOURCE_STATE_GENERIC_READ;
	CheckReturn(D3D12Util::CreateBuffer(md3dDevice.Get(), instanceBufferInfo, mTLAS->InstanceDesc.GetAddressOf()));
	
	// Copy the instance data to the buffer
	void* pData;
	CheckHRESULT(mTLAS->InstanceDesc->Map(0, nullptr, &pData));
	std::memcpy(pData, instanceDescs.data(), instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
	mTLAS->InstanceDesc->Unmap(0, nullptr);
	
	// Get the size requirements for the TLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = mTLAS->InstanceDesc->GetGPUVirtualAddress();
	inputs.NumDescs = static_cast<UINT>(instanceDescs.size());
	inputs.Flags = buildFlags;
	
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
	
	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
	
	// Set TLAS size
	mTLAS->ResultDataMaxSizeInBytes = prebuildInfo.ResultDataMaxSizeInBytes;
	
	// Create TLAS sratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);	
	CheckReturn(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS->Scratch.GetAddressOf()));
	
	// Create the TLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;	
	CheckReturn(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS->Result.GetAddressOf()));
	
	// Describe and build the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = mTLAS->Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = mTLAS->Result->GetGPUVirtualAddress();
	
	cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	
	// Wait for the TLAS build to complete
	D3D12Util::UavBarrier(cmdList, mTLAS->Result.Get());

	return true;
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	CheckReturn(FlushCommandQueue());

	mTextures[file] = std::move(texMap);

	return mCurrDescriptorIndex++;
}

bool DxRenderer::UpdateShadingObjects(float delta) {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mIrradianceMap->Update(
		mCommandQueue.Get(),
		mCbvSrvUavHeap.Get(),
		cmdList, 
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mCurrFrameResource->ConvEquirectToCubeCB.Resource()->GetGPUVirtualAddress(), 
		mMipmapGenerator.get(),
		mIrradianceCubeMap
	);

	CheckReturn(BuildTLASs(cmdList));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	return true;
}

bool DxRenderer::UpdateShadowPassCB(float delta) {
	XMVECTOR lightDir = XMLoadFloat3(&mLightDir);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = UnitVectors::UpVector;
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

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

bool DxRenderer::UpdateMainPassCB(float delta) {
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

bool DxRenderer::UpdateSsaoPassCB(float delta) {
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

	auto& currSsaoCB = mCurrFrameResource->SsaoCB;
	currSsaoCB.CopyData(0, ssaoCB);

	return true;
}

bool DxRenderer::UpdateBlurPassCB(float delta) {
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

bool DxRenderer::UpdateDofCB(float delta) {
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

bool DxRenderer::UpdateSsrCB(float delta) {
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

bool DxRenderer::UpdateObjectCBs(float delta) {
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

bool DxRenderer::UpdateMaterialCBs(float delta) {
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

bool DxRenderer::UpdateConvEquirectToCubeCB(float delta) {
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

bool DxRenderer::DrawShadowMap() {
	const auto cmdList = mCommandList.Get();

	if (!bShadowEnabled) {
		if (!bShadowMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			const auto shadow = mShadowMap->Resource();

			shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			cmdList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);

			CheckHRESULT(cmdList->Close());
			ID3D12CommandList* cmdsLists[] = { cmdList };
			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawGBuffer() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);
	
	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	const auto matCBAddress = mCurrFrameResource->MaterialCB.Resource()->GetGPUVirtualAddress();

	mGBuffer->Run(
		cmdList,
		passCBAddress,
		objCBAddress,
		matCBAddress,
		mhGpuDescForTexMaps,
		mRitemRefs[RenderType::E_Opaque]
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawSsao() {
	const auto cmdList = mCommandList.Get();

	if (!bSsaoEnabled) {
		if (!bSsaoMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			const auto aoCoeffMap = mSsao->AOCoefficientMapResource(0);
			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cmdList->ClearRenderTargetView(mSsao->AOCoefficientMapRtv(0), Ssao::AOCoefficientMapClearValues, 0, nullptr);

			aoCoeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CheckHRESULT(cmdList->Close());
			ID3D12CommandList* cmdsLists[] = { cmdList };
			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawBackBuffer() {
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
		mShadowMap->Srv(),
		mSsao->AOCoefficientMapSrv(0),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		BRDF::Render::E_Raster
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::IntegrateSpecIrrad() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

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
		mSsao->AOCoefficientMapSrv(0),
		mIrradianceMap->PrefilteredEnvironmentCubeMapSrv(),
		mIrradianceMap->IntegratedBrdfMapSrv(),
		mSsr->SsrMapSrv(0)
	);	

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawSkySphere() {
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawEquirectangulaToCube() {
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyTAA() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mTaa->Run(
		cmdList,
		mSwapChainBuffer->CurrentBackBuffer(),
		mSwapChainBuffer->CurrentBackBufferSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::TemporalAA::ModulationFactor
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::BuildSsr() {
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

		backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		mSsr->Build(
			cmdList,
			ssrCBAddress,
			backBufferSrv,
			mGBuffer->NormalMapSrv(),
			mGBuffer->DepthMapSrv(),
			mGBuffer->RMSMapSrv()
		);

		backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

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
			BlurFilter::FilterType::R32G32B32A32,
			ShaderArgs::Ssr::BlurCount
		);

		CheckHRESULT(cmdList->Close());
		ID3D12CommandList* cmdsLists[] = { cmdList };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	}
	else {
		if (!bSsrMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			ssrMap0->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cmdList->ClearRenderTargetView(mSsr->SsrMapRtv(0), Ssr::ClearValues, 0, nullptr);

			ssrMap0->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CheckHRESULT(cmdList->Close());
			ID3D12CommandList* cmdsLists[] = { cmdList };
			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

			bSsrMapCleanedUp = true;
		};
	}

	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyBloom() {
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
		BlurFilter::FilterType::R32G32B32A32,
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyDepthOfField() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	auto backBufferSrv = mSwapChainBuffer->CurrentBackBufferSrv();

	const auto dofCBAddress = mCurrFrameResource->DofCB.Resource()->GetGPUVirtualAddress();
	mDof->CalcFocalDist(
		cmdList,
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
		backBufferSrv,
		ShaderArgs::DepthOfField::BokehRadius,
		ShaderArgs::DepthOfField::CocThreshold,
		ShaderArgs::DepthOfField::CocDiffThreshold,
		ShaderArgs::DepthOfField::HighlightPower,
		ShaderArgs::DepthOfField::SampleCount
	);

	const auto blurCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mDof->BlurDof(
		cmdList,
		blurCBAddress,
		ShaderArgs::DepthOfField::BlurCount
	);

	const auto dofMap = mDof->DofMapResource();

	dofMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(backBuffer->Resource(), dofMap->Resource());

	dofMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyMotionBlur() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	auto backBufferSrv = mSwapChainBuffer->CurrentBackBufferSrv();
	
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mMotionBlur->Run(
		cmdList,
		backBufferSrv,
		mGBuffer->DepthMapSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::MotionBlur::Intensity,
		ShaderArgs::MotionBlur::Limit,
		ShaderArgs::MotionBlur::DepthBias,
		ShaderArgs::MotionBlur::SampleCount
	);

	const auto motionVector = mMotionBlur->MotionVectorMapResource();

	motionVector->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(backBuffer->Resource(), motionVector->Resource());

	motionVector->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ResolveToneMapping() {
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyGammaCorrection() {
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawDebuggingInfo() {
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
		mSwapChainBuffer->CurrentBackBufferRtv(),
		DepthStencilView()
	);

	if (ShaderArgs::Debug::ShowCollisionBox) {
		mDebugCollision->Run(
			cmdList,
			mScreenViewport,
			mScissorRect,
			mSwapChainBuffer->CurrentBackBufferRtv(),
			mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
			mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress(),
			mRitemRefs[RenderType::E_Opaque]
		);
	}

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawImGui() {
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

	static const auto BuildDebugMaps = [&](bool& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, Debug::SampleMask::Type type) {
		if (mode) { if (!mDebugMap->AddDebugMap(handle, type)) mode = false; }
		else { mDebugMap->RemoveDebugMap(handle); }
	};

	{
		ImGui::Begin("Main Panel");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Debug")) {
			if (ImGui::TreeNode("Map Info")) {
				if (ImGui::Checkbox("Albedo", &mDebugMapStates[DebugMapLayout::E_Albedo])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Albedo],
						mGBuffer->AlbedoMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Normal", &mDebugMapStates[DebugMapLayout::E_Normal])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Normal],
						mGBuffer->NormalMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Depth", &mDebugMapStates[DebugMapLayout::E_Depth])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Depth],
						mGBuffer->DepthMapSrv(),
						Debug::SampleMask::RRR);
				}
				if (ImGui::Checkbox("RoughnessMetalicSpecular", &mDebugMapStates[DebugMapLayout::E_RMS])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_RMS],
						mGBuffer->RMSMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Velocity", &mDebugMapStates[DebugMapLayout::E_Velocity])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Velocity],
						mGBuffer->VelocityMapSrv(),
						Debug::SampleMask::RG);
				}
				if (ImGui::Checkbox("Shadow", &mDebugMapStates[DebugMapLayout::E_Shadow])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Shadow],
						mShadowMap->Srv(),
						Debug::SampleMask::RRR);
				}
				if (ImGui::Checkbox("SSAO", &mDebugMapStates[DebugMapLayout::E_SSAO])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_SSAO],
						mSsao->AOCoefficientMapSrv(0),
						Debug::SampleMask::RRR);
				}
				if (ImGui::Checkbox("Bloom", &mDebugMapStates[DebugMapLayout::E_Bloom])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Bloom],
						mBloom->BloomMapSrv(0),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("SSR", &mDebugMapStates[DebugMapLayout::E_SSR])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_SSR],
						mSsr->SsrMapSrv(0),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Equirectangular Map", &mDebugMapStates[DebugMapLayout::E_Equirectangular])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_Equirectangular],
						mIrradianceMap->EquirectangularMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Temporary Equirectangular Map", &mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_TemporaryEquirectangular],
						mIrradianceMap->TemporaryEquirectangularMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Diffuse Irradiance Equirectangular Map", &mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_DiffuseIrradianceEquirect],
						mIrradianceMap->DiffuseIrradianceEquirectMapSrv(),
						Debug::SampleMask::RGB);
				}
				if (ImGui::Checkbox("DXR Shadow", &mDebugMapStates[DebugMapLayout::E_DxrShadow])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::E_DxrShadow],
						mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow0),
						Debug::SampleMask::RRR);
				}

				ImGui::TreePop();
			}
			ImGui::Checkbox("Show Collision Box", &ShaderArgs::Debug::ShowCollisionBox);
		}
		if (ImGui::CollapsingHeader("Effects")) {
			ImGui::Checkbox("Shadow", &bShadowEnabled);
			ImGui::Checkbox("SSAO", &bSsaoEnabled);
			ImGui::Checkbox("TAA", &bTaaEnabled);
			ImGui::Checkbox("Motion Blur", &bMotionBlurEnabled);
			ImGui::Checkbox("Depth of Field", &bDepthOfFieldEnabled);
			ImGui::Checkbox("Bloom", &bBloomEnabled);
			ImGui::Checkbox("SSR", &bSsrEnabled);
			ImGui::Checkbox("Gamma Correction", &bGammaCorrectionEnabled);
			ImGui::Checkbox("Tone Mapping", &bToneMappingEnabled);
		}
		if (ImGui::CollapsingHeader("Lights")) {
			ImGui::ColorPicker3("Amblient Light", ShaderArgs::Light::AmbientLight);

			if (ImGui::TreeNode("Directional Lights")) {
				ImGui::ColorPicker3("Strength", ShaderArgs::Light::DirectionalLight::Strength);
				ImGui::SliderFloat("Multiplier", &ShaderArgs::Light::DirectionalLight::Multiplier, 0, 100.0f);

				ImGui::TreePop();
			}
		}
		if (ImGui::CollapsingHeader("BRDF")) {
			ImGui::RadioButton("Blinn-Phong", reinterpret_cast<int*>(&mBRDF->ModelType), BRDF::Model::E_BlinnPhong); ImGui::SameLine();
			ImGui::RadioButton("Cook-Torrance", reinterpret_cast<int*>(&mBRDF->ModelType), BRDF::Model::E_CookTorrance);
			
		}
		if (ImGui::CollapsingHeader("Environment")) {
			ImGui::Checkbox("Show Irradiance CubeMap", &ShaderArgs::IrradianceMap::ShowIrradianceCubeMap);
			if (ShaderArgs::IrradianceMap::ShowIrradianceCubeMap) {
				ImGui::RadioButton(
					"Environment CubeMap", 
					reinterpret_cast<int*>(&mIrradianceMap->DrawCubeType), 
					IrradianceMap::DrawCube::E_EnvironmentCube);
				ImGui::RadioButton(
					"Equirectangular Map", 
					reinterpret_cast<int*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_Equirectangular);
				ImGui::RadioButton(
					"Diffuse Irradiance CubeMap",
					reinterpret_cast<int*>(&mIrradianceMap->DrawCubeType),
					IrradianceMap::DrawCube::E_DiffuseIrradianceCube);
				ImGui::RadioButton(
					"Prefiltered Irradiance CubeMap",
					reinterpret_cast<int*>(&mIrradianceMap->DrawCubeType),
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
				ImGui::SliderInt("Number of Blurs", &ShaderArgs::Ssao::BlurCount, 0, 8);

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
		}

		ImGui::End();
	}
	{
		ImGui::Begin("Reference Panel");
		ImGui::NewLine();

		if (mPickedRitem != nullptr) {
			auto mat = mPickedRitem->Material;
			ImGui::Text(mPickedRitem->Geometry->Name.c_str());

			float albedo[4] = { mat->Albedo.x, mat->Albedo.y, mat->Albedo.z, mat->Albedo.w };
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
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawDxrShadowMap() {
	const auto cmdList = mCommandList.Get();	
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();	
	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		
	mDxrShadowMap->Run(
		cmdList,
		mTLAS->Result->GetGPUVirtualAddress(),
		mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress(),
		mDxrGeometryBuffer->VerticesSrv(),
		mDxrGeometryBuffer->IndicesSrv(),
		mGBuffer->DepthMapSrv()
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
		mDxrShadowMap->Width(), mDxrShadowMap->Height(),
		ShaderArgs::DxrShadowMap::BlurCount
	);
	
	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawDxrBackBuffer() {
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
		mDxrShadowMap->Descriptor(DxrShadowMap::Descriptors::ES_Shadow0),
		mSsao->AOCoefficientMapSrv(0),
		mIrradianceMap->DiffuseIrradianceCubeMapSrv(),
		BRDF::Render::E_Raytrace
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawRtao() {


	return true;
}