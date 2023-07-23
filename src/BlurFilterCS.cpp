#include "BlurFilterCS.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"

using namespace BlurFilterCS;

namespace {
	std::string R8G8B8A8_H = "horzGaussianBlurCS";
	std::string R8G8B8A8_V = "vertGaussianBlurCS";
	std::string R16_H = "horzGaussianBlurR16CS";
	std::string R16_V = "vertGaussianBlurR16CS";
}

bool BlurFilterCSClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

bool BlurFilterCSClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"GaussianBlurCS.hlsl";
	{
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"HorzBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, R8G8B8A8_H));
	}
	{
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"VertBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, R8G8B8A8_V));
	}
	{
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"HorzBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, R16_H));
	}
	{
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"VertBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, R16_V));
	}

	return true;
}

bool BlurFilterCSClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[4];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	slotRootParameter[RootSignatureLayout::ECB_BlurPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(RootConstantsLayout::Count, 1, 0);
	slotRootParameter[RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Input].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::EUO_Output].InitAsDescriptorTable(1, &texTables[3]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);
	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool BlurFilterCSClass::BuildPso() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (UINT i = 0; i < Filter::Type::Count; ++i) {
		for (UINT j = 0; j < Direction::Count; ++j) {
			auto cs = GetShader((Filter::Type)i, (Direction::Type)j);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
			CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[(Filter::Type)i][(Direction::Type)j])));
		}
	}

	return true;
}

void BlurFilterCSClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		GpuResource* const primary,
		GpuResource* const secondary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_primary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_secondary,
		Filter::Type type,
		UINT width, UINT height,
		size_t blurCount) {
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	cmdList->SetComputeRootConstantBufferView(RootSignatureLayout::ECB_BlurPass, cbAddress);

	UINT values[RootConstantsLayout::Count] = { width, height };
	cmdList->SetComputeRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::ESI_Depth, si_depth);

	for (int i = 0; i < blurCount; ++i) {
		cmdList->SetPipelineState(mPSOs[type][Direction::Horizontal].Get());

		secondary->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::ESI_Input, si_primary);
		cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::EUO_Output, uo_secondary);

		cmdList->Dispatch(D3D12Util::CeilDivide(width, BlurFilterCS::ThreadGroup::Size), height, 1);

		primary->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		secondary->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		cmdList->SetPipelineState(mPSOs[type][Direction::Vertical].Get());

		cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::ESI_Input, si_secondary);
		cmdList->SetComputeRootDescriptorTable(RootSignatureLayout::EUO_Output, uo_primary);

		cmdList->Dispatch(width, D3D12Util::CeilDivide(height, BlurFilterCS::ThreadGroup::Size), 1);

		primary->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

IDxcBlob* BlurFilterCSClass::GetShader(Filter::Type type, Direction::Type direction) {
	if (direction == Direction::Horizontal) {
		switch (type)
		{
		case BlurFilterCS::Filter::R8G8B8A8:
			return mShaderManager->GetDxcShader(R8G8B8A8_H);
		case BlurFilterCS::Filter::R16:
			return mShaderManager->GetDxcShader(R16_H);
		default:
			return nullptr;
		}
	}
	else {
		switch (type)
		{
		case BlurFilterCS::Filter::R8G8B8A8:
			return mShaderManager->GetDxcShader(R8G8B8A8_V);
		case BlurFilterCS::Filter::R16:
			return mShaderManager->GetDxcShader(R16_V);
		default:
			return nullptr;
		}
	}
}