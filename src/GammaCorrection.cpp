#include "GammaCorrection.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "GpuResource.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"

using namespace GammaCorrection;

namespace {
	const std::string GammaCorrectionVS = "GammaCorrectionVS";
	const std::string GammaCorrectionPS = "GammaCorrectionPS";
}

GammaCorrectionClass::GammaCorrectionClass() {
	mDuplicatedBackBuffer = std::make_unique<GpuResource>();
}

BOOL GammaCorrectionClass::Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	CheckReturn(BuildResources());

	return true;
}

BOOL GammaCorrectionClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"GammaCorrection.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, GammaCorrectionVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, GammaCorrectionPS));

	return true;
}

BOOL GammaCorrectionClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(RootConstantsLayout::Count, 0);
	slotRootParameter[RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

BOOL GammaCorrectionClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(GammaCorrectionVS);
		auto ps = mShaderManager->GetDxcShader(GammaCorrectionPS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = SDR_FORMAT;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void GammaCorrectionClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		FLOAT gamma) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mDuplicatedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(mDuplicatedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mDuplicatedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	FLOAT values[RootConstantsLayout::Count] = { gamma };
	cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_BackBuffer, mhDuplicatedBackBufferGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void GammaCorrectionClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, 
		UINT descSize) {
	mhDuplicatedBackBufferCpuSrv = hCpuSrv;
	mhDuplicatedBackBufferGpuSrv = hGpuSrv;

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);

	BuildDescriptors();
}

BOOL GammaCorrectionClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}


void GammaCorrectionClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = SDR_FORMAT;

	md3dDevice->CreateShaderResourceView(mDuplicatedBackBuffer->Resource(), &srvDesc, mhDuplicatedBackBufferCpuSrv);
}

BOOL GammaCorrectionClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.Format = SDR_FORMAT;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckReturn(mDuplicatedBackBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		L"DuplicatedBackBuffer"
	));

	return true;
}