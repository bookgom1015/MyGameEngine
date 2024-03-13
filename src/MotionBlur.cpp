#include "MotionBlur.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"

using namespace MotionBlur;

namespace {
	const CHAR* VS_MotionBlur = "VS_MotionBlur";
	const CHAR* PS_MotionBlur = "PS_MotionBlur";
}

MotionBlurClass::MotionBlurClass() {
	mCopiedBackBuffer = std::make_unique<GpuResource>();
}

BOOL MotionBlurClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL MotionBlurClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"MotionBlur.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_MotionBlur));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_MotionBlur));

	return TRUE;
}

BOOL MotionBlurClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	slotRootParameter[RootSignature::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(1, &texTable1);
	slotRootParameter[RootSignature::ESI_Velocity].InitAsDescriptorTable(1, &texTable2);
	slotRootParameter[RootSignature::EC_Consts].InitAsConstants(RootSignature::RootConstant::Count, 0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL MotionBlurClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC motionBlurPsoDesc = quadPsoDesc;
	motionBlurPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_MotionBlur);
		auto ps = mShaderManager->GetDxcShader(PS_MotionBlur);
		motionBlurPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		motionBlurPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	motionBlurPsoDesc.RTVFormats[0] = SDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&motionBlurPsoDesc, IID_PPV_ARGS(&mPSO)));

	return TRUE;
}

void MotionBlurClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,	CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhCopiedBackBufferCpuSrv = hCpu.Offset(1, descSize);
	mhCopiedBackBufferGpuSrv = hGpu.Offset(1, descSize);

	BuildDescriptors();
}

BOOL MotionBlurClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void MotionBlurClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
		FLOAT intensity,
		FLOAT limit,
		FLOAT depthBias,
		INT sampleCount) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_BackBuffer, mhCopiedBackBufferGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Velocity, si_velocity);

	FLOAT values[RootSignature::RootConstant::Count] = { intensity, limit, depthBias, static_cast<FLOAT>(sampleCount) };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::EC_Consts, _countof(values), values, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void MotionBlurClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = SDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	md3dDevice->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferCpuSrv);
}

BOOL MotionBlurClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Format = SDR_FORMAT;
	rscDesc.Alignment = 0;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckReturn(mCopiedBackBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		L"MB_CopiedBackBuffer"
	));

	return TRUE;
}