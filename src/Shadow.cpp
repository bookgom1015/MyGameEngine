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
	const CHAR* const GS_ZDepth = "GS_ZDepth";
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
		auto gsInfo = D3D12ShaderInfo(actualPath.c_str(), L"GS", L"gs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ZDepth));
		CheckReturn(mShaderManager->CompileShader(gsInfo, GS_ZDepth));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ZDepth));
	}
	{
		const std::wstring actualPath = filePath + L"DrawShadowCS.hlsl";
		auto csInfo = D3D12ShaderInfo(actualPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_Shadow));
	}

	return TRUE;
}

BOOL ShadowClass::BuildRootSignature(const StaticSamplers& samplers) {
	// Draw z-depth
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
	// Draw shadow
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Shadow::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[6];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::Shadow::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::Shadow::EC_Consts].InitAsConstants(RootSignature::Shadow::RootConstant::Count, 1);
		slotRootParameter[RootSignature::Shadow::ESI_Position].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::Shadow::ESI_ZDepth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::Shadow::ESI_ZDepthCube].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::Shadow::ESI_VSDepthCube].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::Shadow::ESI_FaceIDCube].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::Shadow::EUO_Shadow].InitAsDescriptorTable(1, &texTables[5]);

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
	// Draw z-depth
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), Shadow::ZDepthMapFormat);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ZDepth].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ZDepth);
			auto gs = mShaderManager->GetDxcShader(GS_ZDepth);
			auto ps = mShaderManager->GetDxcShader(PS_ZDepth);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = VSDepthCubeMapFormat;
		psoDesc.RTVFormats[1] = FaceIDCubeMapFormat;
		psoDesc.RasterizerState.DepthBias = 100000;
		psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		psoDesc.RasterizerState.DepthBiasClamp = 0.1f;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ZDepth])));
	}
	// Draw shadow on compute shader
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
		BOOL point,
		UINT index) {
	DrawZDepth(cmdList, cb_pass, cb_obj, cb_mat, objCBByteSize, matCBByteSize, si_texMaps, point, index, ritems);
	DrawShadow(cmdList, cb_pass, si_pos, index);
}

void ShadowClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT dsvDescSize, UINT rtvDescSize) {
	mhCpuDescs[Descriptor::ESI_ZDepth] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_ZDepth] = hGpu.Offset(1, descSize);

	mhCpuDescs[Descriptor::ESI_ZDepthCube] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_ZDepthCube] = hGpu.Offset(1, descSize);

	mhCpuDescs[Descriptor::ESI_FaceIDCube] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_FaceIDCube] = hGpu.Offset(1, descSize);

	mhCpuDescs[Descriptor::ESI_Shadow] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_Shadow] = hGpu.Offset(1, descSize);
	mhCpuDescs[Descriptor::EUO_Shadow] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::EUO_Shadow] = hGpu.Offset(1, descSize);

	mhCpuDsvs[Descriptor::DSV::EDS_ZDepth]		= hCpuDsv.Offset(1, dsvDescSize);
	mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube]	= hCpuDsv.Offset(1, dsvDescSize);

	mhCpuDescs[Descriptor::ESI_VSDepthCube]		= hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::ESI_VSDepthCube]		= hGpu.Offset(1, descSize);
	mhCpuRtvs[Descriptor::RTV::ERT_VSDepthCube] = hCpuRtv.Offset(1, rtvDescSize);

	mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube] = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

void ShadowClass::BuildDescriptors() {
	// Z-depth & Z-depth cube
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.Texture2D.PlaneSlice = 0;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format = ZDepthMapFormat;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Texture2D.MipSlice = 0;

			const auto& depth = mShadowMaps[Resource::E_ZDepth]->Resource();

			md3dDevice->CreateDepthStencilView(depth, &dsvDesc, mhCpuDsvs[Descriptor::DSV::EDS_ZDepth]);
			md3dDevice->CreateShaderResourceView(depth, &srvDesc, mhCpuDescs[Descriptor::ESI_ZDepth]);
		}
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = 6;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Format = ZDepthMapFormat;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Texture2DArray.ArraySize = 6;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;

			const auto& depthCube = mShadowMaps[Resource::E_ZDepthCube]->Resource();

			md3dDevice->CreateDepthStencilView(depthCube, &dsvDesc, mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube]);
			md3dDevice->CreateShaderResourceView(depthCube, &srvDesc, mhCpuDescs[Descriptor::ESI_ZDepthCube]);
		}
	}
	// Shadow
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = ShadowMapFormat;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = ShadowMapFormat;

		const auto& shadow = mShadowMaps[Resource::E_Shadow]->Resource();

		md3dDevice->CreateUnorderedAccessView(shadow, nullptr, &uavDesc, mhCpuDescs[Descriptor::EUO_Shadow]);
		md3dDevice->CreateShaderResourceView(shadow, &srvDesc, mhCpuDescs[Descriptor::ESI_Shadow]);
	}
	// VS-depth cube
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Format = VSDepthCubeMapFormat;
		srvDesc.Texture2DArray.ArraySize = 6;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = VSDepthCubeMapFormat;
		rtvDesc.Texture2DArray.ArraySize = 6;
		rtvDesc.Texture2DArray.FirstArraySlice = 0;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		const auto& vsDepthCube = mShadowMaps[Resource::E_VSDepthCube]->Resource();

		md3dDevice->CreateRenderTargetView(vsDepthCube, &rtvDesc, mhCpuRtvs[Descriptor::RTV::ERT_VSDepthCube]);
		md3dDevice->CreateShaderResourceView(vsDepthCube, &srvDesc, mhCpuDescs[Descriptor::ESI_VSDepthCube]);
	}
	// Face id cube
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Format = FaceIDCubeMapFormat;
		srvDesc.Texture2DArray.ArraySize = 6;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = FaceIDCubeMapFormat;
		rtvDesc.Texture2DArray.ArraySize = 6;
		rtvDesc.Texture2DArray.FirstArraySlice = 0;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		const auto& faceIDCube = mShadowMaps[Resource::E_FaceIDCube]->Resource();

		md3dDevice->CreateRenderTargetView(faceIDCube, &rtvDesc, mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube]);
		md3dDevice->CreateShaderResourceView(faceIDCube, &srvDesc, mhCpuDescs[Descriptor::ESI_FaceIDCube]);
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
	// Z-Depth cube map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = ZDepthMapFormat;
		texDesc.DepthOrArraySize = 6;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = ZDepthMapFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		CheckReturn(mShadowMaps[Resource::E_ZDepthCube]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&optClear,
			L"Shadow_ZDepthCubeMap"
		));
	}
	// Z-Depth map in view space
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = VSDepthCubeMapFormat;
		texDesc.DepthOrArraySize = 6;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_CLEAR_VALUE optClear(VSDepthCubeMapFormat, VSDepthCubeMapFormatClearValues);

		CheckReturn(mShadowMaps[Resource::E_VSDepthCube]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			L"Shadow_VSDepthCubeMap"
		));
	}
	// FaceID cube map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = FaceIDCubeMapFormat;
		texDesc.DepthOrArraySize = 6;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_CLEAR_VALUE optClear(FaceIDCubeMapFormat, FaceIDCubeMapClearValues);

		CheckReturn(mShadowMaps[Resource::E_FaceIDCube]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			L"Shadow_FaceIDCubeMap"
		));
	}

	return TRUE;
}

void ShadowClass::DrawZDepth(
		ID3D12GraphicsCommandList* cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize, UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		BOOL point, UINT index,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ZDepth].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ZDepth].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	auto& vsDepth = mShadowMaps[Resource::E_VSDepthCube];
	auto& faceID = mShadowMaps[Resource::E_FaceIDCube];
	auto& shadow = mShadowMaps[point ? Resource::E_ZDepthCube : Resource::E_ZDepth];

	if (point) {
		faceID->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		vsDepth->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto& rtv_vs = mhCpuRtvs[Descriptor::RTV::ERT_VSDepthCube];
		cmdList->ClearRenderTargetView(rtv_vs, VSDepthCubeMapFormatClearValues, 0, nullptr);

		auto& rtv_face = mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube];
		cmdList->ClearRenderTargetView(rtv_face, FaceIDCubeMapClearValues, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[Descriptor::RTV::Count] = { rtv_vs, rtv_face };

		auto& dsv = mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube];
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		cmdList->OMSetRenderTargets(_countof(rtvs), rtvs, FALSE, &dsv);
	}
	else {
		auto& shadow = mShadowMaps[point ? Resource::E_ZDepthCube : Resource::E_ZDepth];
		shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto& dsv = mhCpuDsvs[Descriptor::DSV::EDS_ZDepth];
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	}

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Pass, cb_pass);

	UINT values[RootSignature::ZDepth::RootConstant::Count] = { index };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::ZDepth::EC_Consts, _countof(values), values, 0);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ZDepth::ESI_TexMaps, si_texMaps);

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

	if (point) {
		vsDepth->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		faceID->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
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
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_ZDepthCube, mhGpuDescs[Descriptor::ESI_ZDepthCube]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_VSDepthCube, mhGpuDescs[Descriptor::ESI_VSDepthCube]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_FaceIDCube, mhGpuDescs[Descriptor::ESI_FaceIDCube]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::EUO_Shadow, mhGpuDescs[Descriptor::EUO_Shadow]);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mClientWidth), Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mClientHeight), Default::ThreadGroup::Height), 1);

	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, shadow);
}