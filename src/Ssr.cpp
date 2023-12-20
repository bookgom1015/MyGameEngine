#include "Ssr.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

using namespace Ssr;

namespace {
	const std::string BuildingSsrVS = "BuildingSsrVS";
	const std::string BuildingSsrPS = "BuildingSsrPS";
}

SsrClass::SsrClass() {
	mSsrMaps[0] = std::make_unique<GpuResource>();
	mSsrMaps[1] = std::make_unique<GpuResource>();	
}

BOOL SsrClass::Initialize(
		ID3D12Device* device, ShaderManager*const manager, 
		UINT width, UINT height, UINT divider) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mReducedWidth = width / divider;
	mReducedHeight = height / divider;

	mDivider = divider;

	mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mReducedWidth), static_cast<FLOAT>(mReducedHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<INT>(mReducedWidth), static_cast<INT>(mReducedHeight) };

	CheckReturn(BuildResources());

	return true;
}

BOOL SsrClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"BuildingSsr.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, BuildingSsrVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, BuildingSsrPS));

	return true;
}

BOOL SsrClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[4];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);

	slotRootParameter[RootSignatureLayout::ECB_Ssr].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));	

	return true;
}

BOOL SsrClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(BuildingSsrVS);
		auto ps = mShaderManager->GetDxcShader(BuildingSsrPS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = SsrMapFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void SsrClass::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
	CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
	UINT descSize, UINT rtvDescSize) {
	mhSsrMapCpuSrvs[0] = hCpu;
	mhSsrMapGpuSrvs[0] = hGpu;
	mhSsrMapCpuRtvs[0] = hCpuRtv;
	mhSsrMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhSsrMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhSsrMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

BOOL SsrClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mReducedWidth = width / mDivider;
		mReducedHeight = height / mDivider;

		mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mReducedWidth), static_cast<FLOAT>(mReducedHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<INT>(mReducedWidth), static_cast<INT>(mReducedHeight) };

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void SsrClass::Build(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		GpuResource*const backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto ssrMap = mSsrMaps[0].get();

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	const auto rtv = mhSsrMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(rtv, Ssr::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &rtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Ssr, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Depth, si_depth);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SsrClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = SsrMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = SsrMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (INT i = 0; i < 2; ++i) {
		md3dDevice->CreateShaderResourceView(mSsrMaps[i]->Resource(), &srvDesc, mhSsrMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(mSsrMaps[i]->Resource(), &rtvDesc, mhSsrMapCpuRtvs[i]);
	}
}

BOOL SsrClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = SsrMapFormat;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	{
		rscDesc.Width = mReducedWidth;
		rscDesc.Height = mReducedHeight;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_CLEAR_VALUE optClear(SsrMapFormat, ClearValues);
		for (INT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << "SsrMap_" << i;

			CheckReturn(mSsrMaps[i]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				wsstream.str().c_str()
			));
		}
	}

	return true;
}