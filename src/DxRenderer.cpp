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

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

using namespace DirectX;

namespace {
	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";

	const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
	const DXGI_FORMAT SpecularMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT VelocityMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
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
		float MaxDistance = 100.0f;
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

	CheckReturn(mImGui->Initialize(mhMainWnd, device, mCbvSrvUavHeap.Get(), SwapChainBufferCount, BackBufferFormat));

	std::array<ID3D12Resource*, SwapChainBufferCount> backBuffers;
	for (int i = 0; i < SwapChainBufferCount; ++i) {
		backBuffers[i] = BackBuffer(i);
	}
	CheckReturn(mBackBuffer->Initialize(device, width, height, shaderManager, BackBufferFormat, backBuffers.data(), SwapChainBufferCount));
	CheckReturn(mGBuffer->Initialize(device, width, height, shaderManager, mDepthStencilBuffer.Get(), DepthStencilView(), DepthStencilFormat));
	CheckReturn(mShadowMap->Initialize(device, shaderManager, 2048, 2048));
	CheckReturn(mSsao->Initialize(device, cmdList, shaderManager, width, height, 1));
	CheckReturn(mBlurFilter->Initialize(device, shaderManager));
	CheckReturn(mBloom->Initialize(device, shaderManager, width, height, 4, BackBufferFormat));
	CheckReturn(mSsr->Initialize(device, shaderManager, width, height, 2, BackBufferFormat));
	CheckReturn(mDof->Initialize(device, shaderManager, cmdList, width, height, 4, BackBufferFormat));
	CheckReturn(mMotionBlur->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mTaa->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mDebug->Initialize(device, shaderManager, width, height, BackBufferFormat));
	CheckReturn(mSkyCube->Initialize(device, cmdList, shaderManager, width, height, BackBufferFormat));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	CheckReturn(FlushCommandQueue());

	CheckReturn(CompileShaders());
	CheckReturn(BuildGeometries());

	CheckReturn(BuildFrameResources());
	CheckReturn(BuildDescriptorHeaps());
	BuildDescriptors();
	CheckReturn(BuildRootSignatures());
	CheckReturn(BuildPSOs());
	BuildRenderItems();

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}

	bInitialized = true;
	return true;
}

void DxRenderer::CleanUp() {
	mImGui->CleanUp();
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

	return true;
}

bool DxRenderer::Draw() {
	CheckHRESULT(mCurrFrameResource->CmdListAlloc->Reset());

	CheckReturn(DrawShadowMap());
	CheckReturn(DrawGBuffer());
	CheckReturn(DrawSsao());
	CheckReturn(DrawBackBuffer());
	CheckReturn(DrawSkyCube());
	CheckReturn(ApplySsr())
	if (bBloomEnabled) CheckReturn(ApplyBloom());
	if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
	if (bTaaEnabled) CheckReturn(ApplyTAA());
	if (bMotionBlurEnabled) CheckReturn(ApplyMotionBlur());
	if (bDebuggingEnabled) CheckReturn(DrawDebuggingInfo());
	if (bShowImGui) CheckReturn(DrawImGui());

	CheckHRESULT(mSwapChain->Present(0, 0));
	NextBackBuffer();

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

	CheckReturn(mBackBuffer->OnResize(width, height));
	CheckReturn(mGBuffer->OnResize(width, height, mDepthStencilBuffer.Get()));
	CheckReturn(mSsao->OnResize(width, height));
	CheckReturn(mBloom->OnResize(width, height));
	CheckReturn(mSsr->OnResize(width, height));
	CheckReturn(mDof->OnResize(cmdList, width, height));
	CheckReturn(mMotionBlur->OnResize(width, height));
	CheckReturn(mTaa->OnResize(width, height));
	CheckReturn(mDebug->OnResize(width, height));

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
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + GBuffer::NumRenderTargets + Ssao::NumRenderTargets + TemporalAA::NumRenderTargets + MotionBlur::NumRenderTargets
		+ DepthOfField::NumRenderTargets + Bloom::NumRenderTargets + Ssr::NumRenderTargets;
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

	return true;
}

bool DxRenderer::CompileShaders() {
	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	{
		const std::wstring filePath = ShaderFilePath + L"Mapping.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "mappingVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "mappingPS"));
	}

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

	ID3D12GraphicsCommandList* cmdList = mCommandList.Get();
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

bool DxRenderer::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = MAX_DESCRIPTOR_SIZE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvUavHeap)));

	return true;
}

void DxRenderer::BuildDescriptors() {
	auto cpuStart = mCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	auto gpuStart = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	
	auto descSize = GetCbvSrvUavDescriptorSize();
	auto rtvDescSize = GetRtvDescriptorSize();
	auto dsvDescSize = GetDsvDescriptorSize();
		
	auto hCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart);
	auto hGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart);
	auto hCpuDsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart);
	auto hCpuRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart);

	mImGui->BuildDescriptors(hCpu, hGpu, descSize);

	mBackBuffer->BuildDescriptors(hCpu, hGpu, descSize);
	mGBuffer->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize, mDepthStencilBuffer.Get());
	mShadowMap->BuildDescriptors(hCpu, hGpu, hCpuDsv, descSize, dsvDescSize);
	mSsao->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mBloom->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mSsr->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mDof->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);
	mMotionBlur->BuildDescriptors(hCpuRtv, rtvDescSize);
	mTaa->BuildDescriptors(hCpu, hGpu, hCpuRtv, descSize, rtvDescSize);

	mhCpuDescForTexMaps = hCpu;
	mhGpuDescForTexMaps = hGpu;
}

bool DxRenderer::BuildRootSignatures() {
	auto staticSamplers = Samplers::GetStaticSamplers();

	// For mapping
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EMappingRootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[EMappingRootSignatureLayout::EInput].InitAsDescriptorTable(1, &texTable);

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["mapping"].GetAddressOf()));
	}
			
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

	return true;
}

bool DxRenderer::BuildPSOs() {
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,			0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc;
	ZeroMemory(&defaultPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	defaultPsoDesc.InputLayout = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
	defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	defaultPsoDesc.SampleMask = UINT_MAX;
	defaultPsoDesc.SampleDesc.Count = 1;
	defaultPsoDesc.SampleDesc.Quality = 0;
	defaultPsoDesc.DSVFormat = DepthStencilFormat;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = defaultPsoDesc;
	quadPsoDesc.InputLayout = { nullptr, 0 };
	quadPsoDesc.NumRenderTargets = 1;
	quadPsoDesc.DepthStencilState.DepthEnable = FALSE;
	quadPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC backBufferMappingPsoDesc = quadPsoDesc;
	backBufferMappingPsoDesc.pRootSignature = mRootSignatures["mapping"].Get();
	{
		auto vs = mShaderManager->GetShader("mappingVS");
		auto ps = mShaderManager->GetShader("mappingPS");
		backBufferMappingPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		backBufferMappingPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	backBufferMappingPsoDesc.RTVFormats[0] = BackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&backBufferMappingPsoDesc, IID_PPV_ARGS(&mPSOs["backBufferMapping"])));
	
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
	CheckReturn(mBackBuffer->BuildPso());
	CheckReturn(mGBuffer->BuildPso(inputLayoutDesc));
	CheckReturn(mShadowMap->BuildPso(inputLayoutDesc, DepthStencilFormat));
	CheckReturn(mSsao->BuildPso(inputLayoutDesc));
	CheckReturn(mBloom->BuildPso(inputLayoutDesc));
	CheckReturn(mBlurFilter->BuildPso());
	CheckReturn(mSsr->BuildPso());
	CheckReturn(mDof->BuildPso(inputLayoutDesc, DepthStencilFormat));
	CheckReturn(mMotionBlur->BuildPso(inputLayoutDesc, DepthStencilFormat));
	CheckReturn(mTaa->BuildPso());
	CheckReturn(mDebug->BuildPso());

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

	ID3D12GraphicsCommandList* cmdList = mCommandList.Get();
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
	mMainPassCB->Lights[0].Direction = mLightDir;
	mMainPassCB->Lights[0].Strength = { 0.4f, 0.4f, 0.4f };

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

			cmdList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_DEPTH_READ,
					D3D12_RESOURCE_STATE_DEPTH_WRITE
				)
			);

			cmdList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			cmdList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_DEPTH_READ
				)
			);

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

			auto aoCoeffMap = mSsao->AOCoefficientMapResource(0);
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(aoCoeffMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

			cmdList->ClearRenderTargetView(mSsao->AOCoefficientMapRtv(0), Ssao::AOCoefficientMapClearValues, 0, nullptr);

			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(aoCoeffMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

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

	const auto pCurrBackBuffer = CurrentBackBuffer();
	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto pCurrBackBufferView = CurrentBackBufferView();
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

	const auto backBuffer = CurrentBackBuffer();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	const auto passCBAddress = mCurrFrameResource->PassCB.Resource()->GetGPUVirtualAddress();
	mSkyCube->Run(
		cmdList,
		mScreenViewport,
		mScissorRect,
		CurrentBackBufferView(),
		DepthStencilView(),
		passCBAddress,
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

	mTaa->Run(
		cmdList,
		mBackBuffer->BackBuffer(CurrentBackBufferIndex()),
		mBackBuffer->BackBufferSrv(CurrentBackBufferIndex()),
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

	const auto backBuffer = CurrentBackBuffer();
	const auto ssrMap0 = mSsr->SsrMapResource(0);
	const auto ssrMap1 = mSsr->SsrMapResource(1);

	auto ssrCBAddress = mCurrFrameResource->SsrCB.Resource()->GetGPUVirtualAddress();

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
				),
				CD3DX12_RESOURCE_BARRIER::Transition(
					ssrMap0,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				)
			};
			cmdList->ResourceBarrier(_countof(barriers), barriers);
		}

		mSsr->BuildSsr(
			cmdList,
			ssrCBAddress,
			mBackBuffer->BackBufferSrv(CurrentBackBufferIndex()),
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
				),
				CD3DX12_RESOURCE_BARRIER::Transition(
					ssrMap0,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
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
			ShaderArgs::Bloom::BlurCount
		);

		CheckHRESULT(cmdList->Close());
		ID3D12CommandList* cmdsLists[] = { cmdList };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	}
	else {
		if (!bSsrMapCleanedUp) {
			CheckHRESULT(cmdList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			cmdList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					ssrMap0,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				)
			);

			cmdList->ClearRenderTargetView(mSsr->SsrMapRtv(0), Ssr::ClearValues, 0, nullptr);

			cmdList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					ssrMap0,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			);

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
		mScreenViewport,
		mScissorRect,
		ssrCBAddress,
		mSkyCube->CubeMapSrv(),
		mBackBuffer->BackBufferSrv(CurrentBackBufferIndex()),
		mGBuffer->NormalMapSrv(),
		mGBuffer->DepthMapSrv(),
		mGBuffer->SpecularMapSrv()
	);

	const auto resultMap = mSsr->ResultMapResource();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				resultMap,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_COPY_DEST
			),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	cmdList->CopyResource(backBuffer, resultMap);

	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				resultMap,
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			),
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

	const auto backBuffer = CurrentBackBuffer();
	const auto bloomMap0 = mBloom->BloomMapResource(0);
	const auto bloomMap1 = mBloom->BloomMapResource(1);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				bloomMap0,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		};
		cmdList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}

	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				bloomMap0,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		};
		mCommandList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}

	const auto backBufferSrv = mBackBuffer->BackBufferSrv(CurrentBackBufferIndex());
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

	const auto resultMap = mBloom->ResultMapResource();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				resultMap ,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			)
		};
		mCommandList->ResourceBarrier(_countof(barriers), barriers);
	}

	mCommandList->CopyResource(backBuffer, resultMap);

	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				resultMap ,
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
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

	const auto cocMap = mDof->CocMapResource();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cocMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
	mDof->CalcCoc(
		cmdList,
		mScreenViewport,
		mScissorRect,
		dofCBAddress,
		mGBuffer->DepthMapSrv()
	);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cocMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	const auto dofMap = mDof->DofMapResource();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dofMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
	mDof->ApplyDof(
		cmdList,
		mScreenViewport,
		mScissorRect,
		mBackBuffer->BackBufferSrv(CurrentBackBufferIndex()),
		ShaderArgs::DepthOfField::BokehRadius,
		ShaderArgs::DepthOfField::CocThreshold,
		ShaderArgs::DepthOfField::CocDiffThreshold,
		ShaderArgs::DepthOfField::HighlightPower,
		ShaderArgs::DepthOfField::SampleCount
	);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dofMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	const auto blurCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mDof->BlurDof(
		cmdList,
		blurCBAddress,
		ShaderArgs::DepthOfField::BlurCount
	);

	const auto backBuffer = CurrentBackBuffer();
	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(dofMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}
	cmdList->CopyResource(backBuffer, dofMap);
	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(dofMap, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
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
	
	const auto backBuffer = CurrentBackBuffer();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	mMotionBlur->Run(
		cmdList,
		mBackBuffer->BackBufferSrv(CurrentBackBufferIndex()),
		mGBuffer->DepthMapSrv(),
		mGBuffer->VelocityMapSrv(),
		ShaderArgs::MotionBlur::Intensity,
		ShaderArgs::MotionBlur::Limit,
		ShaderArgs::MotionBlur::DepthBias,
		ShaderArgs::MotionBlur::SampleCount
	);

	const auto motionVector = mMotionBlur->MotionVectorMapResource();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(motionVector, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}
	cmdList->CopyResource(backBuffer, motionVector);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(motionVector, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
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
	
	const auto backBuffer = CurrentBackBuffer();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	mDebug->Run(
		cmdList,
		CurrentBackBufferView(),
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

	const auto pCurrBackBuffer = CurrentBackBuffer();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pCurrBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	auto pCurrBackBufferView = CurrentBackBufferView();
	cmdList->OMSetRenderTargets(1, &pCurrBackBufferView, true, nullptr);

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Main Panel");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Debug")) {
			ImGui::Checkbox("Debugging windows", &bDebuggingEnabled);
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