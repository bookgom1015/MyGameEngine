#include "GBuffer.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "MathHelper.h"
#include "HlslCompaction.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "Vertex.h"
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

BOOL GBufferClass::Initialize(ID3D12Device*const device, UINT width, UINT height, 
		ShaderManager*const manager, GpuResource*const depth, D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
	md3dDevice = device;
	mShaderManager = manager;

	mDepthMap = depth;
	mhDepthMapCpuDsv = dsv;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL GBufferClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"GBuffer.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GBuffer));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_GBuffer));

	return TRUE;
}

BOOL GBufferClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignatureLayout::ECB_Mat].InitAsConstantBufferView(2);
	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(RootSignatureLayout::RootConstant::Count, 3);
	slotRootParameter[RootSignatureLayout::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL GBufferClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = Vertex::InputLayoutDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_GBuffer);
		auto ps = mShaderManager->GetDxcShader(PS_GBuffer);
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

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return TRUE;
}

void GBufferClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
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
	cmdList->ClearDepthStencilView(mhDepthMapCpuDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cb_pass);

	float values[RootSignatureLayout::RootConstant::Count] = { maxDist, minDist };
	cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_TexMaps, si_texMaps);
	
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


void GBufferClass::BuildDescriptors(
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

	BuildDescriptors();
}

BOOL GBufferClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void GBufferClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	{
		srvDesc.Format = AlbedoMapFormat;
		rtvDesc.Format = AlbedoMapFormat;
		md3dDevice->CreateShaderResourceView(mAlbedoMap->Resource(), &srvDesc, mhAlbedoMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mAlbedoMap->Resource(), &rtvDesc, mhAlbedoMapCpuRtv);
	}
	{
		srvDesc.Format = NormalMapFormat;
		rtvDesc.Format = NormalMapFormat;
		md3dDevice->CreateShaderResourceView(mNormalMap->Resource(), &srvDesc, mhNormalMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mNormalMap->Resource(), &rtvDesc, mhNormalMapCpuRtv);
	}
	{
		srvDesc.Format = NormalDepthMapFormat;
		rtvDesc.Format = NormalDepthMapFormat;
		md3dDevice->CreateShaderResourceView(mNormalDepthMap->Resource(), &srvDesc, mhNormalDepthMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mNormalDepthMap->Resource(), &rtvDesc, mhNormalDepthMapCpuRtv);

		md3dDevice->CreateShaderResourceView(mReprojNormalDepthMap->Resource(), &srvDesc, mhReprojNormalDepthMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mReprojNormalDepthMap->Resource(), &rtvDesc, mhReprojNormalDepthMapCpuRtv);

		md3dDevice->CreateShaderResourceView(mPrevNormalDepthMap->Resource(), &srvDesc, mhPrevNormalDepthMapCpuSrv);
	}
	{
		srvDesc.Format = DepthMapFormat;
		md3dDevice->CreateShaderResourceView(mDepthMap->Resource(), &srvDesc, mhDepthMapCpuSrv);
	}
	{
		srvDesc.Format = RMSMapFormat;
		rtvDesc.Format = RMSMapFormat;
		md3dDevice->CreateShaderResourceView(mRMSMap->Resource(), &srvDesc, mhRMSMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mRMSMap->Resource(), &rtvDesc, mhRMSMapCpuRtv);
	}
	{
		srvDesc.Format = VelocityMapFormat;
		rtvDesc.Format = VelocityMapFormat;
		md3dDevice->CreateShaderResourceView(mVelocityMap->Resource(), &srvDesc, mhVelocityMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mVelocityMap->Resource(), &rtvDesc, mhVelocityMapCpuRtv);
	}
	{
		srvDesc.Format = PositionMapFormat;
		rtvDesc.Format = PositionMapFormat;
		md3dDevice->CreateShaderResourceView(mPositionMap->Resource(), &srvDesc, mhPositionMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mPositionMap->Resource(), &rtvDesc, mhPositionMapCpuRtv);
	}
	{
		srvDesc.Format = PositionMapFormat;
		md3dDevice->CreateShaderResourceView(mPositionMap->Resource(), &srvDesc, mhPositionMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mPositionMap->Resource(), &rtvDesc, mhPositionMapCpuRtv);
	}
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

	{
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		{
			rscDesc.Format = AlbedoMapFormat;

			CD3DX12_CLEAR_VALUE optClear(AlbedoMapFormat, AlbedoMapClearValues);

			CheckReturn(mAlbedoMap->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				L"GBuffer_AlbedoMap"
			));
		}
		{
			rscDesc.Format = NormalMapFormat;

			CD3DX12_CLEAR_VALUE optClear(NormalMapFormat, NormalMapClearValues);

			CheckReturn(mNormalMap->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				L"GBuffer_NormalMap"
			));
		}
		{
			rscDesc.Format = NormalDepthMapFormat;

			{
				CD3DX12_CLEAR_VALUE optClear(NormalDepthMapFormat, NormalDepthMapClearValues);

				CheckReturn(mNormalDepthMap->Initialize(
					md3dDevice,
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					&optClear,
					L"GBuffer_NormalDepthMap"
				));
			}
			{
				CD3DX12_CLEAR_VALUE optClear(NormalDepthMapFormat, ReprojNormalDepthMapClearValues);

				CheckReturn(mReprojNormalDepthMap->Initialize(
					md3dDevice,
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					&optClear,
					L"GBuffer_ReprojNormalDepthMap"
				));
			}
		}
		{
			rscDesc.Format = RMSMapFormat;

			CD3DX12_CLEAR_VALUE optClear(RMSMapFormat, RMSMapClearValues);

			CheckReturn(mRMSMap->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				L"GBuffer_RoughnessMetalicSpecularMap"
			));
		}
		{
			rscDesc.Format = VelocityMapFormat;

			CD3DX12_CLEAR_VALUE optClear(VelocityMapFormat, VelocityMapClearValues);

			CheckReturn(mVelocityMap->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				L"GBuffer_VelocityMap"
			));
		}
		{
			rscDesc.Format = PositionMapFormat;

			CD3DX12_CLEAR_VALUE optClear(PositionMapFormat, PositionMapClearValues);

			CheckReturn(mPositionMap->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				L"GBuffer_PositionMap"
			));
		}
	}
	{
		rscDesc.Format = NormalDepthMapFormat;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CheckReturn(mPrevNormalDepthMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"GBuffer_PrevNormalDepthMap"
		));
	}

	return TRUE;
}

void GBufferClass::DrawRenderItems(
		ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj, D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize, UINT matCBByteSize) {
	
	for (size_t i = 0, end = ritems.size(); i < end; ++i) {
		auto& ri = ritems[i];
		
		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cb_obj + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = cb_mat + ri->Material->MatCBIndex * matCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}