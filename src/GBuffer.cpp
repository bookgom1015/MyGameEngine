#include "GBuffer.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "MathHelper.h"
#include "HlslCompaction.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"

using namespace GBuffer;

namespace {
	const std::string GBufferVS = "GBufferVS";
	const std::string GBufferPS = "GBufferPS";
}

GBufferClass::GBufferClass() {
	mColorMap = std::make_unique<GpuResource>();
	mAlbedoMap = std::make_unique<GpuResource>();
	mNormalMap = std::make_unique<GpuResource>();
	mSpecularMap = std::make_unique<GpuResource>();
	mVelocityMap = std::make_unique<GpuResource>();
}

bool GBufferClass::Initialize(ID3D12Device* device, UINT width, UINT height, 
		ShaderManager*const manager, GpuResource*const depth, D3D12_CPU_DESCRIPTOR_HANDLE dsv, DXGI_FORMAT depthFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mDepthMap = depth;
	mhDepthMapCpuDsv = dsv;
	mDepthFormat = depthFormat;

	CheckReturn(BuildResources());

	return true;
}

bool GBufferClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"GBuffer.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, GBufferVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, GBufferPS));

	return true;
}

bool GBufferClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignatureLayout::ECB_Mat].InitAsConstantBufferView(2);
	slotRootParameter[RootSignatureLayout::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[0]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool GBufferClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayout;
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(GBufferVS);
		auto ps = mShaderManager->GetDxcShader(GBufferPS);
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
	psoDesc.RTVFormats[0] = ColorMapFormat;
	psoDesc.RTVFormats[1] = AlbedoMapFormat;
	psoDesc.RTVFormats[2] = NormalMapFormat;
	psoDesc.RTVFormats[3] = SpecularMapFormat;
	psoDesc.RTVFormats[4] = VelocityMapFormat;
	psoDesc.DSVFormat = mDepthFormat;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void GBufferClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	mColorMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mAlbedoMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mNormalMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mSpecularMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mVelocityMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	
	cmdList->ClearRenderTargetView(mhColorMapCpuRtv, GBuffer::ColorMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhAlbedoMapCpuRtv, GBuffer::AlbedoMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhNormalMapCpuRtv, GBuffer::NormalMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhSpecularMapCpuRtv, GBuffer::SpecularMapClearValues, 0, nullptr);
	cmdList->ClearRenderTargetView(mhVelocityMapCpuRtv, GBuffer::VelocityMapClearValues, 0, nullptr);
	
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::NumRenderTargets> renderTargets = { 
		mhColorMapCpuRtv, mhAlbedoMapCpuRtv, mhNormalMapCpuRtv, mhSpecularMapCpuRtv, mhVelocityMapCpuRtv 
	};
	
	cmdList->OMSetRenderTargets(static_cast<UINT>(renderTargets.size()), renderTargets.data(), true, &mhDepthMapCpuDsv);
	cmdList->ClearDepthStencilView(mhDepthMapCpuDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, passCBAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_TexMaps, si_texMaps);
	
	DrawRenderItems(cmdList, ritems, objCBAddress, matCBAddress);
	
	mColorMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mAlbedoMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mNormalMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mSpecularMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mVelocityMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mDepthMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
}


void GBufferClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhColorMapCpuSrv = hCpuSrv;
	mhColorMapGpuSrv = hGpuSrv;
	mhColorMapCpuRtv = hCpuRtv;

	mhAlbedoMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhAlbedoMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhAlbedoMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhNormalMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhNormalMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhNormalMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhDepthMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDepthMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhSpecularMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhSpecularMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhSpecularMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhVelocityMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhVelocityMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhVelocityMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool GBufferClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
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
		srvDesc.Format = ColorMapFormat;
		rtvDesc.Format = ColorMapFormat;
		md3dDevice->CreateShaderResourceView(mColorMap->Resource(), &srvDesc, mhColorMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mColorMap->Resource(), &rtvDesc, mhColorMapCpuRtv);
	}
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
		srvDesc.Format = DepthMapFormat;
		md3dDevice->CreateShaderResourceView(mDepthMap->Resource(), &srvDesc, mhDepthMapCpuSrv);
	}
	{
		srvDesc.Format = SpecularMapFormat;
		rtvDesc.Format = SpecularMapFormat;
		md3dDevice->CreateShaderResourceView(mSpecularMap->Resource(), &srvDesc, mhSpecularMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mSpecularMap->Resource(), &rtvDesc, mhSpecularMapCpuRtv);
	}
	{
		srvDesc.Format = VelocityMapFormat;
		rtvDesc.Format = VelocityMapFormat;
		md3dDevice->CreateShaderResourceView(mVelocityMap->Resource(), &srvDesc, mhVelocityMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mVelocityMap->Resource(), &rtvDesc, mhVelocityMapCpuRtv);
	}
}

bool GBufferClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		rscDesc.Format = ColorMapFormat;

		CD3DX12_CLEAR_VALUE optClear(ColorMapFormat, ColorMapClearValues);

		CheckReturn(mColorMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			L"ColorMap"
		));
	}
	{
		CD3DX12_CLEAR_VALUE optClear(AlbedoMapFormat, AlbedoMapClearValues);

		CheckReturn(mAlbedoMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			L"AlbedoMap"
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
			L"NormalMap"
		));
	}
	{
		rscDesc.Format = SpecularMapFormat;

		CD3DX12_CLEAR_VALUE optClear(SpecularMapFormat, SpecularMapClearValues);

		CheckReturn(mSpecularMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			L"SpecularMap"
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
			L"VelocityMap"
		));
	}

	return true;
}

void GBufferClass::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress, D3D12_GPU_VIRTUAL_ADDRESS matCBAddress) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];
		
		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = objCBAddress + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = matCBAddress + ri->Material->MatCBIndex * matCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}