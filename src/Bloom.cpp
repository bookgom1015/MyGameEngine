#include "Bloom.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"

using namespace Bloom;

namespace {
	const std::string BloomVS = "BloomVS";
	const std::string BloomPS = "BloomPS";

	const std::string ExtractHighlightsVS = "ExtractHighlightsVS";
	const std::string ExtractHighlightsPS = "ExtractHighlightsPS";
}

BloomClass::BloomClass() {
	mBloomMaps[0] = std::make_unique<GpuResource>();
	mBloomMaps[1] = std::make_unique<GpuResource>();
	mResultMap = std::make_unique<GpuResource>();
}

BOOL BloomClass::Initialize(
		ID3D12Device* device, ShaderManager*const manager, 
		UINT width, UINT height, UINT divider) {
	md3dDevice = device;
	mShaderManager = manager;

	mBloomMapWidth = width / divider;
	mBloomMapHeight = height / divider;

	mResultMapWidth = width;
	mResultMapHeight = height;

	mDivider = divider;

	mReducedViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mBloomMapWidth), static_cast<FLOAT>(mBloomMapHeight), 0.0f, 1.0f };
	mReducedScissorRect = { 0, 0, static_cast<INT>(mBloomMapWidth), static_cast<INT>(mBloomMapHeight) };

	mOriginalViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mResultMapWidth), static_cast<FLOAT>(mResultMapHeight), 0.0f, 1.0f };
	mOriginalScissorRect = { 0, 0, static_cast<INT>(mResultMapWidth), static_cast<INT>(mResultMapHeight) };

	CheckReturn(BuildResources());

	return true;
}

BOOL BloomClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"Bloom.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, BloomVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, BloomPS));
	}
	{
		const std::wstring actualPath = filePath + L"ExtractHighlights.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ExtractHighlightsVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ExtractHighlightsPS));
	}

	return true;
}

BOOL BloomClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ExtractHighlights::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ExtractHighlights::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ExtractHighlights::RootSignatureLayout::EC_Consts].InitAsConstants(ExtractHighlights::RootConstatLayout::Count, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[PipelineState::E_Extract]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ApplyBloom::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[ApplyBloom::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ApplyBloom::RootSignatureLayout::ESI_Bloom].InitAsDescriptorTable(1, &texTable1);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[PipelineState::E_Bloom]));
	}

	return true;
}

BOOL BloomClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC extHlightsPsoDesc = quadPsoDesc;
	extHlightsPsoDesc.pRootSignature = mRootSignatures[PipelineState::E_Extract].Get();
	{
		auto vs = mShaderManager->GetDxcShader(ExtractHighlightsVS);
		auto ps = mShaderManager->GetDxcShader(ExtractHighlightsPS);
		extHlightsPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		extHlightsPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	extHlightsPsoDesc.RTVFormats[0] = HDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&extHlightsPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_Extract])));
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomPsoDesc = quadPsoDesc;
	bloomPsoDesc.pRootSignature = mRootSignatures[PipelineState::E_Bloom].Get();
	{
		auto vs = mShaderManager->GetDxcShader(BloomVS);
		auto ps = mShaderManager->GetDxcShader(BloomPS);
		bloomPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		bloomPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	bloomPsoDesc.RTVFormats[0] = HDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_Bloom])));

	return true;
}

void BloomClass::ExtractHighlights(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		FLOAT threshold) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_Extract].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[PipelineState::E_Extract].Get());

	cmdList->RSSetViewports(1, &mReducedViewport);
	cmdList->RSSetScissorRects(1, &mReducedScissorRect);

	const auto bloomMap0 = mBloomMaps[0].get();
	bloomMap0->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	auto bloomMapRtv = mhBloomMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(bloomMapRtv, ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &bloomMapRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(ExtractHighlights::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);

	FLOAT values[ExtractHighlights::RootConstatLayout::Count] = { threshold };
	cmdList->SetGraphicsRoot32BitConstants(ExtractHighlights::RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	bloomMap0->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void BloomClass::Bloom(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_Bloom].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[PipelineState::E_Bloom].Get());

	cmdList->RSSetViewports(1, &mOriginalViewport);
	cmdList->RSSetScissorRects(1, &mOriginalScissorRect);

	cmdList->ClearRenderTargetView(mhResultMapCpuRtv, Bloom::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhResultMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(ApplyBloom::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(ApplyBloom::RootSignatureLayout::ESI_Bloom, mhBloomMapGpuSrvs[0]);
	 
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void BloomClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhBloomMapCpuSrvs[0] = hCpu.Offset(1, descSize);
	mhBloomMapGpuSrvs[0] = hGpu.Offset(1, descSize);
	mhBloomMapCpuRtvs[0] = hCpuRtv.Offset(1, rtvDescSize);

	mhBloomMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhBloomMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhBloomMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	mhResultMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

BOOL BloomClass::OnResize(UINT width, UINT height) {
	if ((mResultMapWidth != width) || (mResultMapHeight != height)) {
		mBloomMapWidth = width / mDivider;
		mBloomMapHeight = height / mDivider;

		mResultMapWidth = width;
		mResultMapHeight = height;

		mReducedViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mBloomMapWidth), static_cast<FLOAT>(mBloomMapHeight), 0.0f, 1.0f };
		mReducedScissorRect = { 0, 0, static_cast<INT>(mBloomMapWidth), static_cast<INT>(mBloomMapHeight) };

		mOriginalViewport = { 0.0f, 0.0f, static_cast<FLOAT>(mResultMapWidth), static_cast<FLOAT>(mResultMapHeight), 0.0f, 1.0f };
		mOriginalScissorRect = { 0, 0, static_cast<INT>(mResultMapWidth), static_cast<INT>(mResultMapHeight) };

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void BloomClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = HDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = HDR_FORMAT;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (INT i = 0; i < 2; ++i) {
		auto bloomMap = mBloomMaps[i]->Resource();
		md3dDevice->CreateShaderResourceView(bloomMap, &srvDesc, mhBloomMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(bloomMap, &rtvDesc, mhBloomMapCpuRtvs[i]);
	}

	md3dDevice->CreateRenderTargetView(mResultMap->Resource(), &rtvDesc, mhResultMapCpuRtv);
}

BOOL BloomClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = HDR_FORMAT;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(HDR_FORMAT, ClearValues);
	
	rscDesc.Width = mBloomMapWidth;
	rscDesc.Height = mBloomMapHeight;
	for (INT i = 0; i < 2; ++i) {
		std::wstringstream wsstream;
		wsstream << "BloomMap_" << i;
		CheckReturn(mBloomMaps[i]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			wsstream.str().c_str()
		));
	}
	

	rscDesc.Width = mResultMapWidth;
	rscDesc.Height = mResultMapHeight;
	CheckReturn(mResultMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		L"BloomResultMap"
	));

	return true;
}