#include "ToneMapping.h"
#include "Logger.h"
#include "GpuResource.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"

using namespace ToneMapping;

namespace {
	const CHAR* const VS_ToneMapping = "VS_ToneMapping";
	const CHAR* const PS_ToneMapping = "PS_ToneMapping";

	const CHAR* const PS_JustResolving = "PS_JustResolving";
}

ToneMappingClass::ToneMappingClass() {
	mIntermediateMap = std::make_unique<GpuResource>();
}

BOOL ToneMappingClass::Initialize(ID3D12Device* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL ToneMappingClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"ToneMapping.hlsl";

	const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ToneMapping));
	{		
		const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ToneMapping));
	}
	{
		DxcDefine defines[] = {
			{ L"NON_HDR", L"1" }
		};
		const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_JustResolving));
	}

	return TRUE;
}

BOOL ToneMappingClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count] = {};
	slotRootParameter[RootSignature::Default::EC_Cosnts].InitAsConstants(RootConstant::Default::Count, 0);
	slotRootParameter[RootSignature::Default::ESI_Intermediate].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL ToneMappingClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto vs = mShaderManager->GetDxcShader(VS_ToneMapping);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = SDR_FORMAT;

	{
		const auto ps = mShaderManager->GetDxcShader(PS_ToneMapping);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ToneMapping])));

	{
		const auto ps = mShaderManager->GetDxcShader(PS_JustResolving);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_JustResolving])));

	return TRUE;
}

void ToneMappingClass::Resolve(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_JustResolving].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Intermediate, mhIntermediateMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void ToneMappingClass::Resolve(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		FLOAT exposure) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ToneMapping].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);
	
	RootConstant::Default::Struct rc;
	rc.gExposure = exposure;

	std::array<std::uint32_t, RootConstant::Default::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

	cmdList->SetGraphicsRoot32BitConstants(RootSignature::Default::EC_Cosnts, RootConstant::Default::Count, consts.data(), 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Intermediate, mhIntermediateMapGpuSrv);
	
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void ToneMappingClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhIntermediateMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhIntermediateMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhIntermediateMapCpuRtv = hCpuRtv.Offset(1, descSize);
}

BOOL ToneMappingClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = ToneMapping::IntermediateMapFormat;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	rtvDesc.Format = ToneMapping::IntermediateMapFormat;

	md3dDevice->CreateShaderResourceView(mIntermediateMap->Resource(), &srvDesc, mhIntermediateMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mIntermediateMap->Resource(), &rtvDesc, mhIntermediateMapCpuRtv);

	return TRUE;
}

BOOL ToneMappingClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

BOOL ToneMappingClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.Format = ToneMapping::IntermediateMapFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	const CD3DX12_CLEAR_VALUE optClear(ToneMapping::IntermediateMapFormat, IntermediateMapClearValues);

	CheckReturn(mIntermediateMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		L"ToneMapping_IntermediateMap"
	));

	return TRUE;
}
