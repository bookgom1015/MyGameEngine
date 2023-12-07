#include "MipmapGenerator.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

using namespace MipmapGenerator;

namespace {
	const std::string VS = "VS";
	const std::string GenerateMipmapPS = "GenerateMipmapPS";
	const std::string JustCopyPS = "JustCopyPS";
}

BOOL MipmapGeneratorClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

BOOL MipmapGeneratorClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"GenerateMipmap.hlsl";

	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS));
	{
		
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"GenerateMipmapPS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, GenerateMipmapPS));
	}
	{
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"JustCopyPS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, JustCopyPS));
	}

	return true;
}

BOOL MipmapGeneratorClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[MipmapGenerator::RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[MipmapGenerator::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[MipmapGenerator::RootSignatureLayout::EC_Consts].InitAsConstants(MipmapGenerator::RootConstantsLayout::Count, 1);
	slotRootParameter[MipmapGenerator::RootSignatureLayout::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

BOOL MipmapGeneratorClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = HDR_FORMAT;
	{
		auto vs = mShaderManager->GetDxcShader(VS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
	}

	{		
		auto ps = mShaderManager->GetDxcShader(GenerateMipmapPS);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_GenerateMipmap])));

	{
		auto ps = mShaderManager->GetDxcShader(JustCopyPS);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_JustCopy])));

	return true;
}

BOOL MipmapGeneratorClass::GenerateMipmap(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* output,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
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

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input, si_input);

	{
		FLOAT invW = static_cast<FLOAT>(1 / width);
		FLOAT invH = static_cast<FLOAT>(1 / height);

		FLOAT values[RootConstantsLayout::Count] = { invW, invH, 0.0f, 0.0f };
		cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, 0);
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

		FLOAT values[RootConstantsLayout::Count] = { invW, invH, invMW, invMH };
		cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, 0);
	
		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(6, 1, 0, 0);
	}

	output->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return true;
}