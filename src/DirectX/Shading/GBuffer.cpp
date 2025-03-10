#include "DirectX/Shading/GBuffer.h"
#include "Common/Debug/Logger.h"
#include "Common/Mesh/Vertex.h"
#include "Common/Render/RenderItem.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"
#include "DxMesh.h"
#include "ResourceUploadBatch.h"
#include "DDSTextureLoader.h"

using namespace DirectX;
using namespace GBuffer;

namespace {
	const CHAR* const VS_GBuffer = "VS_GBuffer";
	const CHAR* const PS_GBuffer = "PS_GBuffer";
}

GBufferClass::GBufferClass() {
	mAlbedoMap = std::make_unique<GpuResource>();
	mNormalMap = std::make_unique<GpuResource>();
	mNormalDepthMap = std::make_unique<GpuResource>();
	mPrevNormalDepthMap = std::make_unique<GpuResource>();
	mRMSMap = std::make_unique<GpuResource>();
	mVelocityMap = std::make_unique<GpuResource>();
	mReprojNormalDepthMap = std::make_unique<GpuResource>();
	mPositionMap = std::make_unique<GpuResource>();
}

BOOL GBufferClass::Initialize(Locker<ID3D12Device5>* const device, UINT width, UINT height,
		ShaderManager* const manager, GpuResource* const depth, D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
	md3dDevice = device;
	mShaderManager = manager;

	mDepthMap = depth;
	mhDepthMapCpuDsv = dsv;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL GBufferClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"GBuffer.hlsl";
	const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GBuffer));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_GBuffer));

	return TRUE;
}

BOOL GBufferClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count] = {};
	slotRootParameter[RootSignature::Default::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::Default::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignature::Default::ECB_Mat].InitAsConstantBufferView(2);
	slotRootParameter[RootSignature::Default::EC_Consts].InitAsConstants(RootConstant::Default::Count, 3);
	slotRootParameter[RootSignature::Default::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateRootSignature(device, rootSigDesc, &mRootSignature, L"GBuffer_RS_Default"));
	}

	return TRUE;
}

BOOL GBufferClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = Vertex::InputLayoutDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto vs = mShaderManager->GetDxcShader(VS_GBuffer);
		const auto ps = mShaderManager->GetDxcShader(PS_GBuffer);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.NumRenderTargets = NumRenderTargets;
	psoDesc.RTVFormats[0] = AlbedoMapFormat;
	psoDesc.RTVFormats[1] = NormalMapFormat;
	psoDesc.RTVFormats[2] = NormalDepthMapFormat;
	psoDesc.RTVFormats[3] = RMSMapFormat;
	psoDesc.RTVFormats[4] = VelocityMapFormat;
	psoDesc.RTVFormats[5] = NormalDepthMapFormat;
	psoDesc.RTVFormats[6] = PositionMapFormat;
	psoDesc.DSVFormat = DepthStencilBuffer::BufferFormat;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckHRESULT(D3D12Util::CreateGraphicsPipelineState(device, psoDesc, IID_PPV_ARGS(&mPSO), L"GBuffer_GPS_Default"));
	}

	return TRUE;
}

void GBufferClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		const std::vector<RenderItem*>& ritems,
		FLOAT maxDist,
		FLOAT minDist) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	mAlbedoMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mNormalMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mNormalDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mRMSMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mVelocityMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mReprojNormalDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mPositionMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	cmdList->ClearRenderTargetView(mhAlbedoMapCpuRtv, GBuffer::AlbedoMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhNormalMapCpuRtv, GBuffer::NormalMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhNormalDepthMapCpuRtv, GBuffer::NormalDepthMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhRMSMapCpuRtv, GBuffer::RMSMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhVelocityMapCpuRtv, GBuffer::VelocityMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhReprojNormalDepthMapCpuRtv, GBuffer::ReprojNormalDepthMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhPositionMapCpuRtv, GBuffer::PositionMapClearValues, 0, nullptr);
	
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::NumRenderTargets> renderTargets = { 
		mhAlbedoMapCpuRtv, mhNormalMapCpuRtv, mhNormalDepthMapCpuRtv, mhRMSMapCpuRtv, mhVelocityMapCpuRtv, mhReprojNormalDepthMapCpuRtv, mhPositionMapCpuRtv
	};
	
	cmdList->OMSetRenderTargets(static_cast<UINT>(renderTargets.size()), renderTargets.data(), TRUE, &mhDepthMapCpuDsv);
	cmdList->ClearDepthStencilView(mhDepthMapCpuDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	
	cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_Pass, cb_pass);

	RootConstant::Default::Struct rc;
	rc.gMaxDistance = maxDist;
	rc.gMinDistance = minDist;

	std::array<std::uint32_t, RootConstant::Default::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

	cmdList->SetGraphicsRoot32BitConstants(RootSignature::Default::EC_Consts, RootConstant::Default::Count, consts.data(), 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_TexMaps, si_texMaps);
	
	DrawRenderItems(cmdList, ritems, cb_obj, cb_mat, objCBByteSize, matCBByteSize);
	
	mAlbedoMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mNormalMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mNormalDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mRMSMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mVelocityMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
	mReprojNormalDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mPositionMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}


void GBufferClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhAlbedoMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhAlbedoMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhAlbedoMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhNormalMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhNormalMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhNormalMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhNormalDepthMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhNormalDepthMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhNormalDepthMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhPrevNormalDepthMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhPrevNormalDepthMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDepthMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDepthMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhRMSMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhRMSMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhRMSMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhVelocityMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhVelocityMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhVelocityMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhReprojNormalDepthMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhReprojNormalDepthMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhReprojNormalDepthMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhPositionMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhPositionMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhPositionMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);
}

BOOL GBufferClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	D3D12Util::Descriptor::View::Builder builder;
	{
		srvDesc.Format = AlbedoMapFormat;
		rtvDesc.Format = AlbedoMapFormat;

		builder.Enqueue(mAlbedoMap->Resource(), srvDesc, mhAlbedoMapCpuSrv);
		builder.Enqueue(mAlbedoMap->Resource(), rtvDesc, mhAlbedoMapCpuRtv);
	}
	{
		srvDesc.Format = NormalMapFormat;
		rtvDesc.Format = NormalMapFormat;

		builder.Enqueue(mNormalMap->Resource(), srvDesc, mhNormalMapCpuSrv);
		builder.Enqueue(mNormalMap->Resource(), rtvDesc, mhNormalMapCpuRtv);
	}
	{
		srvDesc.Format = NormalDepthMapFormat;
		rtvDesc.Format = NormalDepthMapFormat;

		builder.Enqueue(mNormalDepthMap->Resource(), srvDesc, mhNormalDepthMapCpuSrv);
		builder.Enqueue(mNormalDepthMap->Resource(), rtvDesc, mhNormalDepthMapCpuRtv);

		builder.Enqueue(mReprojNormalDepthMap->Resource(), srvDesc, mhReprojNormalDepthMapCpuSrv);
		builder.Enqueue(mReprojNormalDepthMap->Resource(), rtvDesc, mhReprojNormalDepthMapCpuRtv);

		builder.Enqueue(mPrevNormalDepthMap->Resource(), srvDesc, mhPrevNormalDepthMapCpuSrv);
	}
	{
		srvDesc.Format = DepthMapFormat;

		builder.Enqueue(mDepthMap->Resource(), srvDesc, mhDepthMapCpuSrv);
	}
	{
		srvDesc.Format = RMSMapFormat;
		rtvDesc.Format = RMSMapFormat;

		builder.Enqueue(mRMSMap->Resource(), srvDesc, mhRMSMapCpuSrv);
		builder.Enqueue(mRMSMap->Resource(), rtvDesc, mhRMSMapCpuRtv);
	}
	{
		srvDesc.Format = VelocityMapFormat;
		rtvDesc.Format = VelocityMapFormat;

		builder.Enqueue(mVelocityMap->Resource(), srvDesc, mhVelocityMapCpuSrv);
		builder.Enqueue(mVelocityMap->Resource(), rtvDesc, mhVelocityMapCpuRtv);
	}
	{
		srvDesc.Format = PositionMapFormat;
		rtvDesc.Format = PositionMapFormat;

		builder.Enqueue(mPositionMap->Resource(), srvDesc, mhPositionMapCpuSrv);
		builder.Enqueue(mPositionMap->Resource(), rtvDesc, mhPositionMapCpuRtv);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL GBufferClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

BOOL GBufferClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	const CD3DX12_CLEAR_VALUE albedoOptClear(AlbedoMapFormat, AlbedoMapClearValues);
	const CD3DX12_CLEAR_VALUE normalOptClear(NormalMapFormat, NormalMapClearValues);
	const CD3DX12_CLEAR_VALUE normalDepthOptClear(NormalDepthMapFormat, NormalDepthMapClearValues);
	const CD3DX12_CLEAR_VALUE reprojNormalDepthOptClear(NormalDepthMapFormat, ReprojNormalDepthMapClearValues);
	const CD3DX12_CLEAR_VALUE rmsOptClear(RMSMapFormat, RMSMapClearValues);
	const CD3DX12_CLEAR_VALUE velocityOptClear(VelocityMapFormat, VelocityMapClearValues);
	const CD3DX12_CLEAR_VALUE posOptClear(PositionMapFormat, PositionMapClearValues);

	D3D12Util::Descriptor::Resource::Builder builder;
	{
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		{
			rscDesc.Format = AlbedoMapFormat;

			builder.Enqueue(
				mAlbedoMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&albedoOptClear,
				L"GBuffer_AlbedoMap"
			);
		}
		{
			rscDesc.Format = NormalMapFormat;

			builder.Enqueue(
				mNormalMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&normalOptClear,
				L"GBuffer_NormalMap"
			);
		}
		{
			rscDesc.Format = NormalDepthMapFormat;

			builder.Enqueue(
				mNormalDepthMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&normalDepthOptClear,
				L"GBuffer_NormalDepthMap"
			);
			builder.Enqueue(
				mReprojNormalDepthMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&reprojNormalDepthOptClear,
				L"GBuffer_ReprojNormalDepthMap"
			);
		}
		{
			rscDesc.Format = RMSMapFormat;

			builder.Enqueue(
				mRMSMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&rmsOptClear,
				L"GBuffer_RoughnessMetalicSpecularMap"
			);
		}
		{
			rscDesc.Format = VelocityMapFormat;

			builder.Enqueue(
				mVelocityMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&velocityOptClear,
				L"GBuffer_VelocityMap"
			);
		}
		{
			rscDesc.Format = PositionMapFormat;

			builder.Enqueue(
				mPositionMap.get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&posOptClear,
				L"GBuffer_PositionMap"
			);
		}
	}
	{
		rscDesc.Format = NormalDepthMapFormat;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		builder.Enqueue(
			mPrevNormalDepthMap.get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"GBuffer_PrevNormalDepthMap"
		);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void GBufferClass::DrawRenderItems(
		ID3D12GraphicsCommandList* const cmdList, const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj, D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize, UINT matCBByteSize) {
	
	for (size_t i = 0, end = ritems.size(); i < end; ++i) {
		auto& ri = ritems[i];
		
		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		const D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cb_obj + static_cast<UINT64>(ri->ObjCBIndex) * static_cast<UINT64>(objCBByteSize);
		cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			const D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = cb_mat + static_cast<UINT64>(ri->Material->MatCBIndex) * static_cast<UINT64>(matCBByteSize);
			cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}