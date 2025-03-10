#include "DirectX/Shading/Shadow.h"
#include "Common/Debug/Logger.h"
#include "Common/Mesh/Vertex.h"
#include "Common/Render/RenderItem.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"
#include "DxMesh.h"

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

BOOL ShadowClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT clientW, UINT clientH, UINT texW, UINT texH) {
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
		const std::wstring actualPath = filePath + L"ZDepth.hlsl";
		const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		const auto gsInfo = D3D12ShaderInfo(actualPath.c_str(), L"GS", L"gs_6_3");
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ZDepth));
		CheckReturn(mShaderManager->CompileShader(gsInfo, GS_ZDepth));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ZDepth));
	}
	{
		const std::wstring actualPath = filePath + L"ShadowCS.hlsl";
		const auto csInfo = D3D12ShaderInfo(actualPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_Shadow));
	}

	return TRUE;
}

BOOL ShadowClass::BuildRootSignature(const StaticSamplers& samplers) {
	D3D12Util::Descriptor::RootSignature::Builder builder;
	// Z-depth
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ZDepth::Count] = {};
		slotRootParameter[RootSignature::ZDepth::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ZDepth::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[RootSignature::ZDepth::ECB_Mat].InitAsConstantBufferView(2);
		slotRootParameter[RootSignature::ZDepth::EC_Consts].InitAsConstants(RootConstant::ZDepth::Count, 3);
		slotRootParameter[RootSignature::ZDepth::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_ZDepth]), L"Shadow_RS_ZDepth");
	}
	// Shadow
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[6] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Shadow::Count] = {};
		slotRootParameter[RootSignature::Shadow::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::Shadow::EC_Consts].InitAsConstants(RootConstant::Shadow::Count, 1);
		slotRootParameter[RootSignature::Shadow::ESI_Position].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Shadow::ESI_ZDepth].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::Shadow::ESI_ZDepthCube].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Shadow::ESI_FaceIDCube].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Shadow::EUO_Shadow].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::Shadow::EUO_Debug].InitAsDescriptorTable(		1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_Shadow]), L"Shadow_RS_Shadow");
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL ShadowClass::BuildPSO() {
	D3D12Util::Descriptor::PipelineState::Builder builder;
	// Draw z-depth
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), Shadow::ZDepthMapFormat);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ZDepth].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_ZDepth);
			const auto gs = mShaderManager->GetDxcShader(GS_ZDepth);
			const auto ps = mShaderManager->GetDxcShader(PS_ZDepth);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = FaceIDCubeMapFormat;
		psoDesc.RasterizerState.DepthBias = 100000;
		psoDesc.RasterizerState.SlopeScaledDepthBias = 1.f;
		psoDesc.RasterizerState.DepthBiasClamp = 0.1f;

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ZDepth]), L"Shadow_GPS_ZDepth");
	}
	// Draw shadow on compute shader
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_Shadow].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			const auto cs = mShaderManager->GetDxcShader(CS_Shadow);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_Shadow]), L"Shadow_CPS_Shadow");
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void ShadowClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		const std::vector<RenderItem*>& ritems,
		GpuResource* const dst_zdepth,
		GpuResource* const dst_faceIDCube,
		UINT lightType,
		UINT index) {
	if (lightType == LightType::E_Tube || lightType == LightType::E_Rect) return;

	BOOL needCubemap = lightType == LightType::E_Point || lightType == LightType::E_Spot;

	DrawZDepth(cmdList, cb_pass, cb_obj, cb_mat, objCBByteSize, matCBByteSize, si_texMaps, needCubemap, index, ritems);
	CopyZDepth(cmdList, dst_zdepth, dst_faceIDCube, needCubemap);
	DrawShadow(cmdList, cb_pass, si_pos, index);
}

void ShadowClass::AllocateDescriptors(
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

	mhCpuDescs[Descriptor::EUO_Debug] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptor::EUO_Debug] = hGpu.Offset(1, descSize);

	mhCpuDsvs[Descriptor::DSV::EDS_ZDepth]		= hCpuDsv.Offset(1, dsvDescSize);
	mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube]	= hCpuDsv.Offset(1, dsvDescSize);

	mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube] = hCpuRtv.Offset(1, rtvDescSize);
}

BOOL ShadowClass::BuildDescriptors() {
	D3D12Util::Descriptor::View::Builder builder;
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

			builder.Enqueue(depth, dsvDesc, mhCpuDsvs[Descriptor::DSV::EDS_ZDepth]);
			builder.Enqueue(depth, srvDesc, mhCpuDescs[Descriptor::ESI_ZDepth]);
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

			builder.Enqueue(depthCube, dsvDesc, mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube]);
			builder.Enqueue(depthCube, srvDesc, mhCpuDescs[Descriptor::ESI_ZDepthCube]);
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

		builder.Enqueue(shadow, uavDesc, mhCpuDescs[Descriptor::EUO_Shadow]);
		builder.Enqueue(shadow, srvDesc, mhCpuDescs[Descriptor::ESI_Shadow]);
	}
	// Debug
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DebugMapFormat;

		const auto& debug = mShadowMaps[Resource::E_Debug]->Resource();

		builder.Enqueue(debug, uavDesc, mhCpuDescs[Descriptor::EUO_Debug]);
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

		builder.Enqueue(faceIDCube, rtvDesc, mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube]);
		builder.Enqueue(faceIDCube, srvDesc, mhCpuDescs[Descriptor::ESI_FaceIDCube]);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
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
	
	D3D12_CLEAR_VALUE zdepthOptClear;
	zdepthOptClear.Format = ZDepthMapFormat;
	zdepthOptClear.DepthStencil.Depth = 1.f;
	zdepthOptClear.DepthStencil.Stencil = 0;

	CD3DX12_CLEAR_VALUE faceIDOptClear(FaceIDCubeMapFormat, FaceIDCubeMapClearValues);

	D3D12Util::Descriptor::Resource::Builder builder;
	// Shadow map
	{
		texDesc.Width = mClientWidth;
		texDesc.Height = mClientHeight;
		texDesc.Format = ShadowMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		builder.Enqueue(
			mShadowMaps[Resource::E_Shadow].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Shadow_ShadowMap"
		);
	}
	// Debug map
	{
		texDesc.Width = mClientWidth;
		texDesc.Height = mClientHeight;
		texDesc.Format = DebugMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		builder.Enqueue(
			mShadowMaps[Resource::E_Debug].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			L"Shadow_DebugMap"
		);
	}
	// Z-Depth map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = ZDepthMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		

		builder.Enqueue(
			mShadowMaps[Resource::E_ZDepth].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&zdepthOptClear,
			L"Shadow_ZDepthMap"
		);
	}
	// Z-Depth cube map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = ZDepthMapFormat;
		texDesc.DepthOrArraySize = 6;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		builder.Enqueue(
			mShadowMaps[Resource::E_ZDepthCube].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&zdepthOptClear,
			L"Shadow_ZDepthCubeMap"
		);
	}
	// FaceID cube map
	{
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.Format = FaceIDCubeMapFormat;
		texDesc.DepthOrArraySize = 6;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_CLEAR_VALUE optClear(FaceIDCubeMapFormat, FaceIDCubeMapClearValues);

		builder.Enqueue(
			mShadowMaps[Resource::E_FaceIDCube].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&faceIDOptClear,
			L"Shadow_FaceIDCubeMap"
		);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void ShadowClass::DrawZDepth(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize, UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		BOOL needCubemap, UINT index,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ZDepth].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ZDepth].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	auto& faceID = mShadowMaps[Resource::E_FaceIDCube];
	auto& shadow = mShadowMaps[needCubemap ? Resource::E_ZDepthCube : Resource::E_ZDepth];

	if (needCubemap) {
		faceID->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto& rtv = mhCpuRtvs[Descriptor::RTV::ERT_FaceIDCube];
		cmdList->ClearRenderTargetView(rtv, FaceIDCubeMapClearValues, 0, nullptr);

		auto& dsv = mhCpuDsvs[Descriptor::DSV::EDS_ZDepthCube];
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	}
	else {
		shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto& dsv = mhCpuDsvs[Descriptor::DSV::EDS_ZDepth];
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	}

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Pass, cb_pass);

	RootConstant::ZDepth::Struct rc;
	rc.gLightIndex = index;

	std::array<std::uint32_t, RootConstant::ZDepth::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::ZDepth::Struct));

	cmdList->SetGraphicsRoot32BitConstants(RootSignature::ZDepth::EC_Consts, RootConstant::ZDepth::Count, consts.data(), 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ZDepth::ESI_TexMaps, si_texMaps);

	for (UINT i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cb_obj + static_cast<UINT64>(ri->ObjCBIndex) * static_cast<UINT64>(objCBByteSize);
		cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = cb_mat + static_cast<UINT64>(ri->Material->MatCBIndex) * static_cast<UINT64>(matCBByteSize);
			cmdList->SetGraphicsRootConstantBufferView(RootSignature::ZDepth::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	if (needCubemap) faceID->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
}

void ShadowClass::CopyZDepth(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* const dst_zdepth,
		GpuResource* const dst_faceIDCube,
		BOOL needCubemap) {
	static const auto copy = [&](GpuResource* const src) {
		src->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		dst_zdepth->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

		cmdList->CopyResource(dst_zdepth->Resource(), src->Resource());

		src->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
		dst_zdepth->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	};

	if (needCubemap) {
		GpuResource* const src_zdepth = mShadowMaps[Resource::E_ZDepthCube].get();
		copy(src_zdepth);

		GpuResource* const src_faceIDCube = mShadowMaps[Resource::E_FaceIDCube].get();

		src_faceIDCube->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		dst_faceIDCube->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

		cmdList->CopyResource(dst_faceIDCube->Resource(), src_faceIDCube->Resource());

		src_faceIDCube->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		dst_faceIDCube->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	else {
		GpuResource* const src_zdepth = mShadowMaps[Resource::E_ZDepth].get();
		copy(src_zdepth);
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

	RootConstant::Shadow::Struct rc;
	rc.gLightIndex = index;

	std::array<std::uint32_t, RootConstant::Shadow::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::Shadow::Struct));
	
	cmdList->SetComputeRoot32BitConstants(RootSignature::Shadow::EC_Consts, RootConstant::Shadow::Count, consts.data(), 0);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_Position, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_ZDepth, mhGpuDescs[Descriptor::ESI_ZDepth]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_ZDepthCube, mhGpuDescs[Descriptor::ESI_ZDepthCube]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::ESI_FaceIDCube, mhGpuDescs[Descriptor::ESI_FaceIDCube]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::EUO_Shadow, mhGpuDescs[Descriptor::EUO_Shadow]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Shadow::EUO_Debug, mhGpuDescs[Descriptor::EUO_Debug]);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mClientWidth), Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mClientHeight), Default::ThreadGroup::Height), 1);

	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, shadow);
}