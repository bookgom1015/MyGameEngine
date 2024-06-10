#include "Shadow.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "MathHelper.h"
#include "D3D12Util.h"
#include "Vertex.h"

using namespace Shadow;

namespace {
	const CHAR* const VS_ZDepth = "VS_ZDepth";
	const CHAR* const PS_ZDepth = "PS_ZDepth";

	const CHAR* const CS_Shadow = "CS_Shadow";
}

ShadowClass::ShadowClass() {
	for (UINT i = 0; i < Resource::Count; ++i) 
		mShadowMaps[i] = std::make_unique<GpuResource>();
}

BOOL ShadowClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT clientW, UINT clientH,	UINT texW, UINT texH) {
	md3dDevice = device;
	mShaderManager = manager;

	mClientWidth = clientW;
	mClientHeight = clientH;

	mTexWidth = texW;
	mTexHeight = texH;

	mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(texW), static_cast<FLOAT>(texH), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<INT>(texW), static_cast<INT>(texH) };

	CheckReturn(BuildResources());

	return TRUE;
}

BOOL ShadowClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"DrawZDepth.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ZDepth));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ZDepth));
	}
	{
		const std::wstring actualPath = filePath + L"DrawShadow.hlsl";
		auto shaderInfo = D3D12ShaderInfo(actualPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_Shadow));
	}

	return TRUE;
}

BOOL ShadowClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ZDepth::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0, 0);

		slotRootParameter[RootSignature::ZDepth::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ZDepth::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[RootSignature::ZDepth::ECB_Mat].InitAsConstantBufferView(2);
		slotRootParameter[RootSignature::ZDepth::EC_Consts].InitAsConstants(RootSignature::ZDepth::RootConstant::Count, 3);
		slotRootParameter[RootSignature::ZDepth::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ZDepth]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Shadow::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[3];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::Shadow::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::Shadow::EC_Consts].InitAsConstants(RootSignature::Shadow::RootConstant::Count, 1);
		slotRootParameter[RootSignature::Shadow::ESI_Position].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::Shadow::ESI_ZDepth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::Shadow::EUO_Shadow].InitAsDescriptorTable(1, &texTables[2]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_Shadow]));
	}

	return TRUE;
}

BOOL ShadowClass::BuildPSO() {
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), DepthStencilBuffer::BufferFormat);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ZDepth].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ZDepth);
			auto ps = mShaderManager->GetDxcShader(PS_ZDepth);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 0;
		psoDesc.RasterizerState.DepthBias = 100000;
		psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		psoDesc.RasterizerState.DepthBiasClamp = 0.1f;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ZDepth])));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_Shadow].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			auto cs = mShaderManager->GetDxcShader(CS_Shadow);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_Shadow])));
	}

	return TRUE;
}

void ShadowClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		const std::vector<RenderItem*>& ritems,
		UINT index) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ZDepth].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ZDepth].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mShadowMaps[Resource::E_ZDepth]->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);	

	cmdList->ClearDepthStencilView(mhCpuDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, false, &mhCpuDsv);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Pass, cb_pass);

	UINT values[RootSignature::ZDepth::RootConstant::Count] = { index };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::ZDepth::EC_Consts, _countof(values), values, 0);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ZDepth::ESI_TexMaps, si_texMaps);

	DrawRenderItems(cmdList, ritems, cb_obj, cb_mat, objCBByteSize, matCBByteSize);

	mShadowMaps[Resource::E_ZDepth]->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);

	DrawShadow(cmdList, cb_pass, si_pos, index);
}

void ShadowClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
		UINT descSize, UINT dsvDescSize) {
	mhCpuDescs[Descriptor::ESI_ZDepth] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_ZDepth] = hGpu.Offset(1, descSize);

	mhCpuDescs[Descriptor::ESI_Shadow] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_Shadow] = hGpu.Offset(1, descSize);
	mhCpuDescs[Descriptor::EUO_Shadow] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::EUO_Shadow] = hGpu.Offset(1, descSize);

	mhCpuDsv = hCpuDsv.Offset(1, dsvDescSize);

	BuildDescriptors();
}

void ShadowClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;

	{
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = ZDepthMapFormat;
		dsvDesc.Texture2D.MipSlice = 0;

		md3dDevice->CreateDepthStencilView(mShadowMaps[Resource::E_ZDepth]->Resource(), &dsvDesc, mhCpuDsv);
		md3dDevice->CreateShaderResourceView(mShadowMaps[Resource::E_ZDepth]->Resource(), &srvDesc, mhCpuDescs[Descriptor::ESI_ZDepth]);
	}
	{
		srvDesc.Format = ShadowMapFormat;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = ShadowMapFormat;

		md3dDevice->CreateUnorderedAccessView(mShadowMaps[Resource::E_Shadow]->Resource(), nullptr, &uavDesc, mhCpuDescs[Descriptor::EUO_Shadow]);
		md3dDevice->CreateShaderResourceView(mShadowMaps[Resource::E_Shadow]->Resource(), &srvDesc, mhCpuDescs[Descriptor::ESI_Shadow]);
	}
}

BOOL ShadowClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	
	// Shadow map
	{
		texDesc.Width = mClientWidth;
		texDesc.Height = mClientHeight;
		texDesc.Format = ShadowMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CheckReturn(mShadowMaps[Resource::E_Shadow]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Shadow_ShadowMap"
		));
	}
	// Z-Depth map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = ZDepthMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = ZDepthMapFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		CheckReturn(mShadowMaps[Resource::E_ZDepth]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&optClear,
			L"Shadow_ZDepthMap"
		));
	}

	return TRUE;
}

void ShadowClass::DrawRenderItems(
		ID3D12GraphicsCommandList* cmdList, 
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj, 
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize) {
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cb_obj + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = cb_mat + ri->Material->MatCBIndex * matCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void ShadowClass::DrawShadow(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		UINT index) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_Shadow].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_Shadow].Get());

	const auto& shadow = mShadowMaps[Resource::E_Shadow].get();

	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, shadow);

	cmdList->SetComputeRootConstantBufferView(RootSignature::Shadow::ECB_Pass, cb_pass);

	UINT values[RootSignature::Shadow::RootConstant::Count] = { index };
	cmdList->SetComputeRoot32BitConstants(RootSignature::Shadow::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_Position, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_ZDepth, mhGpuDescs[Descriptor::ESI_ZDepth]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::EUO_Shadow, mhGpuDescs[Descriptor::EUO_Shadow]);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mViewport.Width), Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mViewport.Height), Default::ThreadGroup::Height), 1);

	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, shadow);
}