#include "DirectX/Shading/BRDF.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

using namespace BRDF;

namespace {
	const CHAR* const VS_BlinnPhong			= "VS_BlinnPhong";
	const CHAR* const PS_BlinnPhong			= "PS_BlinnPhong";
	const CHAR* const VS_DXR_BlinnPhong		= "VS_DXR_BlinnPhong";
	const CHAR* const PS_DXR_BlinnPhong		= "PS_DXR_BlinnPhong";

	const CHAR* const VS_CookTorrance		= "VS_CookTorrance";
	const CHAR* const PS_CookTorrance		= "PS_CookTorrance";
	const CHAR* const VS_DXR_CookTorrance	= "VS_DXR_rCookTorrance";
	const CHAR* const PS_DXR_CookTorrance	= "PS_DXR_rCookTorrance";

	const CHAR* const VS_IntegrateSpecular	= "VS_IntegrateSpecular";
	const CHAR* const PS_IntegrateSpecular	= "PS_IntegrateSpecular";
}

BRDFClass::BRDFClass() {
	mCopiedBackBuffer = std::make_unique<GpuResource>();
}

BOOL BRDFClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager*const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL BRDFClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"BRDF.hlsl";

		{
			DxcDefine defines[] = {
				{ L"BLINN_PHONG", L"1" }
			};

			const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_BlinnPhong));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_BlinnPhong));
		}
		{
			DxcDefine defines[] = {
				{ L"BLINN_PHONG", L"1" },
				{ L"DXR", L"1" }
			};

			const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DXR_BlinnPhong));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DXR_BlinnPhong));
		}
	}
	{
		const auto fullPath = filePath + L"BRDF.hlsl";

		{
			DxcDefine defines[] = {
				{ L"COOK_TORRANCE", L"1" }
			};

			const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_CookTorrance));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_CookTorrance));
		}
		{
			DxcDefine defines[] = {
				{ L"COOK_TORRANCE", L"1" },
				{ L"DXR", L"1" }
			};

			const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DXR_CookTorrance));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DXR_CookTorrance));
		}
	}
	{
		const auto fullPath = filePath + L"IntegrateSpecular.hlsl";
		const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_IntegrateSpecular));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_IntegrateSpecular));
	}

	return TRUE;
}

BOOL BRDFClass::BuildRootSignature(const StaticSamplers& samplers) {
	D3D12Util::Descriptor::RootSignature::Builder builder;
	// Intetrate diffuse
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[8] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcReflectanceEquation::Count] = {};
		slotRootParameter[RootSignature::CalcReflectanceEquation::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Albedo].InitAsDescriptorTable(	  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Normal].InitAsDescriptorTable(	  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Depth].InitAsDescriptorTable(		  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_RMS].InitAsDescriptorTable(		  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Position].InitAsDescriptorTable(	  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_DiffuseIrrad].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_AOCoeiff].InitAsDescriptorTable(	  1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Shadow].InitAsDescriptorTable(	  1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_CalcReflectanceEquation]));
	}
	// Integrate specular
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[10] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::IntegrateSpecular::Count] = {};
		slotRootParameter[RootSignature::IntegrateSpecular::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_BackBuffer].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Albedo].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Normal].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Depth].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_RMS].InitAsDescriptorTable(			1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Position].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_AOCoeiff].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Prefiltered].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_BrdfLUT].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Reflection].InitAsDescriptorTable(	1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_IntegrateSpecular]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

BOOL BRDFClass::BuildPSO() {
	const auto& diffuseRootSig = mRootSignatures[RootSignature::E_CalcReflectanceEquation].Get();

	D3D12Util::Descriptor::PipelineState::Builder builder;
	// Raster
	{
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				const auto vs = mShaderManager->GetDxcShader(VS_BlinnPhong);
				const auto ps = mShaderManager->GetDxcShader(PS_BlinnPhong);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raster][Model::E_BlinnPhong]));
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				const auto vs = mShaderManager->GetDxcShader(VS_CookTorrance);
				const auto ps = mShaderManager->GetDxcShader(PS_CookTorrance);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raster][Model::E_CookTorrance]));
		}
	}
	// Raytrace
	{
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				const auto vs = mShaderManager->GetDxcShader(VS_DXR_BlinnPhong);
				const auto ps = mShaderManager->GetDxcShader(PS_DXR_BlinnPhong);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raytrace][Model::E_BlinnPhong]));
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				const auto vs = mShaderManager->GetDxcShader(VS_DXR_CookTorrance);
				const auto ps = mShaderManager->GetDxcShader(PS_DXR_CookTorrance);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raytrace][Model::E_CookTorrance]));
		}
	}
	// IntegrateSpecular
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_IntegrateSpecular].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_IntegrateSpecular);
			const auto ps = mShaderManager->GetDxcShader(PS_IntegrateSpecular);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = HDR_FORMAT;

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mIntegrateSpecularPSO));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void BRDFClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhCopiedBackBufferSrvCpu = hCpu.Offset(1, descSize);
	mhCopiedBackBufferSrvGpu = hGpu.Offset(1, descSize);
}

BOOL BRDFClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = HDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	srvDesc.Texture2D.MipLevels = 1;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		device->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferSrvCpu);
	}

	return TRUE;
}

BOOL BRDFClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void BRDFClass::CalcReflectanceWithoutSpecIrrad(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_diffuseIrrad,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
		D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
		Render::Type renderType) {
	cmdList->SetPipelineState(mPSOs[renderType][ModelType].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_CalcReflectanceEquation].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::CalcReflectanceEquation::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Position, si_pos);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_DiffuseIrrad, si_diffuseIrrad);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_AOCoeiff, si_aocoeiff);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Shadow, si_shadow);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void BRDFClass::IntegrateSpecularIrrad(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* const backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
		D3D12_GPU_DESCRIPTOR_HANDLE si_prefiltered,
		D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
		D3D12_GPU_DESCRIPTOR_HANDLE si_reflection) {
	cmdList->SetPipelineState(mIntegrateSpecularPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_IntegrateSpecular].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::IntegrateSpecular::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_BackBuffer, mhCopiedBackBufferSrvGpu);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Position, si_pos);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_AOCoeiff, si_aocoeiff);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Prefiltered, si_prefiltered);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_BrdfLUT, si_brdf);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Reflection, si_reflection);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

BOOL BRDFClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = HDR_FORMAT;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(mCopiedBackBuffer->Initialize(
			device,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			L"BRDF_CopiedBackBufferMap"
		));
	}

	return TRUE;
}