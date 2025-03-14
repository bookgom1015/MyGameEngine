#include "DirectX/Shading/GammaCorrection.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "HlslCompaction.h"

using namespace GammaCorrection;

namespace {
	const CHAR* const VS_GammaCorrection = "VS_GammaCorrection";
	const CHAR* const PS_GammaCorrection = "PS_GammaCorrection";
}

GammaCorrectionClass::GammaCorrectionClass() {
	mCopiedBackBuffer = std::make_unique<GpuResource>();
}

BOOL GammaCorrectionClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL GammaCorrectionClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"GammaCorrection.hlsl";
	const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GammaCorrection));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_GammaCorrection));

	return TRUE;
}

BOOL GammaCorrectionClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count] = {};
	slotRootParameter[RootSignature::Default::EC_Consts].InitAsConstants(RootConstant::Default::Count, 0);
	slotRootParameter[RootSignature::Default::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateRootSignature(device, rootSigDesc, &mRootSignature));
	}

	return TRUE;
}

BOOL GammaCorrectionClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto vs = mShaderManager->GetDxcShader(VS_GammaCorrection);
		const auto ps = mShaderManager->GetDxcShader(PS_GammaCorrection);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = SDR_FORMAT;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckHRESULT(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
	}

	return TRUE;
}

void GammaCorrectionClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		FLOAT gamma) {
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

	RootConstant::Default::Struct rc;
	rc.gGamma = gamma;

	std::array<std::uint32_t, RootConstant::Default::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

	cmdList->SetGraphicsRoot32BitConstants(RootSignature::Default::EC_Consts, RootConstant::Default::Count, consts.data(), 0);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_BackBuffer, mhCopiedBackBufferGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void GammaCorrectionClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, 
		UINT descSize) {
	mhCopiedBackBufferCpuSrv = hCpuSrv.Offset(1, descSize);
	mhCopiedBackBufferGpuSrv = hGpuSrv.Offset(1, descSize);
}

BOOL GammaCorrectionClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = SDR_FORMAT;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		device->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferCpuSrv);
	}

	return TRUE;
}

BOOL GammaCorrectionClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

BOOL GammaCorrectionClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.Format = SDR_FORMAT;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(mCopiedBackBuffer->Initialize(
			device,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"GammaCorrection_CopiedBackBuffer"
		));
	}

	return TRUE;
}