#include "EquirectangularConverter.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "Vertex.h"
#include "RenderItem.h"
#include "GpuResource.h"
#include "RenderItem.h"
#include "DxMesh.h"

using namespace EquirectangularConverter;

namespace {
	const CHAR* const VS_ConvRectToCube = "VS_ConvRectToCube";
	const CHAR* const PS_ConvRectToCube = "PS_ConvRectToCube";

	const CHAR* const VS_ConvCubeToRect = "VS_ConvCubeToRect";
	const CHAR* const PS_ConvCubeToRect = "PS_ConvCubeToRect";
}

BOOL EquirectangularConverterClass::Initialize(ID3D12Device* device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL EquirectangularConverterClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"ConvertEquirectangularToCubeMap.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ConvRectToCube));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ConvRectToCube));
	}
	{
		const std::wstring actualPath = filePath + L"ConvertCubeMapToEquirectangular.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ConvCubeToRect));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ConvCubeToRect));
	}

	return TRUE;
}

BOOL EquirectangularConverterClass::BuildRootSignature(const StaticSamplers& samplers) {
	// ConvEquirectToCube
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ConvEquirectToCube::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[RootSignature::ConvEquirectToCube::ECB_ConvEquirectToCube].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ConvEquirectToCube::EC_Consts].InitAsConstants(RootSignature::ConvEquirectToCube::RootConstant::Count, 1);
		slotRootParameter[RootSignature::ConvEquirectToCube::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvEquirectToCube]));
	}
	// ConvCubeToEquirect
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ConvCubeToEquirect::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[RootSignature::ConvCubeToEquirect::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::ConvCubeToEquirect::EC_Consts].InitAsConstants(
			RootSignature::ConvCubeToEquirect::RootConstant::Count, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvCubeToEquirect]));
	}

	return TRUE;
}

BOOL EquirectangularConverterClass::BuildPSO() {
	// ConvEquirectToCube
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc({ nullptr, 0 }, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvEquirectToCube].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ConvRectToCube);
			auto ps = mShaderManager->GetDxcShader(PS_ConvRectToCube);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.RTVFormats[0] = HDR_FORMAT;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvEquirectToCube])));
	}
	// ConvCubeToEquirect
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvCubeToEquirect].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ConvCubeToRect);
			auto ps = mShaderManager->GetDxcShader(PS_ConvCubeToRect);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = HDR_FORMAT;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvCubeToEquirect])));
	}
	return TRUE;
}

void EquirectangularConverterClass::ConvertEquirectangularToCube(
	ID3D12GraphicsCommandList* const cmdList,
	D3D12_VIEWPORT viewport,
	D3D12_RECT scissorRect,
	GpuResource* resource,
	D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
	D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
	D3D12_CPU_DESCRIPTOR_HANDLE* ro_outputs) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvEquirectToCube].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvEquirectToCube].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ConvEquirectToCube::ECB_ConvEquirectToCube, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ConvEquirectToCube::ESI_Equirectangular, si_equirectangular);

	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		cmdList->OMSetRenderTargets(1, &ro_outputs[i], TRUE, nullptr);

		cmdList->SetGraphicsRoot32BitConstant(RootSignature::ConvEquirectToCube::EC_Consts, i, 0);

		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(36, 1, 0, 0);
	}

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void EquirectangularConverterClass::ConvertEquirectangularToCube(
	ID3D12GraphicsCommandList* const cmdList,
	UINT width, UINT height,
	GpuResource* resource,
	D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
	D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
	CD3DX12_CPU_DESCRIPTOR_HANDLE ro_outputs[][CubeMapFace::Count],
	UINT maxMipLevel) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvEquirectToCube].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvEquirectToCube].Get());

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ConvEquirectToCube::ECB_ConvEquirectToCube, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ConvEquirectToCube::ESI_Equirectangular, si_equirectangular);

	for (UINT mipLevel = 0; mipLevel < maxMipLevel; ++mipLevel) {
		UINT mw = static_cast<UINT>(width / std::pow(2, mipLevel));
		UINT mh = static_cast<UINT>(height / std::pow(2, mipLevel));

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(mw), static_cast<FLOAT>(mh), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<INT>(mw), static_cast<INT>(mh) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		for (UINT face = 0; face < CubeMapFace::Count; ++face) {
			cmdList->OMSetRenderTargets(1, &ro_outputs[mipLevel][face], TRUE, nullptr);

			cmdList->SetGraphicsRoot32BitConstant(RootSignature::ConvEquirectToCube::EC_Consts, face, 0);

			cmdList->IASetVertexBuffers(0, 0, nullptr);
			cmdList->IASetIndexBuffer(nullptr);
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(36, 1, 0, 0);
		}
	}

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void EquirectangularConverterClass::ConvertCubeToEquirectangular(
	ID3D12GraphicsCommandList* const cmdList,
	D3D12_VIEWPORT viewport,
	D3D12_RECT scissorRect,
	GpuResource* equirectResource,
	D3D12_CPU_DESCRIPTOR_HANDLE equirectRtv,
	D3D12_GPU_DESCRIPTOR_HANDLE cubeSrv,
	UINT mipLevel) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvCubeToEquirect].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvCubeToEquirect].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	equirectResource->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &equirectRtv, TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ConvCubeToEquirect::ESI_Cube, cubeSrv);
	cmdList->SetGraphicsRoot32BitConstant(
		RootSignature::ConvCubeToEquirect::EC_Consts, mipLevel, RootSignature::ConvCubeToEquirect::RootConstant::E_MipLevel);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	equirectResource->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
