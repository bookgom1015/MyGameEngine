#include "GaussianFilter.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"

using namespace GaussianFilter;

bool GaussianFilterClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

bool GaussianFilterClass::CompileShaders(const std::wstring& filePath) {
	{
		const auto fullPath = filePath + L"GaussianFilter3x3CS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, "gaussianFilter3x3CS"));
	}
	{
		const auto fullPath = filePath + L"GaussianFilterRG3x3CS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, "gaussianFilterRG3x3CS"));
	}

	return true;
}

bool GaussianFilterClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[2];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];
	slotRootParameter[RootSignature::EC_Consts].InitAsConstants(RootSignature::RootConstants::Count, 0, 0);
	slotRootParameter[RootSignature::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignature::EUO_Output].InitAsDescriptorTable(1, &texTables[1]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return true;
}

bool GaussianFilterClass::BuildPso() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (UINT i = 0; i < FilterType::Count; ++i) {
		IDxcBlob* cs;
		switch (i) {
		case FilterType::Filter3x3:
			cs = mShaderManager->GetDxcShader("gaussianFilter3x3CS");
			break;
		case FilterType::Filter3x3RG:
			cs = mShaderManager->GetDxcShader("gaussianFilterRG3x3CS");
			break;
		default:
			ReturnFalse(L"Unsupported FilterType");
		}
		psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[static_cast<FilterType>(i)])));
	}

	return true;
}

void GaussianFilterClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_output,
		FilterType type,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	{
		UINT values[2] = { width, height };
		cmdList->SetComputeRoot32BitConstants(RootSignature::EC_Consts, _countof(values), values, 0);
	}
	{
		float values[2] = { 1.0f / width, 1.0f / height };
		cmdList->SetComputeRoot32BitConstants(RootSignature
			::EC_Consts, _countof(values), values, RootSignature::RootConstants::E_InvDimensionX);
	}

	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Input, si_input);
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_Output, uo_output);

	cmdList->Dispatch(D3D12Util::CeilDivide(
		width, Default::ThreadGroup::Width), D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);
}