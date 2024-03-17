#include "Pixelation.h"
#include "Logger.h"
#include "GpuResource.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"

using namespace Pixelation;

namespace {
	const CHAR* const VS_Pixelize = "VS_Pixelize";
	const CHAR* const PS_Pixelize = "PS_Pixelize";
}

PixelationClass::PixelationClass() {
	mCopiedBackBuffer = std::make_unique<GpuResource>();
}

BOOL PixelationClass::Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	BuildResources(width, height);

	return TRUE;
}

BOOL PixelationClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"Pixelate.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_Pixelize));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_Pixelize));

	return TRUE;
}

BOOL PixelationClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[RootSignature::EC_Consts].InitAsConstants(RootSignature::RootConstant::Count, 0);
	slotRootParameter[RootSignature::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL PixelationClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_Pixelize);
		auto ps = mShaderManager->GetDxcShader(PS_Pixelize);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = SDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return TRUE;
}

void PixelationClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		UINT descSize) {
	mhCopiedBackBufferCpuSrv = hCpuSrv.Offset(1, descSize);
	mhCopiedBackBufferGpuSrv = hGpuSrv.Offset(1, descSize);

	BuildDescriptors();	
}

BOOL PixelationClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void PixelationClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		FLOAT pixelSize) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_BackBuffer, mhCopiedBackBufferGpuSrv);

	FLOAT values[RootSignature::RootConstant::Count] = { static_cast<FLOAT>(viewport.Width), static_cast<FLOAT>(viewport.Height), pixelSize };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::EC_Consts, _countof(values), values, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void PixelationClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = SDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferCpuSrv);
}

BOOL PixelationClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = SDR_FORMAT;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckReturn(mCopiedBackBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		L"Pixel_CopiedBackBufferMap"
	));

	return TRUE;
}