#include "MipmapGenerator.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

using namespace MipmapGenerator;

namespace {
	const CHAR*const VS_GenerateMipmap = "VS_GenerateMipmap";
	const CHAR*const PS_GenerateMipmap = "PS_GenerateMipmap";
	const CHAR*const PS_JustCopy = "PS_JustCopy";
}

BOOL MipmapGeneratorClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL MipmapGeneratorClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"GenerateMipmap.hlsl";

	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GenerateMipmap));
	{
		
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS_GenerateMipmap", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GenerateMipmap));
	}
	{
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS_JustCopy", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_JustCopy));
	}

	return TRUE;
}

BOOL MipmapGeneratorClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[MipmapGenerator::RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[MipmapGenerator::RootSignature::EC_Consts].InitAsConstants(MipmapGenerator::RootSignature::RootConstant::Count, 0);
	slotRootParameter[MipmapGenerator::RootSignature::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL MipmapGeneratorClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = HDR_FORMAT;
	{
		auto vs = mShaderManager->GetDxcShader(VS_GenerateMipmap);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
	}

	{		
		auto ps = mShaderManager->GetDxcShader(PS_GenerateMipmap);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_GenerateMipmap])));

	{
		auto ps = mShaderManager->GetDxcShader(PS_JustCopy);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_JustCopy])));

	return TRUE;
}

BOOL MipmapGeneratorClass::GenerateMipmap(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* output,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_outputs[],
		UINT width, UINT height,
		UINT maxMipLevel) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_JustCopy].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	{
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
	}

	output->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_outputs[0], TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Input, si_input);

	{
		FLOAT invW = static_cast<FLOAT>(1 / width);
		FLOAT invH = static_cast<FLOAT>(1 / height);

		FLOAT values[RootSignature::RootConstant::Count] = { invW, invH, 0.0f, 0.0f };
		cmdList->SetGraphicsRoot32BitConstants(RootSignature::EC_Consts, _countof(values), values, 0);
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->SetPipelineState(mPSOs[PipelineState::E_GenerateMipmap].Get());
	for (UINT mipLevel = 1; mipLevel < maxMipLevel; ++mipLevel) {
		cmdList->OMSetRenderTargets(1, &ro_outputs[mipLevel], TRUE, nullptr);
		
		UINT mw = static_cast<UINT>(width / std::pow(2, mipLevel));
		UINT mh = static_cast<UINT>(height / std::pow(2, mipLevel));

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(mw), static_cast<FLOAT>(mh), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<INT >(mw), static_cast<INT>(mh) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		FLOAT invW = static_cast<FLOAT>(1 / width);
		FLOAT invH = static_cast<FLOAT>(1 / height);
		FLOAT invMW = static_cast<FLOAT>(1 / mw);
		FLOAT invMH = static_cast<FLOAT>(1 / mh);	

		FLOAT values[RootSignature::RootConstant::Count] = { invW, invH, invMW, invMH };
		cmdList->SetGraphicsRoot32BitConstants(RootSignature::EC_Consts, _countof(values), values, 0);
	
		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(6, 1, 0, 0);
	}

	output->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}