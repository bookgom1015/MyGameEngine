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

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

using namespace DirectX;

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 9> GetStaticSamplers() {
		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
			0,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			1,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			3,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			4,									// shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
			5,									// shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicBorder(
			6,									// shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
			0.0f,								// mipLODBias
			8,									// maxAnisotropy
			D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
		);

		const CD3DX12_STATIC_SAMPLER_DESC depthMap(
			7,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
			0.0f,								// mipLODBias
			0,									// maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			8,													// shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressW
			0.0f,												// mipLODBias
			16,													// maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);

		return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp, anisotropicBorder, depthMap, shadow };
	}

	const std::wstring ShaderFilePath = L".\\..\\..\\assets\\shaders\\hlsl\\";

	const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
	const DXGI_FORMAT SpecularMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
	const DXGI_FORMAT VelocityMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
}

DxRenderer::DxRenderer() {
	bIsCleanedUp = false;

	mCurrDescriptorIndex = EReservedSrvs::ERS_Count;

	mMainPassCB = std::make_unique<PassConstants>();
	mShadowPassCB = std::make_unique<PassConstants>();

	mShaderManager = std::make_unique<ShaderManager>();

	mShadowMap = std::make_unique<ShadowMap>();
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float widthSquared = 32.0f * 32.0f;
	mSceneBounds.Radius = sqrtf(widthSquared + widthSquared);
	mLightDir = { 0.57735f, -0.57735f, 0.57735f };

	mGBuffer = std::make_unique<GBuffer>();
	mSsao = std::make_unique<Ssao>();
	mTaa = std::make_unique<TemporalAA>();
	mMotionBlur = std::make_unique<MotionBlur>();
	mDof = std::make_unique<DepthOfField>();

	auto blurWeights = Blur::CalcGaussWeights(2.5f);
	mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	bShadowMapCleanedUp = false;
	bSsaoMapCleanedUp = false;
	bInitiatingTaa = true;

	mTaaModulationFactor = 0.9f;

	mMotionBlurIntensity = 0.01f;
	mMotionBlurLimit = 0.005f;
	mMotionBlurDepthBias = 0.05f;
	mMotionBlurNumSamples = 10;

	mBokehRadius = 2.0f;
	mFocusRange = 8.0f;
	mFocusingSpeed = 8.0f;

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

	auto pDevice = md3dDevice.Get();
	mGraphicsMemory = std::make_unique<GraphicsMemory>(pDevice);

	CheckHRESULT(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckReturn(mShadowMap->Initialize(pDevice, 2048, 2048));
	CheckReturn(mGBuffer->Initialize(pDevice, width, height, BackBufferFormat, NormalMapFormat, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, SpecularMapFormat, VelocityMapFormat));
	CheckReturn(mSsao->Initialize(pDevice, mCommandList.Get(), width / 2, height / 2, AmbientMapFormat));
	CheckReturn(mTaa->Initialize(pDevice, width, height, BackBufferFormat));
	CheckReturn(mMotionBlur->Initialize(pDevice, width, height, BackBufferFormat));
	CheckReturn(mDof->Initialize(pDevice, width, height, 4, BackBufferFormat));

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
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

	CheckReturn(InitImGui());

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}

	bInitialized = true;
	return true;
}

void DxRenderer::CleanUp() {
	CleanUpImGui();
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
	if (bTaaEnabled) CheckReturn(ApplyTAA());
	if (bDepthOfFieldEnabled) CheckReturn(ApplyDepthOfField());
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

	BuildBackBufferDescriptors();

	CheckReturn(mGBuffer->OnResize(width, height, mDepthStencilBuffer.Get()));
	CheckReturn(mSsao->OnResize(width / 2, height / 2));
	CheckReturn(mTaa->OnResize(width, height));
	CheckReturn(mMotionBlur->OnResize(width, height));
	CheckReturn(mDof->OnResize(width, height));

	for (size_t i = 0, end = mHaltonSequence.size(); i < end; ++i) {
		auto offset = mHaltonSequence[i];
		mFittedToBakcBufferHaltonSequence[i] = XMFLOAT2(((offset.x - 0.5f) / width) * 2.0f, ((offset.y - 0.5f) / height) * 2.0f);
	}

	return true;
}

void* DxRenderer::AddModel(const std::string& file, const Transform& trans, ERenderTypes type) {
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

	auto texMap = std::make_unique<Texture>();
	texMap->DescriptorIndex = EReservedSrvs::ERS_Cube;

	std::wstring filename;
	filename.assign(file.begin(), file.end());

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
		return false;
	}

	const auto& resource = texMap->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, D3D12Util::GetCpuHandle(mCbvSrvUavHeap.Get(), EReservedSrvs::ERS_Cube, GetCbvSrvUavDescriptorSize()));

	CheckHRESULT(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	CheckReturn(FlushCommandQueue());

	mTextures[file] = std::move(texMap);

	return true;
}

bool DxRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + GBuffer::NumRenderTargets + Ssao::NumRenderTargets + TemporalAA::NumRenderTargets + MotionBlur::NumRenderTargets
		+ DepthOfField::NumRenderTargets;
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

bool DxRenderer::InitImGui() {
	// Setup dear ImGui context.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style.
	ImGui::StyleColorsDark();

	auto descSize = GetCbvSrvUavDescriptorSize();

	// Setup platform/renderer backends
	CheckReturn(ImGui_ImplWin32_Init(mhMainWnd));
	CheckReturn(ImGui_ImplDX12_Init(
		md3dDevice.Get(),
		SwapChainBufferCount,
		BackBufferFormat,
		mCbvSrvUavHeap.Get(),
		D3D12Util::GetCpuHandle(mCbvSrvUavHeap.Get(), EReservedSrvs::ERS_Font, descSize),
		D3D12Util::GetGpuHandle(mCbvSrvUavHeap.Get(), EReservedSrvs::ERS_Font, descSize)
	));

	return true;
}

void DxRenderer::CleanUpImGui() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool DxRenderer::CompileShaders() {
	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	{
		const std::wstring filePath = ShaderFilePath + L"DrawBackBuffer.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "backBufferVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "backBufferPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"DrawGBuffer.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "gbufferVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "gbufferPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"DrawGBufferSky.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "gbufferSkyVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "gbufferSkyPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Shadow.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "shadowVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, alphaTestDefines, "PS", "ps_5_1", "shadowPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Debug.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "debugVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "debugPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Sky.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "skyVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "skyPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Ssao.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "ssaoVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "ssaoPS"));
	}
	{
		const D3D_SHADER_MACRO nonBilateralDefines[] = {
			"NON_BILATERAL", "1",
			NULL, NULL
		};

		const std::wstring filePath = ShaderFilePath + L"GaussianBlur.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "gausBlurVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "gausBlurPS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nonBilateralDefines, "PS", "ps_5_1", "nbGausBlurPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"TemporalAA.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "taaVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "taaPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"MotionBlur.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "motionBlurVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "motionBlurPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Mapping.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "mappingVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "mappingPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Coc.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "cocVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "cocPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"CocBlur.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "cocBlurVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "cocBlurPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"Bokeh.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "bokehVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "bokehPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"DepthOfField.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "dofVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "dofPS"));
	}
	{
		const std::wstring filePath = ShaderFilePath + L"FocalDistance.hlsl";
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "fdVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "fdPS"));
	}

	return true;
}

bool DxRenderer::BuildGeometries() {

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(64.0f, 8, 8);

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
	heapDesc.NumDescriptors = ECbvSrvUavHeapLayout::ECSUHL_Count;
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

	BuildBackBufferDescriptors();

	mShadowMap->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_Shadow, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedSrvs::ERS_Shadow, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, dsvDescSize)
	);

	mGBuffer->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_Color, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedSrvs::ERS_Color, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, ERtvHeapLayout::ERHL_Color, rtvDescSize),
		descSize, rtvDescSize,
		mDepthStencilBuffer.Get()
	);

	mSsao->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_Ambient0, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedSrvs::ERS_Ambient0, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, ERtvHeapLayout::ERHL_Ambient0, rtvDescSize),
		descSize, rtvDescSize
	);

	mTaa->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_Resolve, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedSrvs::ERS_Resolve, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, ERtvHeapLayout::ERHL_Resolve, rtvDescSize),
		descSize, rtvDescSize
	);

	mDof->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_Coc, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedSrvs::ERS_Coc, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedUavs::ERU_FocalDist, descSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, EReservedUavs::ERU_FocalDist, descSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, ERtvHeapLayout::ERHL_Coc, rtvDescSize),
		descSize, rtvDescSize		
	);

	mMotionBlur->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, ERtvHeapLayout::ERHL_MotionBlur, rtvDescSize));
}

void DxRenderer::BuildBackBufferDescriptors() {
	auto cpuStart = mCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	auto descSize = GetCbvSrvUavDescriptorSize();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = BackBufferFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	auto hDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, EReservedSrvs::ERS_BackBuffer0, descSize);
	for (int i = 0; i < SwapChainBufferCount; ++i) {
		md3dDevice->CreateShaderResourceView(BackBuffer(i), &srvDesc, hDesc);
		hDesc.Offset(1, descSize);
	}
}

bool DxRenderer::BuildRootSignatures() {
	// Default
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EDefaultRootSignatureLayout::EDRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, EReservedSrvs::ERS_Count, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, (ECbvSrvUavHeapLayout::ECSUHL_Uavs - ECbvSrvUavHeapLayout::ECSUHL_Srvs), EReservedSrvs::ERS_Count, 0);

		slotRootParameter[EDefaultRootSignatureLayout::EDRS_ObjectCB].InitAsConstantBufferView(0);
		slotRootParameter[EDefaultRootSignatureLayout::EDRS_PassCB].InitAsConstantBufferView(1);
		slotRootParameter[EDefaultRootSignatureLayout::EDRS_MatCB].InitAsConstantBufferView(2);
		slotRootParameter[EDefaultRootSignatureLayout::EDRS_ReservedTexMaps].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EDefaultRootSignatureLayout::EDRS_AllocatedTexMaps].InitAsDescriptorTable(1, &texTable1);

		auto samplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["default"].GetAddressOf()));
	}
	// For SSAO
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ESsaoRootSignatureLayout::ESRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

		slotRootParameter[ESsaoRootSignatureLayout::ESRS_PassCB].InitAsConstantBufferView(0);
		slotRootParameter[ESsaoRootSignatureLayout::ESRS_NormalDepth].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ESsaoRootSignatureLayout::ESRS_RandomVector].InitAsDescriptorTable(1, &texTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["ssao"].GetAddressOf()));
	}
	// For blur
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EBlurRootSignatureLayout::EBRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

		slotRootParameter[EBlurRootSignatureLayout::EBRS_BlurCB].InitAsConstantBufferView(0);
		slotRootParameter[EBlurRootSignatureLayout::EBRS_Consts].InitAsConstants(3, 1);
		slotRootParameter[EBlurRootSignatureLayout::EBRS_NormalDepth].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EBlurRootSignatureLayout::EBRS_Input].InitAsDescriptorTable(1, &texTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["blur"].GetAddressOf()));
	}
	// For TAA
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ETaaRootSignatureLayout::ETRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable2;
		texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

		slotRootParameter[ETaaRootSignatureLayout::ETRS_Input].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ETaaRootSignatureLayout::ETRS_History].InitAsDescriptorTable(1, &texTable1);
		slotRootParameter[ETaaRootSignatureLayout::ETRS_Velocity].InitAsDescriptorTable(1, &texTable2);
		slotRootParameter[ETaaRootSignatureLayout::ERTS_Factor].InitAsConstants(1, 0);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["taa"].GetAddressOf()));
	}
	// For motion blur
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EMotionBlurRootSignatureLayout::EMBRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable2;
		texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

		slotRootParameter[EMotionBlurRootSignatureLayout::EMBRS_Input].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EMotionBlurRootSignatureLayout::EMBRS_Depth].InitAsDescriptorTable(1, &texTable1);
		slotRootParameter[EMotionBlurRootSignatureLayout::EMBRS_Velocity].InitAsDescriptorTable(1, &texTable2);
		slotRootParameter[EMotionBlurRootSignatureLayout::EMBRS_Consts].InitAsConstants(EMotionBlurRootConstantsLayout::EMBRC_Count, 0);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["motionBlur"].GetAddressOf()));
	}
	// For mapping
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EMappingRootSignatureLayout::EMRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[EMappingRootSignatureLayout::EMRS_Input].InitAsDescriptorTable(1, &texTable);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["mapping"].GetAddressOf()));
	}
	// For circle of confusion
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ECocRootSignatureLayout::ECRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[ECocRootSignatureLayout::ECRS_DofCB].InitAsConstantBufferView(0);
		slotRootParameter[ECocRootSignatureLayout::ECRS_Depth].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ECocRootSignatureLayout::ECRS_FocalDist].InitAsDescriptorTable(1, &texTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["coc"].GetAddressOf()));
	}
	// For bokeh
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EBokehRootSignatureLayout::EBKHRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[EBokehRootSignatureLayout::EBKHRS_Input].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EBokehRootSignatureLayout::EBKHRS_Consts].InitAsConstants(EBokehRootConstantsLayout::EBKHRCS_Count, 0);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["bokeh"].GetAddressOf()));
	}
	// For depth of field
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EDofRootSignatureLayout::EDOFRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1, 0);

		slotRootParameter[EDofRootSignatureLayout::EDOFRS_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EDofRootSignatureLayout::EDOFRS_CocAndBokeh].InitAsDescriptorTable(1, &texTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["dof"].GetAddressOf()));
	}
	// For focal distance
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[EFocalDistanceRootSignatureLayout::EFDRS_Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[EFocalDistanceRootSignatureLayout::EFDRS_DofCB].InitAsConstantBufferView(0);
		slotRootParameter[EFocalDistanceRootSignatureLayout::EFDRS_Depth].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[EFocalDistanceRootSignatureLayout::EFDRS_FocalDist].InitAsDescriptorTable(1, &texTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignatures["fd"].GetAddressOf()));
	}

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
	defaultPsoDesc.pRootSignature = mRootSignatures["default"].Get();
	defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	defaultPsoDesc.SampleMask = UINT_MAX;
	defaultPsoDesc.SampleDesc.Count = 1;
	defaultPsoDesc.SampleDesc.Quality = 0;
	defaultPsoDesc.DSVFormat = DepthStencilFormat;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC backBufferPsoDesc = defaultPsoDesc;
	backBufferPsoDesc.InputLayout = { nullptr,0 };
	{
		auto vs = mShaderManager->GetShader("backBufferVS");
		auto ps = mShaderManager->GetShader("backBufferPS");
		backBufferPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		backBufferPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	backBufferPsoDesc.NumRenderTargets = 1;
	backBufferPsoDesc.RTVFormats[0] = BackBufferFormat;
	backBufferPsoDesc.DepthStencilState.DepthEnable = FALSE;
	backBufferPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&backBufferPsoDesc, IID_PPV_ARGS(&mPSOs["backBuffer"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferPsoDesc = defaultPsoDesc;
	{
		auto vs = mShaderManager->GetShader("gbufferVS");
		auto ps = mShaderManager->GetShader("gbufferPS");
		gbufferPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		gbufferPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	gbufferPsoDesc.NumRenderTargets = GBuffer::NumRenderTargets;
	gbufferPsoDesc.RTVFormats[0] = BackBufferFormat;
	gbufferPsoDesc.RTVFormats[1] = BackBufferFormat;
	gbufferPsoDesc.RTVFormats[2] = NormalMapFormat;
	gbufferPsoDesc.RTVFormats[3] = SpecularMapFormat;
	gbufferPsoDesc.RTVFormats[4] = VelocityMapFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&gbufferPsoDesc, IID_PPV_ARGS(&mPSOs["gbuffer"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferSkyPsoDesc = gbufferPsoDesc;
	{
		auto vs = mShaderManager->GetShader("gbufferSkyVS");
		auto ps = mShaderManager->GetShader("gbufferSkyPS");
		gbufferSkyPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		gbufferSkyPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	gbufferSkyPsoDesc.NumRenderTargets = 1;
	gbufferSkyPsoDesc.RTVFormats[0] = VelocityMapFormat;
	gbufferSkyPsoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	gbufferSkyPsoDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
	gbufferSkyPsoDesc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
	gbufferSkyPsoDesc.RTVFormats[4] = DXGI_FORMAT_UNKNOWN;
	gbufferSkyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	gbufferSkyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	gbufferSkyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	gbufferSkyPsoDesc.DepthStencilState.StencilEnable = FALSE;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&gbufferSkyPsoDesc, IID_PPV_ARGS(&mPSOs["gbufferSky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = defaultPsoDesc;
	{
		auto vs = mShaderManager->GetShader("shadowVS");
		auto ps = mShaderManager->GetShader("shadowPS");
		shadowPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		shadowPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	shadowPsoDesc.NumRenderTargets = 0;
	shadowPsoDesc.RasterizerState.DepthBias = 100000;
	shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.1f;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = defaultPsoDesc;
	debugPsoDesc.InputLayout = { nullptr,0 };
	{
		auto vs = mShaderManager->GetShader("debugVS");
		auto ps = mShaderManager->GetShader("debugPS");
		debugPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		debugPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = BackBufferFormat;
	debugPsoDesc.DepthStencilState.DepthEnable = FALSE;
	debugPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = defaultPsoDesc;
	{
		auto vs = mShaderManager->GetShader("skyVS");
		auto ps = mShaderManager->GetShader("skyPS");
		skyPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		skyPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	skyPsoDesc.NumRenderTargets = 1;
	skyPsoDesc.RTVFormats[0] = BackBufferFormat;
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	skyPsoDesc.DepthStencilState.StencilEnable = FALSE;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = defaultPsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr,0 };
	ssaoPsoDesc.pRootSignature = mRootSignatures["ssao"].Get();
	{
		auto vs = mShaderManager->GetShader("ssaoVS");
		auto ps = mShaderManager->GetShader("ssaoPS");
		ssaoPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		ssaoPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	ssaoPsoDesc.NumRenderTargets = 1;
	ssaoPsoDesc.RTVFormats[0] = AmbientMapFormat;
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = defaultPsoDesc;
	ssaoBlurPsoDesc.InputLayout = { nullptr, 0 };
	ssaoBlurPsoDesc.pRootSignature = mRootSignatures["blur"].Get();
	{
		auto vs = mShaderManager->GetShader("gausBlurVS");
		auto ps = mShaderManager->GetShader("gausBlurPS");
		ssaoBlurPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		ssaoBlurPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	ssaoBlurPsoDesc.NumRenderTargets = 1;
	ssaoBlurPsoDesc.RTVFormats[0] = AmbientMapFormat;
	ssaoBlurPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoBlurPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC taaPsoDesc = defaultPsoDesc;
	taaPsoDesc.InputLayout = { nullptr, 0 };
	taaPsoDesc.pRootSignature = mRootSignatures["taa"].Get();
	{
		auto vs = mShaderManager->GetShader("taaVS");
		auto ps = mShaderManager->GetShader("taaPS");
		taaPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		taaPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	taaPsoDesc.NumRenderTargets = 1;
	taaPsoDesc.RTVFormats[0] = BackBufferFormat;
	taaPsoDesc.DepthStencilState.DepthEnable = false;
	taaPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&taaPsoDesc, IID_PPV_ARGS(&mPSOs["taa"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC motionBlurPsoDesc = defaultPsoDesc;
	motionBlurPsoDesc.InputLayout = { nullptr, 0 };
	motionBlurPsoDesc.pRootSignature = mRootSignatures["motionBlur"].Get();
	{
		auto vs = mShaderManager->GetShader("motionBlurVS");
		auto ps = mShaderManager->GetShader("motionBlurPS");
		motionBlurPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		motionBlurPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	motionBlurPsoDesc.NumRenderTargets = 1;
	motionBlurPsoDesc.RTVFormats[0] = BackBufferFormat;
	motionBlurPsoDesc.DepthStencilState.DepthEnable = false;
	motionBlurPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&motionBlurPsoDesc, IID_PPV_ARGS(&mPSOs["motionBlur"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC backBufferBlurPsoDesc = ssaoBlurPsoDesc;
	{
		auto ps = mShaderManager->GetShader("nbGausBlurPS");		
		backBufferBlurPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	backBufferBlurPsoDesc.RTVFormats[0] = BackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&backBufferBlurPsoDesc, IID_PPV_ARGS(&mPSOs["backBufferBlur"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC backBufferMappingPsoDesc = defaultPsoDesc;
	backBufferMappingPsoDesc.InputLayout = { nullptr, 0 };
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
	backBufferMappingPsoDesc.NumRenderTargets = 1;
	backBufferMappingPsoDesc.RTVFormats[0] = BackBufferFormat;
	backBufferMappingPsoDesc.DepthStencilState.DepthEnable = false;
	backBufferMappingPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&backBufferMappingPsoDesc, IID_PPV_ARGS(&mPSOs["backBufferMapping"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC cocPsoDesc = defaultPsoDesc;
	cocPsoDesc.InputLayout = { nullptr, 0 };
	cocPsoDesc.pRootSignature = mRootSignatures["coc"].Get();
	{
		auto vs = mShaderManager->GetShader("cocVS");
		auto ps = mShaderManager->GetShader("cocPS");
		cocPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		cocPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	cocPsoDesc.NumRenderTargets = 1;
	cocPsoDesc.RTVFormats[0] = mDof->CocMapFormat;
	cocPsoDesc.DepthStencilState.DepthEnable = false;
	cocPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&cocPsoDesc, IID_PPV_ARGS(&mPSOs["coc"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC bokehPsoDesc = defaultPsoDesc;
	bokehPsoDesc.InputLayout = { nullptr, 0 };
	bokehPsoDesc.pRootSignature = mRootSignatures["bokeh"].Get();
	{
		auto vs = mShaderManager->GetShader("bokehVS");
		auto ps = mShaderManager->GetShader("bokehPS");
		bokehPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		bokehPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	bokehPsoDesc.NumRenderTargets = 1;
	bokehPsoDesc.RTVFormats[0] = mDof->BokehMapFormat();
	bokehPsoDesc.DepthStencilState.DepthEnable = false;
	bokehPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&bokehPsoDesc, IID_PPV_ARGS(&mPSOs["bokeh"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC dofPsoDesc = defaultPsoDesc;
	dofPsoDesc.InputLayout = { nullptr, 0 };
	dofPsoDesc.pRootSignature = mRootSignatures["dof"].Get();
	{
		auto vs = mShaderManager->GetShader("dofVS");
		auto ps = mShaderManager->GetShader("dofPS");
		dofPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		dofPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	dofPsoDesc.NumRenderTargets = 1;
	dofPsoDesc.RTVFormats[0] = BackBufferFormat;
	dofPsoDesc.DepthStencilState.DepthEnable = false;
	dofPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&dofPsoDesc, IID_PPV_ARGS(&mPSOs["dof"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC fdPsoDesc = defaultPsoDesc;
	fdPsoDesc.InputLayout = { nullptr, 0 };
	fdPsoDesc.pRootSignature = mRootSignatures["fd"].Get();
	{
		auto vs = mShaderManager->GetShader("fdVS");
		auto ps = mShaderManager->GetShader("fdPS");
		fdPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		fdPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	fdPsoDesc.NumRenderTargets = 0;
	fdPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.DepthStencilState.DepthEnable = false;
	fdPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&fdPsoDesc, IID_PPV_ARGS(&mPSOs["fd"])));

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
	mRitemRefs[ERenderTypes::ESky].push_back(skyRitem.get());
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

void* DxRenderer::AddRenderItem(const std::string& file, const Transform& trans, ERenderTypes type) {
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

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(mCurrDescriptorIndex, GetCbvSrvUavDescriptorSize());

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
	mMainPassCB->Lights[0].Strength = { 0.6f, 0.6f, 0.6f };

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
	dofCB.FocusRange = mFocusRange;
	dofCB.FocusingSpeed = mFocusingSpeed;
	dofCB.DeltaTime = delta;

	auto& currDofCB = mCurrFrameResource->DofCB;
	currDofCB.CopyData(0, dofCB);

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

void DxRenderer::DrawRenderItems(const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto& objectCB = mCurrFrameResource->ObjectCB;
	auto& matCB = mCurrFrameResource->MaterialCB;

	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];

		mCommandList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB.Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_ObjectCB, objCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB.Resource()->GetGPUVirtualAddress() + ri->Material->MatCBIndex * matCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_MatCB, matCBAddress);
		}

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

bool DxRenderer::DrawShadowMap() {
	if (!bShadowEnabled) {
		if (!bShadowMapCleanedUp) {
			CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_DEPTH_READ,
					D3D12_RESOURCE_STATE_DEPTH_WRITE
				)
			);

			mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_DEPTH_READ
				)
			);

			CheckHRESULT(mCommandList->Close());
			ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

			bShadowMapCleanedUp = true;
		}
		
		return true;
	}
	bShadowMapCleanedUp = false;

	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["shadow"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["default"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_DEPTH_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);

	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	auto passCB = mCurrFrameResource->PassCB.Resource();
	UINT passCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(PassConstants));
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_PassCB, passCBAddress);

	auto hDesc = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	mCommandList->SetGraphicsRootDescriptorTable(EDefaultRootSignatureLayout::EDRS_AllocatedTexMaps, hDesc);

	DrawRenderItems(mRitemRefs[ERenderTypes::EOpaque]);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_DEPTH_READ
		)
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawGBuffer() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["gbuffer"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["default"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pColorMap = mGBuffer->ColorMapResource();
	const auto pAlbedoMap = mGBuffer->AlbedoMapResource();
	const auto pNormalMap = mGBuffer->NormalMapResource();
	const auto pSpecularMap = mGBuffer->SpecularMapResource();
	const auto pVelocityMap = mGBuffer->VelocityMapResource();

	D3D12_RESOURCE_BARRIER beginBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pColorMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pAlbedoMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pNormalMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pSpecularMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pVelocityMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
	};

	mCommandList->ResourceBarrier(
		_countof(beginBarriers),
		beginBarriers
	);

	const auto colorRtv = mGBuffer->ColorMapRtv();
	const auto albedoRtv = mGBuffer->AlbedoMapRtv();
	const auto normalRtv = mGBuffer->NormalMapRtv();
	const auto specularRtv = mGBuffer->SpecularMapRtv();
	const auto velocityRtv = mGBuffer->VelocityMapRtv();
	mCommandList->ClearRenderTargetView(colorRtv, GBuffer::ColorMapClearValues, 0, nullptr);
	mCommandList->ClearRenderTargetView(albedoRtv, GBuffer::AlbedoMapClearValues, 0, nullptr);
	mCommandList->ClearRenderTargetView(normalRtv, GBuffer::NormalMapClearValues, 0, nullptr);
	mCommandList->ClearRenderTargetView(specularRtv, GBuffer::SpecularMapClearValues, 0, nullptr);
	mCommandList->ClearRenderTargetView(velocityRtv, GBuffer::VelocityMapClearValues, 0, nullptr);

	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::NumRenderTargets> renderTargets = { colorRtv, albedoRtv, normalRtv, specularRtv, velocityRtv };
	mCommandList->OMSetRenderTargets(static_cast<UINT>(renderTargets.size()), renderTargets.data(), true, &DepthStencilView());
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	auto passCB = mCurrFrameResource->PassCB.Resource();
	mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_PassCB, passCB->GetGPUVirtualAddress());

	auto hDesc = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	mCommandList->SetGraphicsRootDescriptorTable(EDefaultRootSignatureLayout::EDRS_AllocatedTexMaps, hDesc);

	DrawRenderItems(mRitemRefs[ERenderTypes::EOpaque]);

	mCommandList->SetPipelineState(mPSOs["gbufferSky"].Get());
	mCommandList->OMSetRenderTargets(1, &velocityRtv, true, &DepthStencilView());
	mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_PassCB, passCB->GetGPUVirtualAddress());
	DrawRenderItems(mRitemRefs[ERenderTypes::ESky]);

	D3D12_RESOURCE_BARRIER endBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pVelocityMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pSpecularMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_DEPTH_READ
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pNormalMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pAlbedoMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pColorMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	};

	mCommandList->ResourceBarrier(
		_countof(endBarriers),
		endBarriers
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawSsao() {
	if (!bSsaoEnabled) {
		if (!bSsaoMapCleanedUp) {
			CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

			auto pSsaoMap = mSsao->AmbientMap0Resource();
			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					pSsaoMap,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				)
			);

			mCommandList->ClearRenderTargetView(mSsao->AmbientMap0Rtv(), Ssao::AmbientMapClearValues, 0, nullptr);

			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					pSsaoMap,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			);

			CheckHRESULT(mCommandList->Close());
			ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

			bSsaoMapCleanedUp = true;
		}

		return true;
	}
	bSsaoMapCleanedUp = false;

	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["ssao"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["ssao"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mSsao->Viewport());
	mCommandList->RSSetScissorRects(1, &mSsao->ScissorRect());
	
	// We compute the initial SSAO to AmbientMap0.
	auto pAmbientMap0 = mSsao->AmbientMap0Resource();
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pAmbientMap0,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto ambientMap0Rtv = mSsao->AmbientMap0Rtv();
	mCommandList->ClearRenderTargetView(ambientMap0Rtv, Ssao::AmbientMapClearValues, 0, nullptr);
	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &ambientMap0Rtv, true, nullptr);

	// Bind the constant buffer for this pass.
	auto ssaoCBAddress = mCurrFrameResource->SsaoCB.Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(ESsaoRootSignatureLayout::ESRS_PassCB, ssaoCBAddress);
	
	// Bind the normal and depth maps.
	mCommandList->SetGraphicsRootDescriptorTable(ESsaoRootSignatureLayout::ESRS_NormalDepth, mGBuffer->NormalMapSrv());
	
	// Bind the random vector map.
	mCommandList->SetGraphicsRootDescriptorTable(ESsaoRootSignatureLayout::ESRS_RandomVector, mSsao->RandomVectorMapSrv());
	
	// Draw fullscreen quad.
	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pAmbientMap0,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	);

	static const auto blurAmbientMap = [&](bool horzBlur) {	
		ID3D12Resource* output = nullptr;
		CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;
	
		// Ping-pong the two ambient map textures as we apply
		// horizontal and vertical blur passes.
		if (horzBlur == true) {
			output = mSsao->AmbientMap1Resource();
			outputRtv = mSsao->AmbientMap1Rtv();
			inputSrv = mSsao->AmbientMap0Srv();
			mCommandList->SetGraphicsRoot32BitConstant(EBlurRootSignatureLayout::EBRS_Consts, 1, EBlurRootConstantsLayout::EBRC_HorizontalBlur);
		}
		else {
			output = mSsao->AmbientMap0Resource();
			outputRtv = mSsao->AmbientMap0Rtv();
			inputSrv = mSsao->AmbientMap1Srv();
			mCommandList->SetGraphicsRoot32BitConstant(EBlurRootSignatureLayout::EBRS_Consts, 0, EBlurRootConstantsLayout::EBRC_HorizontalBlur);
		}
	
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				output,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		);
	
		mCommandList->ClearRenderTargetView(outputRtv, Ssao::AmbientMapClearValues, 0, nullptr);
	
		mCommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);
	
		// Bind the input ambient map to second texture table.
		mCommandList->SetGraphicsRootDescriptorTable(EBlurRootSignatureLayout::EBRS_Input, inputSrv);
	
		mCommandList->IASetVertexBuffers(0, 0, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(6, 1, 0, 0);
	
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				output,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		);
	};

	mCommandList->SetPipelineState(mPSOs["ssaoBlur"].Get());
	mCommandList->SetGraphicsRootSignature(mRootSignatures["blur"].Get());

	auto blurCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(EBlurRootSignatureLayout::EBRS_BlurCB, blurCBAddress);
	mCommandList->SetGraphicsRootDescriptorTable(EBlurRootSignatureLayout::EBRS_NormalDepth, mGBuffer->NormalMapSrv());
	
	for (int i = 0; i < 3; ++i) {
		blurAmbientMap(true);
		blurAmbientMap(false);
	}

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawBackBuffer() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["backBuffer"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["default"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pCurrBackBuffer = CurrentBackBuffer();
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto pCurrBackBufferView = CurrentBackBufferView();
	mCommandList->ClearRenderTargetView(pCurrBackBufferView, Colors::AliceBlue, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &pCurrBackBufferView, true, nullptr);

	auto passCB = mCurrFrameResource->PassCB.Resource();
	mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_PassCB, passCB->GetGPUVirtualAddress());

	auto hDesc = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	mCommandList->SetGraphicsRootDescriptorTable(EDefaultRootSignatureLayout::EDRS_ReservedTexMaps, hDesc);
	
	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawSkyCube() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["sky"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["default"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pBackBuffer = CurrentBackBuffer();
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	auto passCB = mCurrFrameResource->PassCB.Resource();
	mCommandList->SetGraphicsRootConstantBufferView(EDefaultRootSignatureLayout::EDRS_PassCB, passCB->GetGPUVirtualAddress());

	auto hDesc = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	mCommandList->SetGraphicsRootDescriptorTable(EDefaultRootSignatureLayout::EDRS_ReservedTexMaps, hDesc);

	DrawRenderItems(mRitemRefs[ERenderTypes::ESky]);
	
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyTAA() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["taa"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["taa"].Get());

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pBackBuffer = CurrentBackBuffer();
	const auto pResolveMap = mTaa->ResolveMapResource();

	if (bInitiatingTaa) {
		D3D12_RESOURCE_BARRIER beginBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				pBackBuffer,
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				pResolveMap,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST
			)
		};

		mCommandList->ResourceBarrier(
			_countof(beginBarriers),
			beginBarriers
		);

		mCommandList->CopyResource(pResolveMap, pBackBuffer);

		D3D12_RESOURCE_BARRIER endBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				pResolveMap,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				pBackBuffer,
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_PRESENT
			)
		};

		mCommandList->ResourceBarrier(
			_countof(endBarriers),
			endBarriers
		);

		bInitiatingTaa = false;
	}
	
	D3D12_RESOURCE_BARRIER beginBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pResolveMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	};
	
	mCommandList->ResourceBarrier(
		_countof(beginBarriers),
		beginBarriers
	);
	
	auto resolveRtv = mTaa->ResolveMapRtv();
	mCommandList->ClearRenderTargetView(resolveRtv, TemporalAA::ClearValues, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &resolveRtv, true, nullptr);

	mCommandList->SetGraphicsRootDescriptorTable(
		ETaaRootSignatureLayout::ETRS_Input,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_BackBuffer0 + CurrentBackBufferIndex(), descSize)
	);
	mCommandList->SetGraphicsRootDescriptorTable(
		ETaaRootSignatureLayout::ETRS_History, 
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_History, descSize)
	);
	mCommandList->SetGraphicsRootDescriptorTable(
		ETaaRootSignatureLayout::ETRS_Velocity,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Velocity, descSize)
	);

	mCommandList->SetGraphicsRoot32BitConstants(ETaaRootSignatureLayout::ERTS_Factor, 1, &mTaaModulationFactor, 0);
	
	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);
	
	auto pHistoryMap = mTaa->HistoryMapResource();
	
	D3D12_RESOURCE_BARRIER middleBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pHistoryMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pResolveMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST
		)
	};
	
	mCommandList->ResourceBarrier(
		_countof(middleBarriers),
		middleBarriers
	);
	
	mCommandList->CopyResource(pHistoryMap, pResolveMap);
	mCommandList->CopyResource(pBackBuffer, pResolveMap);
	
	D3D12_RESOURCE_BARRIER endBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pResolveMap,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pHistoryMap,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	};
	
	mCommandList->ResourceBarrier(
		_countof(endBarriers),
		endBarriers
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyDepthOfField() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["fd"].Get()));
	
	mCommandList->SetGraphicsRootSignature(mRootSignatures["fd"].Get());
	
	const auto pDescHeap = mCbvSrvUavHeap.Get();
	auto descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { pDescHeap };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto currDofCB = mCurrFrameResource->DofCB.Resource();
	mCommandList->SetGraphicsRootConstantBufferView(EFocalDistanceRootSignatureLayout::EFDRS_DofCB, currDofCB->GetGPUVirtualAddress());

	mCommandList->SetGraphicsRootDescriptorTable(
		EFocalDistanceRootSignatureLayout::EFDRS_Depth,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Depth, descSize)
	);

	mCommandList->SetGraphicsRootDescriptorTable(
		EFocalDistanceRootSignatureLayout::EFDRS_FocalDist,
		D3D12Util::GetGpuHandle(
			pDescHeap,
			EReservedUavs::ERU_FocalDist,
			descSize
		)
	);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	mCommandList->DrawInstanced(1, 1, 0, 0);

	mCommandList->SetPipelineState(mPSOs["coc"].Get());
	mCommandList->SetGraphicsRootSignature(mRootSignatures["coc"].Get());

	const auto pCocMap = mDof->CocMapResource();
	const auto pBokehMap = mDof->BokehMapResource();
	const auto pDofMap = mDof->DofMapResource();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCocMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	const auto cocRtv = mDof->CocMapRtv();
	mCommandList->ClearRenderTargetView(cocRtv, mDof->CocMapClearValues, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &cocRtv, true, nullptr);

	mCommandList->SetGraphicsRootConstantBufferView(ECocRootSignatureLayout::ECRS_DofCB, currDofCB->GetGPUVirtualAddress());

	mCommandList->SetGraphicsRootDescriptorTable(
		ECocRootSignatureLayout::ECRS_Depth,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Depth, descSize)
	);

	mCommandList->SetGraphicsRootDescriptorTable(
		ECocRootSignatureLayout::ECRS_FocalDist,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedUavs::ERU_FocalDist, descSize)
	);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	mCommandList->SetPipelineState(mPSOs["bokeh"].Get());
	mCommandList->SetGraphicsRootSignature(mRootSignatures["bokeh"].Get());

	mCommandList->RSSetViewports(1, &mDof->Viewport());
	mCommandList->RSSetScissorRects(1, &mDof->ScissorRect());

	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				pCocMap,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				pBokehMap ,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		};
		mCommandList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}

	const auto bokehRtv = mDof->BokehMapRtv();
	mCommandList->ClearRenderTargetView(bokehRtv, mDof->BokehMapClearValues, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &bokehRtv, true, nullptr);

	mCommandList->SetGraphicsRootDescriptorTable(
		EBokehRootSignatureLayout::EBKHRS_Input,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_BackBuffer0 + CurrentBackBufferIndex(), descSize)
	);

	float bokehConsts[EBokehRootConstantsLayout::EBKHRCS_Count] = { mBokehRadius };
	mCommandList->SetGraphicsRoot32BitConstants(EBokehRootSignatureLayout::EBKHRS_Consts, _countof(bokehConsts), bokehConsts, 0);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pBokehMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	);

	mCommandList->SetPipelineState(mPSOs["backBufferBlur"].Get());
	mCommandList->SetGraphicsRootSignature(mRootSignatures["blur"].Get());

	static const auto blurBokehMap = [&](bool horzBlur) {
		ID3D12Resource* output = nullptr;
		CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

		// Ping-pong the two ambient map textures as we apply
		// horizontal and vertical blur passes.
		if (horzBlur == true) {
			output = mDof->BokehBlurMapResource();
			outputRtv = mDof->BokehBlurMapRtv();
			inputSrv = mDof->BokehMapSrv();
			mCommandList->SetGraphicsRoot32BitConstant(EBlurRootSignatureLayout::EBRS_Consts, 1, EBlurRootConstantsLayout::EBRC_HorizontalBlur);
		}
		else {
			output = mDof->BokehMapResource();
			outputRtv = mDof->BokehMapRtv();
			inputSrv = mDof->BokehBlurMapSrv();
			mCommandList->SetGraphicsRoot32BitConstant(EBlurRootSignatureLayout::EBRS_Consts, 0, EBlurRootConstantsLayout::EBRC_HorizontalBlur);
		}

		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				output,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		);

		mCommandList->ClearRenderTargetView(outputRtv, DepthOfField::BokehMapClearValues, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

		// Bind the input ambient map to second texture table.
		mCommandList->SetGraphicsRootDescriptorTable(EBlurRootSignatureLayout::EBRS_Input, inputSrv);

		mCommandList->IASetVertexBuffers(0, 0, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(6, 1, 0, 0);

		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				output,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		);
	};

	auto blurCBAddress = mCurrFrameResource->BlurCB.Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(EBlurRootSignatureLayout::EBRS_BlurCB, blurCBAddress);
	mCommandList->SetGraphicsRootDescriptorTable(EBlurRootSignatureLayout::EBRS_NormalDepth, mGBuffer->NormalMapSrv());

	for (int i = 0; i < 1; ++i) {
		blurBokehMap(true);
		blurBokehMap(false);
	}

	mCommandList->SetPipelineState(mPSOs["dof"].Get());
	mCommandList->SetGraphicsRootSignature(mRootSignatures["dof"].Get());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto dofMapRtv = mDof->DofMapRtv();
	mCommandList->OMSetRenderTargets(1, &dofMapRtv, true, nullptr);

	mCommandList->SetGraphicsRootDescriptorTable(
		EDofRootSignatureLayout::EDOFRS_BackBuffer,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_BackBuffer0 + CurrentBackBufferIndex(), descSize)
	);
	mCommandList->SetGraphicsRootDescriptorTable(
		EDofRootSignatureLayout::EDOFRS_CocAndBokeh,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Coc, descSize)
	);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	const auto pBackBuffer = CurrentBackBuffer();
	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pDofMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST
		)
		};
		mCommandList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}

	mCommandList->CopyResource(pBackBuffer, pDofMap);

	{
		CD3DX12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pDofMap,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
		};
		mCommandList->ResourceBarrier(
			_countof(barriers),
			barriers
		);
	}
	
	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::ApplyMotionBlur() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["motionBlur"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["motionBlur"].Get());

	auto pDescHeap = mCbvSrvUavHeap.Get();
	UINT descSize = GetCbvSrvUavDescriptorSize();

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pMotionBlurMap = mMotionBlur->MotionMapResource();
	const auto pBackBuffer = CurrentBackBuffer();

	D3D12_RESOURCE_BARRIER beginBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
	};

	mCommandList->ResourceBarrier(
		_countof(beginBarriers),
		beginBarriers
	);

	const auto motionBlurRtv = mMotionBlur->MotionMapRtv();
	mCommandList->ClearRenderTargetView(motionBlurRtv, MotionBlur::ClearValues, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &motionBlurRtv, true, nullptr);

	mCommandList->SetGraphicsRootDescriptorTable(
		EMotionBlurRootSignatureLayout::EMBRS_Input,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_BackBuffer0 + CurrentBackBufferIndex(), descSize)
	);
	mCommandList->SetGraphicsRootDescriptorTable(
		EMotionBlurRootSignatureLayout::EMBRS_Depth,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Depth, descSize)
	);
	mCommandList->SetGraphicsRootDescriptorTable(
		EMotionBlurRootSignatureLayout::EMBRS_Velocity,
		D3D12Util::GetGpuHandle(pDescHeap, EReservedSrvs::ERS_Velocity, descSize)
	);

	float floatingValues[EMotionBlurRootConstantsLayout::EMBRC_Count - 1] = { mMotionBlurIntensity, mMotionBlurLimit, mMotionBlurDepthBias };
	mCommandList->SetGraphicsRoot32BitConstants(EMotionBlurRootSignatureLayout::EMBRS_Consts, _countof(floatingValues), floatingValues, 0);

	UINT integerValues[1] = { mMotionBlurNumSamples };
	mCommandList->SetGraphicsRoot32BitConstants(EMotionBlurRootSignatureLayout::EMBRS_Consts, _countof(integerValues), integerValues, EMotionBlurRootConstantsLayout::EMBRS_NumSamples);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 1, 0, 0);

	D3D12_RESOURCE_BARRIER middleBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pMotionBlurMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		)
	};

	mCommandList->ResourceBarrier(
		_countof(middleBarriers),
		middleBarriers
	);

	mCommandList->CopyResource(pBackBuffer, pMotionBlurMap);

	D3D12_RESOURCE_BARRIER endBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			pMotionBlurMap,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT
		)
	};

	mCommandList->ResourceBarrier(
		_countof(endBarriers),
		endBarriers
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawDebuggingInfo() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["debug"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignatures["default"].Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pCurrBackBuffer = CurrentBackBuffer();
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto pCurrBackBufferView = CurrentBackBufferView();
	mCommandList->OMSetRenderTargets(1, &pCurrBackBufferView, true, &DepthStencilView());

	const auto& passCB = mCurrFrameResource->PassCB;
	mCommandList->SetGraphicsRootDescriptorTable(EDefaultRootSignatureLayout::EDRS_ReservedTexMaps, mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawInstanced(6, 10, 0, 0);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

bool DxRenderer::DrawImGui() {
	CheckHRESULT(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	const auto pCurrBackBuffer = CurrentBackBuffer();
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto pCurrBackBufferView = CurrentBackBufferView();
	mCommandList->OMSetRenderTargets(1, &pCurrBackBufferView, true, nullptr);

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
		}
		

		ImGui::End();
	}
	{
		ImGui::Begin("Sub Panel");
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Post Pass")) {
			if (ImGui::TreeNode("TAA")) {
				ImGui::SliderFloat("Modulation Factor", &mTaaModulationFactor, 0.1f, 0.9f);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Motion Blur")) {
				ImGui::SliderFloat("Intensity", &mMotionBlurIntensity, 0.01f, 0.1f);
				ImGui::SliderFloat("Limit", &mMotionBlurLimit, 0.001f, 0.01f);
				ImGui::SliderFloat("Depth Bias", &mMotionBlurDepthBias, 0.001f, 0.01f);
				ImGui::SliderInt("Sample Count", reinterpret_cast<int*>(&mMotionBlurNumSamples), 1, 32);

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Depth of Field")) {
				ImGui::SliderFloat("Bokeh Radius", &mBokehRadius, 1.0f, 8.0f);
				ImGui::SliderFloat("Focus Range", &mFocusRange, 0.1f, 100.0f);
				ImGui::SliderFloat("Focusing Speed", &mFocusingSpeed, 1.0f, 100.0f);

				ImGui::TreePop();
			}
		}

		ImGui::End();
	}

	ImGui::Render();

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pCurrBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	CheckHRESULT(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}