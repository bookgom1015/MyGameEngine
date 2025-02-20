#include "MipmapGenerator.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

using namespace MipmapGenerator;

namespace {
	const CHAR* const VS_GenerateMipmap = "VS_GenerateMipmap";
	const CHAR* const PS_GenerateMipmap = "PS_GenerateMipmap";
	const CHAR* const PS_JustCopy = "PS_JustCopy";
}

BOOL MipmapGeneratorClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL MipmapGeneratorClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"GenerateMipmap.hlsl";
	const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GenerateMipmap));
	{		
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS_GenerateMipmap", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GenerateMipmap));
	}
	{
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS_JustCopy", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_JustCopy));
	}

	return TRUE;
}

BOOL MipmapGeneratorClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count] = {};
	slotRootParameter[RootSignature::Default::EC_Consts].InitAsConstants(RootConstant::Default::Count, 0);
	slotRootParameter[RootSignature::Default::ESI_Input].InitAsDescriptorTable(1, &texTables[index++]);

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
		const auto vs = mShaderManager->GetDxcShader(VS_GenerateMipmap);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
	}

	{		
		const auto ps = mShaderManager->GetDxcShader(PS_GenerateMipmap);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_GenerateMipmap])));

	{
		const auto ps = mShaderManager->GetDxcShader(PS_JustCopy);
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_JustCopy])));

	return TRUE;
}

BOOL MipmapGeneratorClass::GenerateMipmap(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* const output,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_outputs[],
		UINT width, UINT height,
		UINT maxMipLevel) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_JustCopy].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	{
		const D3D12_VIEWPORT viewport = { 0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.f, 1.f };
		const D3D12_RECT scissorRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
	}

	output->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_outputs[0], TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Input, si_input);

	{
		FLOAT invW = 1.f / width;
		FLOAT invH = 1.f / height;

		RootConstant::Default::Struct rc;
		rc.gInvTexSize.x = invW;
		rc.gInvTexSize.y = invH;
		rc.gInvMipmapTexSize.x = 0.f;
		rc.gInvMipmapTexSize.y = 0.f;

		std::array<std::uint32_t, RootConstant::Default::Count> consts;
		std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

		cmdList->SetGraphicsRoot32BitConstants(RootSignature::Default::EC_Consts, RootConstant::Default::Count, consts.data(), 0);
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->SetPipelineState(mPSOs[PipelineState::EG_GenerateMipmap].Get());
	for (UINT mipLevel = 1; mipLevel < maxMipLevel; ++mipLevel) {
		cmdList->OMSetRenderTargets(1, &ro_outputs[mipLevel], TRUE, nullptr);
		
		const UINT mw = static_cast<UINT>(width / std::pow(2, mipLevel));
		const UINT mh = static_cast<UINT>(height / std::pow(2, mipLevel));

		const D3D12_VIEWPORT viewport = { 0.f, 0.f, static_cast<FLOAT>(mw), static_cast<FLOAT>(mh), 0.f, 1.f };
		const D3D12_RECT scissorRect = { 0, 0, static_cast<INT >(mw), static_cast<INT>(mh) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		const FLOAT invW = 1.f / width;
		const FLOAT invH = 1.f / height;
		const FLOAT invMW = 1.f / mw;
		const FLOAT invMH = 1.f / mh;

		RootConstant::Default::Struct rc;
		rc.gInvTexSize.x = invW;
		rc.gInvTexSize.y = invH;
		rc.gInvMipmapTexSize.x = invMW;
		rc.gInvMipmapTexSize.y = invMH;

		std::array<std::uint32_t, RootConstant::Default::Count> consts;
		std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

		cmdList->SetGraphicsRoot32BitConstants(RootSignature::Default::EC_Consts, RootConstant::Default::Count, consts.data(), 0);
	
		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(6, 1, 0, 0);
	}

	output->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}