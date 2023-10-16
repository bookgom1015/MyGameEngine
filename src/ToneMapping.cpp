#include "ToneMapping.h"
#include "Logger.h"
#include "GpuResource.h"
#include "D3D12Util.h"
#include "ShaderManager.h"

using namespace ToneMapping;

namespace {
	const std::string ToneMappingVS = "ToneMappingVS";
	const std::string ToneMappingPS = "ToneMappingPS";

	const std::string JustResolvingPS = "JustResolvingPS";
}

ToneMappingClass::ToneMappingClass() {
	mIntermediateMap = std::make_unique<GpuResource>();
}

bool ToneMappingClass::Initialize(
		ID3D12Device* device, ShaderManager* const manager, 
		UINT width, UINT height, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mBackBufferFormat = backBufferFormat;

	CheckReturn(BuildResources());

	return true;
}

bool ToneMappingClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"ToneMapping.hlsl";

	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, ToneMappingVS));
	{		
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, ToneMappingPS));
	}
	{
		DxcDefine defines[] = {
			{ L"NON_HDR", L"1" }
		};
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(psInfo, JustResolvingPS));
	}

	return true;
}

bool ToneMappingClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[RootSignatureLayout::EC_Cosnts].InitAsConstants(RootConstantsLayout::Count, 0);
	slotRootParameter[RootSignatureLayout::ESI_Intermediate].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool ToneMappingClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(ToneMappingVS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = mBackBufferFormat;

	{
		auto ps = mShaderManager->GetDxcShader(ToneMappingPS);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ToneMapping])));

	{
		auto ps = mShaderManager->GetDxcShader(JustResolvingPS);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_JustResolving])));

	return true;
}

void ToneMappingClass::Resolve(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_JustResolving].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Intermediate, mhIntermediateMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void ToneMappingClass::Resolve(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		float exposure) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ToneMapping].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);
	
	float values[RootConstantsLayout::Count] = { exposure };
	cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Cosnts, _countof(values), values, 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Intermediate, mhIntermediateMapGpuSrv);
	
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
	
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void ToneMappingClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhIntermediateMapCpuSrv = hCpuSrv;
	mhIntermediateMapGpuSrv = hGpuSrv;
	mhIntermediateMapCpuRtv = hCpuRtv;

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);
	hCpuRtv.Offset(1, descSize);

	BuildDescriptors();
}

bool ToneMappingClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void ToneMappingClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = D3D12Util::HDRMapFormat;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	rtvDesc.Format = D3D12Util::HDRMapFormat;

	md3dDevice->CreateShaderResourceView(mIntermediateMap->Resource(), &srvDesc, mhIntermediateMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mIntermediateMap->Resource(), &rtvDesc, mhIntermediateMapCpuRtv);
}

bool ToneMappingClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.Format = D3D12Util::HDRMapFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(D3D12Util::HDRMapFormat, ClearValues);

	CheckReturn(mIntermediateMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		L"IntermediateMap"
	));

	return true;
}
