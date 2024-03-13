#include "BlurFilter.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace BlurFilter;

namespace {
	const CHAR* VS_GaussianBlur_F4 = "VS_GaussianBlur_F4";
	const CHAR* PS_GaussianBlur_F4 = "PS_GaussianBlur_F4";

	const CHAR* VS_GaussianBlur_F3 = "VS_GaussianBlur_F3";
	const CHAR* PS_GaussianBlur_F3 = "PS_GaussianBlur_F3";

	const CHAR* VS_GaussianBlur_F2 = "VS_GaussianBlur_F2";
	const CHAR* PS_GaussianBlur_F2 = "PS_GaussianBlur_F2";

	const CHAR* VS_GaussianBlur_F1 = "VS_GaussianBlur_F1";
	const CHAR* PS_GaussianBlur_F1 = "PS_GaussianBlur_F1";
}

BOOL BlurFilterClass::Initialize(ID3D12Device*const device, ShaderManager*const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL BlurFilterClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"GaussianBlur.hlsl";
	
	{
		DxcDefine defines[] = {
			{ L"FT_F4", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F4));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F4));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F3", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F3));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F3));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F2", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F2));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F2));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F1", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F1));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F1));
	}
	
	return TRUE;
}

BOOL BlurFilterClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[6];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);

	slotRootParameter[RootSignature::ECB_BlurPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::EC_Consts].InitAsConstants(3, 1);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignature::ESI_Input_F4].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignature::ESI_Input_F3].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignature::ESI_Input_F2].InitAsDescriptorTable(1, &texTables[4]);
	slotRootParameter[RootSignature::ESI_Input_F1].InitAsDescriptorTable(1, &texTables[5]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL BlurFilterClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc;
	ZeroMemory(&quadPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	quadPsoDesc.InputLayout = { nullptr, 0 };
	quadPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	quadPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	quadPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	quadPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	quadPsoDesc.NumRenderTargets = 1;
	quadPsoDesc.SampleMask = UINT_MAX;
	quadPsoDesc.SampleDesc.Count = 1;
	quadPsoDesc.SampleDesc.Quality = 0;
	quadPsoDesc.DepthStencilState.DepthEnable = FALSE;
	quadPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	quadPsoDesc.pRootSignature = mRootSignature.Get();

	for (UINT i = 0; i < FilterType::Count; ++i) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = quadPsoDesc;
		switch (i) {
		case FilterType::R8G8B8A8:
		{
			auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case FilterType::R16:
		{
			auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F1);
			auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F1);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UNORM;
			break;
		case FilterType::R16G16B16A16:
		{
			auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case FilterType::R32G32B32A32:
		{
			auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		default:
			ReturnFalse(L"Unknown FilterType");
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[(FilterType)i])));
	}

	return TRUE;
}

void BlurFilterClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
		GpuResource* const primary,
		GpuResource* const secondary,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
		FilterType type,
		UINT blurCount) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ECB_BlurPass, cb_blur);
	cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 0, RootSignature::RootConstant::E_Bilateral);

	for (UINT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 1, RootSignature::RootConstant::E_Horizontal);
		Blur(cmdList, secondary, ro_secondary, si_primary, type, true);

		cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 0, RootSignature::RootConstant::E_Horizontal);
		Blur(cmdList, primary, ro_primary, si_secondary, type, false);
	}
}

void BlurFilterClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		GpuResource* const primary,
		GpuResource* const secondary,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
		D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
		FilterType type,
		UINT blurCount) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ECB_BlurPass, cb_blur);
	cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 1, RootSignature::RootConstant::E_Bilateral);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Depth, si_depth);
	
	for (UINT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 1, RootSignature::RootConstant::E_Horizontal);
		Blur(cmdList, secondary, ro_secondary, si_primary, type, true);

		cmdList->SetGraphicsRoot32BitConstant(RootSignature::EC_Consts, 0, RootSignature::RootConstant::E_Horizontal);
		Blur(cmdList, primary, ro_primary, si_secondary, type, false);
	}
}

void BlurFilterClass::Blur(
		ID3D12GraphicsCommandList* cmdList,
		GpuResource* const output,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_output,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		FilterType type,
		BOOL horzBlur) {
	output->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_output, true, nullptr);

	switch (type)
	{
	case BlurFilter::R8G8B8A8:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Input_F4, si_input);
		break;
	case BlurFilter::R16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Input_F1, si_input);
		break;
	case BlurFilter::R16G16B16A16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Input_F4, si_input);
		break;
	case BlurFilter::R32G32B32A32:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Input_F4, si_input);
		break;
	case BlurFilter::Count:
		break;
	default:
		break;
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	output->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}