#include "GaussianFilter.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"

using namespace GaussianFilter;

namespace {
	const CHAR* const CS_GaussianFilter3x3	 = "CS_GaussianFilter3x3";
	const CHAR* const CS_GaussianFilterRG3x3 = "CS_GaussianFilterRG3x3";
}

BOOL GaussianFilterClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL GaussianFilterClass::CompileShaders(const std::wstring& filePath) {
	{
		const auto fullPath = filePath + L"GaussianFilter3x3CS.hlsl";
		const auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_GaussianFilter3x3));
	}
	{
		const auto fullPath = filePath + L"GaussianFilterRG3x3CS.hlsl";
		const auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_GaussianFilterRG3x3));
	}

	return TRUE;
}

BOOL GaussianFilterClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[2]; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count];
	slotRootParameter[RootSignature::Default::EC_Consts].InitAsConstants(RootConstant::Default::Count, 0, 0);
	slotRootParameter[RootSignature::Default::ESI_Input].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::EUO_Output].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return TRUE;
}

BOOL GaussianFilterClass::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (UINT i = 0; i < FilterType::Count; ++i) {
		IDxcBlob* cs;
		switch (i) {
		case FilterType::Filter3x3:
			cs = mShaderManager->GetDxcShader(CS_GaussianFilter3x3);
			break;
		case FilterType::Filter3x3RG:
			cs = mShaderManager->GetDxcShader(CS_GaussianFilterRG3x3);
			break;
		default:
			ReturnFalse(L"Unsupported FilterType");
		}
		psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[static_cast<FilterType>(i)])));
	}

	return TRUE;
}

void GaussianFilterClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_output,
		FilterType type,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	RootConstant::Default::Struct rc;
	rc.gTextureDim.x = width;
	rc.gTextureDim.y = height;
	rc.gInvTextureDim.x = 1.f / width;
	rc.gInvTextureDim.y = 1.f / height;

	std::array<std::uint32_t, RootConstant::Default::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::Default::Struct));

	cmdList->SetComputeRoot32BitConstants(RootSignature::Default::EC_Consts, RootConstant::Default::Count, consts.data(), 0);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Default::ESI_Input, si_input);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Default::EUO_Output, uo_output);

	cmdList->Dispatch(D3D12Util::CeilDivide(width, ThreadGroup::Default::Width), D3D12Util::CeilDivide(height, ThreadGroup::Default::Height), 1);
}