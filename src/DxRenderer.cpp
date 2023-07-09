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
#include "BackBuffer.h"
#include "Samplers.h"
#include "BlurFilter.h"
#include "Debug.h"
#include "SkyCube.h"
#include "ImGuiManager.h"
#include "DxrShadowMap.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

#undef max
#undef min

using namespace DirectX;

namespace {
	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";

	const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
	const DXGI_FORMAT SpecularMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT VelocityMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;

	const UINT GeometryBufferCount = 64;
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

	mImGui = std::make_unique<ImGuiManager>();

	mBackBuffer = std::make_unique<BackBuffer::BackBufferClass>();
	mGBuffer = std::make_unique<GBuffer::GBufferClass>();
	mShadowMap = std::make_unique<ShadowMap::ShadowMapClass>();
	mSsao = std::make_unique<Ssao::SsaoClass>();
	mBlurFilter = std::make_unique<BlurFilter::BlurFilterClass>();
	mBloom = std::make_unique<Bloom::BloomClass>();
	mSsr = std::make_unique<Ssr::SsrClass>();
	mDof = std::make_unique<DepthOfField::DepthOfFieldClass>();
	mMotionBlur = std::make_unique<MotionBlur::MotionBlurClass>();
	mTaa = std::make_unique<TemporalAA::TemporalAAClass>();
	mDebug = std::make_unique<Debug::DebugClass>();
	mSkyCube = std::make_unique<SkyCube::SkyCubeClass>();

	mTLAS = std::make_unique<AccelerationStructureBuffer>();
	mDxrShadowMap = std::make_unique<DxrShadowMap::DxrShadowMapClass>();

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
	CheckReturn(mImGui->Initialize(mhMainWnd, device, mCbvSrvUavHeap.Get(), SwapChainBufferCount, BackBufferFormat));
	
#ifdef _DEBUG
	WLogln(L"Initializing shading components...");
#endif
	CheckReturn(mBackBuffer->Initialize(device, shaderManager, width, height, BackBufferFormat, SwapChainBufferCount));
	CheckReturn(mGBuffer->Initialize(device, width, height, shaderManager, 
		mDepthStencilBuffer->Resource(), mDepthStencilBuffer->Dsv(), DepthStencilBuffer::Format));
	CheckReturn(mShadowMap->Initialize(device, shaderManager, 2048, 2048));
	CheckReturn(mSsao->Initialize(device, cmdList, shaderManager, width, height, 1));
	CheckReturn(mBlurFilter->Initialize(device, shaderManager));
	CheckReturn(mBloom->Initialize(device, shaderManager, width, height, 4, BackBufferFormat));
	CheckReturn(mSsr->Initialize(device, shaderManager, width, height, 2, BackBufferFormat));
	CheckReturn(mDof->Initialize(device, shaderManager, cmdList, width, height, BackBufferFormat));
	CheckReturn(mMotionBlur->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mTaa->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mDebug->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mSkyCube->Initialize(device, cmdList, shaderManager, width, height, BackBufferFormat));

	CheckReturn(mDxrShadowMap->Initialize(device, cmdList, shaderManager, width, height));
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

	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckReturn(BuildTLASs(cmdList));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	return true;
}

bool DxRenderer::Draw() {
	CheckHRESULT(mCurrFrameResource->CmdListAlloc->Reset());

	// Pre-pass and main-pass
	if (bRaytracing) {
		CheckReturn(DrawGBuffer());

		CheckReturn(DrawBackBuffer());
	}
	else {
		CheckReturn(DrawGBuffer());
		CheckReturn(DrawShadowMap());
		CheckReturn(DrawSsao());

		CheckReturn(DrawBackBuffer());
	}

	// Post-pass
	if (bRaytracing) {
		CheckReturn(DrawSkyCube());
		CheckReturn(ApplySsr());
		if (bBloomEnabled) CheckReturn(ApplyBloom());
		if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
		if (bTaaEnabled) CheckReturn(ApplyTAA());
		if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
	}
	else {
		CheckReturn(DrawSkyCube());
		CheckReturn(ApplySsr());
		if (bBloomEnabled) CheckReturn(ApplyBloom());
		if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
		if (bTaaEnabled) CheckReturn(ApplyTAA());
		if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
	}

	if (bDebuggingEnabled) CheckReturn(DrawDebuggingInfo());
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
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i);
	}

	CheckReturn(mBackBuffer->OnResize(backBuffers.data(), width, height));
	CheckReturn(mGBuffer->OnResize(width, height));
	CheckReturn(mSsao->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));
	CheckReturn(mSsr->OnResize(width, height));
	CheckReturn(mDof->OnResize(cmdList, width, height));
	CheckReturn(mMotionBlur->OnResize(width, height));
	CheckReturn(mTaa->OnResize(width, height));
	CheckReturn(mDebug->OnResize(width, height));

	CheckReturn(mDxrShadowMap->OnResize(cmdList, width, height));

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

bool DxRenderer::SetCubeMap(const std::string& file) {
	ID3D12GraphicsCommandList* cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mSkyCube->SetCubeMap(mCommandQueue.Get(), file);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	return true;
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
		+ Ssr::NumRenderTargets;
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

	CheckReturn(mBackBuffer->CompileShaders(ShaderFilePath));
	CheckReturn(mGBuffer->CompileShaders(ShaderFilePath));
	CheckReturn(mShadowMap->CompileShaders(ShaderFilePath));
	CheckReturn(mSsao->CompileShaders(ShaderFilePath));
	CheckReturn(mBlurFilter->CompileShaders(ShaderFilePath));
	CheckReturn(mBloom->CompileShaders(ShaderFilePath));
	CheckReturn(mSsr->CompileShaders(ShaderFilePath));
	CheckReturn(mDof->CompileShaders(ShaderFilePath));
	CheckReturn(mMotionBlur->CompileShaders(ShaderFilePath));
	CheckReturn(mTaa->CompileShaders(ShaderFilePath));
	CheckReturn(mDebug->CompileShaders(ShaderFilePath));
	CheckReturn(mSkyCube->CompileShaders(ShaderFilePath));

	CheckReturn(mDxrShadowMap->CompileShaders(ShaderFilePath));

#ifdef _DEBUG
	WLogln(L"Finished compiling shaders \n");
#endif 

	return true;
}

bool DxRenderer::BuildGeometries() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1000.0f, 8, 8);

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = static_cast<UINT>(sphere.Indices32.size());
	sphereSubmesh.StartIndexLocation = 0;
	sphereSubmesh.BaseVertexLocation = 0;

	std::vector<Vertex> vertices(sphere.Vertices.size());

	for (size_t i = 0, end = sphere.Vertices.size(); i < end; ++i) {
		vertices[i].Position = sphere.Vertices[i].Position;
		vertices[i].Normal = sphere.Vertices[i].Normal;
		vertices[i].TexCoord = sphere.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices(sphere.GetIndices16().begin(), sphere.GetIndices16().end());

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
		backBuffers[i] = mSwapChainBuffer->BackBuffer(i);
	}

	mImGui->BuildDescriptors(hCpu, hGpu, descSize);
	mBackBuffer->BuildDescriptors(backBuffers.data(), hCpu, hGpu, descSize);
	mGBuffer->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mShadowMap->BuildDescriptors(hCpu, hGpu, hCpuDsv, descSize, dsvDescSize);
	mSsao->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mBloom->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSsr->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mDof->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mMotionBlur->BuildDescriptors(hCpuRtv, rtvDescSize);
	mTaa->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSkyCube->BuildDescriptors(hCpu, hGpu, descSize);

	mhCpuDescForTexMaps = hCpu;
	mhGpuDescForTexMaps = hGpu;
}

bool DxRenderer::BuildRootSignatures() {
#if _DEBUG
	WLogln(L"Building root-signatures...");
#endif

	auto staticSamplers = Samplers::GetStaticSamplers();
	CheckReturn(mBackBuffer->BuildRootSignature(staticSamplers));
	CheckReturn(mGBuffer->BuildRootSignature(staticSamplers));
	CheckReturn(mShadowMap->BuildRootSignature(staticSamplers));
	CheckReturn(mSsao->BuildRootSignature(staticSamplers));
	CheckReturn(mBlurFilter->BuildRootSignature(staticSamplers));
	CheckReturn(mBloom->BuildRootSignature(staticSamplers));
	CheckReturn(mSsr->BuildRootSignature(staticSamplers));
	CheckReturn(mDof->BuildRootSignature(staticSamplers));
	CheckReturn(mMotionBlur->BuildRootSignature(staticSamplers));
	CheckReturn(mTaa->BuildRootSignature(staticSamplers));
	CheckReturn(mDebug->BuildRootSignature(staticSamplers));
	CheckReturn(mSkyCube->BuildRootSignature(staticSamplers));

	CheckReturn(mDxrShadowMap->BuildRootSignatures(staticSamplers, GeometryBufferCount));

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
	CheckReturn(mBackBuffer->BuildPso());
	CheckReturn(mGBuffer->BuildPso(inputLayoutDesc));
	CheckReturn(mShadowMap->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mSsao->BuildPso(inputLayoutDesc));
	CheckReturn(mBloom->BuildPso(inputLayoutDesc));
	CheckReturn(mBlurFilter->BuildPso());
	CheckReturn(mSsr->BuildPso());
	CheckReturn(mDof->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mMotionBlur->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	CheckReturn(mTaa->BuildPso());
	CheckReturn(mDebug->BuildPso());
	CheckReturn(mSkyCube->BuildPso(inputLayoutDesc, DepthStencilBuffer::Format));
	
	CheckReturn(mDxrShadowMap->BuildPso());

#ifdef _DEBUG
	WLogln(L"Finished building pipeline state objects \n");
#endif

	return true;
}

bool DxRenderer::BuildShaderTables() {
	CheckReturn(mDxrShadowMap->BuildShaderTables());

	return true;
}

void DxRenderer::BuildRenderItems() {
	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->ObjCBIndex = static_cast<int>(mRitems.size());
	skyRitem->Geometry = mGeometries["basic"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geometry->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geometry->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geometry->DrawArgs["sphere"].BaseVertexLocation;
	skyRitem->World = MathHelper::Identity4x4();
	mRitemRefs[RenderType::ESky].push_back(skyRitem.get());
	mRitems.push_back(std::move(skyRitem));
}

bool DxRenderer::AddGeometry(const std::string& file) {
	Mesh mesh;
	Material mat;
	CheckReturn(MeshImporter::LoadObj(file, mesh, mat));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = file;
	BoundingBox bound;

	const auto& vertices = mesh.Vertices;
	const auto& indices = mesh.Indices;

	const size_t vertexSize = sizeof(Vertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	for (size_t i = 0; i < vertices.size(); ++i) {
		XMVECTOR P = XMLoadFloat3(&vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
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
	matData->DiffuseAlbedo = material.DiffuseAlbedo;
	matData->FresnelR0 = material.FresnelR0;
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

	const auto opaques = mRitemRefs[RenderType::EOpaque];

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
	mMainPassCB->AmbientLight = { 0.3f, 0.3f, 0.42f, 1.0f };
	mMainPassCB->Lights[0].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB->Lights[0].Direction = mLightDir;
	mMainPassCB->Lights[0].FalloffStart = 1.0f;
	mMainPassCB->Lights[0].FalloffEnd = 10.0f;
	mMainPassCB->Lights[0].SpotPower = 64.0f;

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
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConsts;
			matConsts.DiffuseSrvIndex = mat->DiffuseSrvHeapIndex;
			matConsts.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConsts.FresnelR0 = mat->FresnelR0;
			matConsts.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConsts.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB.CopyData(mat->MatCBIndex, matConsts);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

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
		mRitemRefs[RenderType::EOpaque]
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
		mRitemRefs[RenderType::EOpaque]
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
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto pCurrBackBuffer = mSwapChainBuffer->CurrentBackBuffer();
	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto pCurrBackBufferView = mSwapChainBuffer->CurrentBackBufferView();
	cmdList->ClearRenderTargetView(pCurrBackBufferView, Colors::AliceBlue, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &pCurrBackBufferView, true, nullptr);

	auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	mBackBuffer->Run(
		cmdList,
		passCBAddress,
		mGBuffer->ColorMapSrv(),
		mGBuffer->AlbedoMapSrv(),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->SpecularMapSrv(),
		mShadowMap->Srv(),
		mSsao->AOCoefficientMapSrv(0)
	);


	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawSkyCube() {
	const auto cmdList = mCommandList.Get();
	CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	const auto objCBAddress = mCurrFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress();
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	
	mSkyCube->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mSwapChainBuffer->CurrentBackBufferView(),
		DepthStencilView(),
		passCBAddress,
		objCBAddress,
		objCBByteSize,
		mRitemRefs[RenderType::ESky]
	);
	
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

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

	auto currentBufferIndex = mSwapChainBuffer->CurrentBackBufferIndex();
	mTaa->Run(
		cmdList,
		mSwapChainBuffer->BackBuffer(currentBufferIndex),
		mBackBuffer->BackBufferSrv(currentBufferIndex),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::TemporalAA::ModulationFactor
	);

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplySsr() {
	const auto cmdList = mCommandList.Get();

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	const auto ssrMap0 = mSsr->SsrMapResource(0);
	const auto ssrMap1 = mSsr->SsrMapResource(1);

	auto ssrCBAddress = mCurrFrameResource->SsrCB.Resource()->GetGPUVirtualAddress();

	auto currentBufferIndex = mSwapChainBuffer->CurrentBackBufferIndex();

	if (bSsrEnabled) {
		bSsrMapCleanedUp = false;

		CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

		ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
		cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(
					backBuffer,
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			};
			cmdList->ResourceBarrier(_countof(barriers), barriers);
		}

		mSsr->BuildSsr(
			cmdList,
			ssrCBAddress,
			mBackBuffer->BackBufferSrv(currentBufferIndex),
			mGBuffer->NormalMapSrv(),
			mGBuffer->DepthMapSrv(),
			mGBuffer->SpecularMapSrv()
		);

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(
					backBuffer,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_PRESENT
				)
			};
			cmdList->ResourceBarrier(_countof(barriers), barriers);
		}

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
			BlurFilter::FilterType::R8G8B8A8,
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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mSsr->ApplySsr(
		cmdList,
		ssrCBAddress,
		mSkyCube->CubeMapSrv(),
		mBackBuffer->BackBufferSrv(currentBufferIndex),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->SpecularMapSrv()
	);

	const auto resultMap = mSsr->ResultMapResource();
	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_COPY_DEST
			),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	cmdList->CopyResource(backBuffer, resultMap->Resource());

	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT
			),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

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

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	const auto bloomMap0 = mBloom->BloomMapResource(0);
	const auto bloomMap1 = mBloom->BloomMapResource(1);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		};
		cmdList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}

	const auto backBufferSrv = mBackBuffer->BackBufferSrv(mSwapChainBuffer->CurrentBackBufferIndex());
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
		BlurFilter::FilterType::R8G8B8A8,
		ShaderArgs::Bloom::BlurCount
	);
	
	mBloom->Bloom(
		cmdList,
		backBufferSrv
	);

	auto resultMap = mBloom->ResultMapResource();
	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST
			)
		};
		mCommandList->ResourceBarrier(_countof(barriers), barriers);
	}

	mCommandList->CopyResource(backBuffer, resultMap->Resource());

	resultMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT
			)
		};
		mCommandList->ResourceBarrier(_countof(barriers), barriers);
	}

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
		mBackBuffer->BackBufferSrv(mSwapChainBuffer->CurrentBackBufferIndex()),
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

	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	const auto dofMap = mDof->DofMapResource();

	dofMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	cmdList->CopyResource(backBuffer, dofMap->Resource());

	dofMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

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
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	mMotionBlur->Run(
		cmdList,
		mBackBuffer->BackBufferSrv(mSwapChainBuffer->CurrentBackBufferIndex()),
		mGBuffer->DepthMapSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::MotionBlur::Intensity,
		ShaderArgs::MotionBlur::Limit,
		ShaderArgs::MotionBlur::DepthBias,
		ShaderArgs::MotionBlur::SampleCount
	);

	const auto motionVector = mMotionBlur->MotionVectorMapResource();

	motionVector->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	cmdList->CopyResource(backBuffer, motionVector->Resource());

	motionVector->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

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
	
	const auto backBuffer = mSwapChainBuffer->CurrentBackBuffer();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	mDebug->Run(
		cmdList,
		mSwapChainBuffer->CurrentBackBufferView(),
		DepthStencilView()
	);

	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
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

	const auto pCurrBackBuffer = mSwapChainBuffer->CurrentBackBuffer();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pCurrBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	auto pCurrBackBufferView = mSwapChainBuffer->CurrentBackBufferView();
	cmdList->OMSetRenderTargets(1, &pCurrBackBufferView, true, nullptr);

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static const auto BuildDebugMaps = [&](bool& mode, D3D12_GPU_DESCRIPTOR_HANDLE handle, DebugShaderParams::SampleMask::Type type) {
		if (mode) { if (!mDebug->AddDebugMap(handle, type)) mode = false; }
		else { mDebug->RemoveDebugMap(handle); }
	};

	{
		ImGui::Begin("Main Panel");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Debug")) {
			ImGui::Checkbox("Debugging windows", &bDebuggingEnabled);
			
			if (bDebuggingEnabled) {
				ImGui::NewLine();

				if (ImGui::Checkbox("Color", &mDebugMapStates[DebugMapLayout::EColor])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EColor],
						mGBuffer->ColorMapSrv(),
						DebugShaderParams::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Albedo", &mDebugMapStates[DebugMapLayout::EAlbedo])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EAlbedo],
						mGBuffer->AlbedoMapSrv(),
						DebugShaderParams::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Normal", &mDebugMapStates[DebugMapLayout::ENormal])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::ENormal],
						mGBuffer->NormalMapSrv(),
						DebugShaderParams::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Depth", &mDebugMapStates[DebugMapLayout::EDepth])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EDepth],
						mGBuffer->DepthMapSrv(),
						DebugShaderParams::SampleMask::RRR);
				}
				if (ImGui::Checkbox("Specular", &mDebugMapStates[DebugMapLayout::ESpecular])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::ESpecular],
						mGBuffer->SpecularMapSrv(),
						DebugShaderParams::SampleMask::RGB);
				}
				if (ImGui::Checkbox("Velocity", &mDebugMapStates[DebugMapLayout::EVelocity])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EVelocity],
						mGBuffer->VelocityMapSrv(),
						DebugShaderParams::SampleMask::RG);
				}
				if (ImGui::Checkbox("Shadow", &mDebugMapStates[DebugMapLayout::EShadow])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EShadow],
						mShadowMap->Srv(),
						DebugShaderParams::SampleMask::RRR);
				}
				if (ImGui::Checkbox("SSAO", &mDebugMapStates[DebugMapLayout::ESsao])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::ESsao],
						mSsao->AOCoefficientMapSrv(0),
						DebugShaderParams::SampleMask::RRR);
				}
				if (ImGui::Checkbox("Bloom", &mDebugMapStates[DebugMapLayout::EBloom])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::EBloom],
						mBloom->BloomMapSrv(0),
						DebugShaderParams::SampleMask::RGB);
				}
				if (ImGui::Checkbox("SSR", &mDebugMapStates[DebugMapLayout::ESsr])) {
					BuildDebugMaps(
						mDebugMapStates[DebugMapLayout::ESsr],
						mSsr->SsrMapSrv(0),
						DebugShaderParams::SampleMask::RGB);
				}
			}
		}
		if (ImGui::CollapsingHeader("Effects")) {
			ImGui::Checkbox("Shadow", &bShadowEnabled);
			ImGui::Checkbox("SSAO", &bSsaoEnabled);
			ImGui::Checkbox("TAA", &bTaaEnabled);
			ImGui::Checkbox("Motion Blur", &bMotionBlurEnabled);
			ImGui::Checkbox("Depth of Field", &bDepthOfFieldEnabled);
			ImGui::Checkbox("Bloom", &bBloomEnabled);
			ImGui::Checkbox("SSR", &bSsrEnabled);
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
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pCurrBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}