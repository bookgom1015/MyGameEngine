#include "Shadow.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "MathHelper.h"
#include "HlslCompaction.h"
#include "D3D12Util.h"
#include "Vertex.h"

using namespace Shadow;

namespace {
	const CHAR* const VS_Shadow = "VS_Shadow";
	const CHAR* const PS_Shadow = "PS_Shadow";
}

ShadowClass::ShadowClass() {
	mShadowMap = std::make_unique<GpuResource>();
}

BOOL ShadowClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };

	CheckReturn(BuildResources());

	return TRUE;
}

BOOL ShadowClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"Shadow.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath .c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath .c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_Shadow));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_Shadow));

	return TRUE;
}

BOOL ShadowClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 0, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignatureLayout::ECB_Mat].InitAsConstantBufferView(2);
	slotRootParameter[RootSignatureLayout::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL ShadowClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), DepthStencilBuffer::BufferFormat);
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_Shadow);
		auto ps = mShaderManager->GetDxcShader(PS_Shadow);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.NumRenderTargets = 0;
	psoDesc.RasterizerState.DepthBias = 100000;
	psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	psoDesc.RasterizerState.DepthBiasClamp = 0.1f;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return TRUE;
}

void ShadowClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mShadowMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);	

	cmdList->ClearDepthStencilView(mhCpuDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, false, &mhCpuDsv);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cb_pass);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_TexMaps, si_texMaps);

	DrawRenderItems(cmdList, ritems, cb_obj, cb_mat, objCBByteSize, matCBByteSize);

	mShadowMap->Transite(cmdList, D3D12_RESOURCE_STATE_DEPTH_READ);
}

void ShadowClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, 
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
		UINT descSize, UINT dsvDescSize) {
	mhCpuSrv = hCpu.Offset(1, descSize);
	mhGpuSrv = hGpu.Offset(1, descSize);
	mhCpuDsv = hCpuDsv.Offset(1, dsvDescSize);

	BuildDescriptors();
}

void ShadowClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateShaderResourceView(mShadowMap->Resource(), &srvDesc, mhCpuSrv);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = ShadowMapFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mShadowMap->Resource(), &dsvDesc, mhCpuDsv);
}

BOOL ShadowClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = ShadowMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = ShadowMapFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	CheckReturn(mShadowMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_DEPTH_READ,
		&optClear,
		L"Shadow_ShadowMap"
	));

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
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);

		if (ri->Material != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS currRitemMatCBAddress = cb_mat + ri->Material->MatCBIndex * matCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Mat, currRitemMatCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}