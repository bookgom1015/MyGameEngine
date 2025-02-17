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

	const CHAR* const CS_R8G8B8A8_H	= "CS_R8G8B8A8_H";
	const CHAR* const CS_R8G8B8A8_V	= "CS_R8G8B8A8_V";
	const CHAR* const CS_R16_H		= "CS_R16_H";
	const CHAR* const CS_R16_V		= "CS_R16_V";
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
		const auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		const auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F4));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F4));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F3", L"1" }
		};
		const auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		const auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F3));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F3));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F2", L"1" }
		};
		const auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		const auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F2));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F2));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F1", L"1" }
		};
		const auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		const auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_GaussianBlur_F1));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_GaussianBlur_F1));
	}
	
	return TRUE;
}

BOOL BlurFilterClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[6]; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);

	index = 0;

	slotRootParameter[RootSignature::Default::ECB_BlurPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::Default::EC_Consts].InitAsConstants(3, 1);
	slotRootParameter[RootSignature::Default::ESI_Normal].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Depth].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Input_F4].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Input_F3].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Input_F2].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Input_F1].InitAsDescriptorTable(1, &texTables[index++]);

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
			const auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			const auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case FilterType::R16:
		{
			const auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F1);
			const auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F1);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UNORM;
			break;
		case FilterType::R16G16B16A16:
		{
			const auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			const auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case FilterType::R32G32B32A32:
		{
			const auto vs = mShaderManager->GetDxcShader(VS_GaussianBlur_F4);
			const auto ps = mShaderManager->GetDxcShader(PS_GaussianBlur_F4);
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

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_BlurPass, cb_blur);
	cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 0, RootConstant::Default::E_Bilateral);

	for (UINT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 1, RootConstant::Default::E_Horizontal);
		Blur(cmdList, secondary, ro_secondary, si_primary, type, TRUE);

		cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 0, RootConstant::Default::E_Horizontal);
		Blur(cmdList, primary, ro_primary, si_secondary, type, FALSE);
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

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_BlurPass, cb_blur);
	cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 1, RootConstant::Default::E_Bilateral);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Depth, si_depth);
	
	for (UINT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 1, RootConstant::Default::E_Horizontal);
		Blur(cmdList, secondary, ro_secondary, si_primary, type, TRUE);

		cmdList->SetGraphicsRoot32BitConstant(RootSignature::Default::EC_Consts, 0, RootConstant::Default::E_Horizontal);
		Blur(cmdList, primary, ro_primary, si_secondary, type, FALSE);
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

	cmdList->OMSetRenderTargets(1, &ro_output, TRUE, nullptr);

	switch (type)
	{
	case BlurFilter::R8G8B8A8:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Input_F4, si_input);
		break;
	case BlurFilter::R16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Input_F1, si_input);
		break;
	case BlurFilter::R16G16B16A16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Input_F4, si_input);
		break;
	case BlurFilter::R32G32B32A32:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Input_F4, si_input);
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

BOOL CS::BlurFilterCSClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL CS::BlurFilterCSClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"GaussianBlurCS.hlsl";
	{
		const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"HorzBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_R8G8B8A8_H));
	}
	{
		const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"VertBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_R8G8B8A8_V));
	}
	{
		const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"HorzBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_R16_H));
	}
	{
		const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"VertBlurCS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_R16_V));
	}

	return TRUE;
}

BOOL CS::BlurFilterCSClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[4]; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	index = 0;

	slotRootParameter[RootSignature::ECB_BlurPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::EC_Consts].InitAsConstants(RootConstant::Default::Count, 1, 0);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_Input].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::EUO_Output].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);
	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL CS::BlurFilterCSClass::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (UINT i = 0; i < Filter::Type::Count; ++i) {
		for (UINT j = 0; j < Direction::Count; ++j) {
			const auto cs = GetShader((Filter::Type)i, (Direction::Type)j);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
			CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[(Filter::Type)i][(Direction::Type)j])));
		}
	}

	return TRUE;
}

void CS::BlurFilterCSClass::Run(
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

	cmdList->SetComputeRootConstantBufferView(RootSignature::ECB_BlurPass, cbAddress);
	
	CS::RootConstant::Default::Struct rc;
	rc.gDimension.x = static_cast<FLOAT>(width);
	rc.gDimension.y = static_cast<FLOAT>(height);

	std::array<std::uint32_t, CS::RootConstant::Default::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(CS::RootConstant::Default::Struct));

	cmdList->SetComputeRoot32BitConstants(RootSignature::EC_Consts, CS::RootConstant::Default::Count, consts.data(), 0);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Depth, si_depth);

	for (UINT i = 0; i < blurCount; ++i) {
		cmdList->SetPipelineState(mPSOs[type][Direction::Horizontal].Get());

		secondary->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Input, si_primary);
		cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_Output, uo_secondary);

		cmdList->Dispatch(D3D12Util::CeilDivide(width, CS::ThreadGroup::Default::Size), height, 1);

		primary->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		secondary->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		cmdList->SetPipelineState(mPSOs[type][Direction::Vertical].Get());

		cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Input, si_secondary);
		cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_Output, uo_primary);

		cmdList->Dispatch(width, D3D12Util::CeilDivide(height, CS::ThreadGroup::Default::Size), 1);

		primary->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

IDxcBlob* CS::BlurFilterCSClass::GetShader(Filter::Type type, Direction::Type direction) {
	if (direction == Direction::Horizontal) {
		switch (type)
		{
		case CS::Filter::R8G8B8A8:
			return mShaderManager->GetDxcShader(CS_R8G8B8A8_H);
		case CS::Filter::R16:
			return mShaderManager->GetDxcShader(CS_R16_H);
		default:
			return nullptr;
		}
	}
	else {
		switch (type)
		{
		case CS::Filter::R8G8B8A8:
			return mShaderManager->GetDxcShader(CS_R8G8B8A8_V);
		case CS::Filter::R16:
			return mShaderManager->GetDxcShader(CS_R16_V);
		default:
			return nullptr;
		}
	}
}