#include "BRDF.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

using namespace BRDF;

namespace {
	const std::string BlinnPhongVS = "BlinnPhongVS";
	const std::string BlinnPhongPS = "BlinnPhongPS";
	const std::string DxrBlinnPhongVS = "DxrBlinnPhongVS";
	const std::string DxrBlinnPhongPS = "DxrBlinnPhongPS";

	const std::string CookTorranceVS = "CookTorranceVS";
	const std::string CookTorrancePS = "CookTorrancePS";
	const std::string DxrCookTorranceVS = "DxrCookTorranceVS";
	const std::string DxrCookTorrancePS = "DxrCookTorrancePS";

	const std::string IntegrateSpecularVS = "IntegrateSpecularVS";
	const std::string IntegrateSpecularPS = "IntegrateSpecularPS";
}

BRDFClass::BRDFClass() {
	mCopiedBackBuffer = std::make_unique<GpuResource>();

	ModelType = Model::E_CookTorrance;
}

bool BRDFClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	CheckReturn(BuildResources());

	return true;
}

bool BRDFClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"BRDF.hlsl";

		{
			DxcDefine defines[] = {
				{ L"BLINN_PHONG", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, BlinnPhongVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, BlinnPhongPS));
		}
		{
			DxcDefine defines[] = {
				{ L"BLINN_PHONG", L"1" },
				{ L"DXR", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, DxrBlinnPhongVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DxrBlinnPhongPS));
		}
	}
	{
		const auto fullPath = filePath + L"BRDF.hlsl";

		{
			DxcDefine defines[] = {
				{ L"COOK_TORRANCE", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, CookTorranceVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, CookTorrancePS));
		}
		{
			DxcDefine defines[] = {
				{ L"COOK_TORRANCE", L"1" },
				{ L"DXR", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, DxrCookTorranceVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DxrCookTorrancePS));
		}
	}
	{
		const auto fullPath = filePath + L"IntegrateSpecular.hlsl";
		auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, IntegrateSpecularVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, IntegrateSpecularPS));
	}

	return true;
}

bool BRDFClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcReflectanceEquation::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[7];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);

		slotRootParameter[RootSignature::CalcReflectanceEquation::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Albedo].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_RMS].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Shadow].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_AOCoeiff].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::CalcReflectanceEquation::ESI_Diffuse].InitAsDescriptorTable(1, &texTables[6]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_CalcReflectanceEquation]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::IntegrateSpecular::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[9];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);
		texTables[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0);
		texTables[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8, 0);

		slotRootParameter[RootSignature::IntegrateSpecular::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Albedo].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Normal].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Depth].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_RMS].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_AOCoeiff].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Prefiltered].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_BrdfLUT].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[RootSignature::IntegrateSpecular::ESI_Reflection].InitAsDescriptorTable(1, &texTables[8]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_IntegrateSpecular]));
	}

	return true;
}

bool BRDFClass::BuildPso() {
	const auto& diffuseRootSig = mRootSignatures[RootSignature::E_CalcReflectanceEquation].Get();
	{
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				auto vs = mShaderManager->GetDxcShader(BlinnPhongVS);
				auto ps = mShaderManager->GetDxcShader(BlinnPhongPS);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(
				&psoDesc, 
				IID_PPV_ARGS(&mPSOs[Render::E_Raster][Model::Type::E_BlinnPhong]))
			);
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				auto vs = mShaderManager->GetDxcShader(CookTorranceVS);
				auto ps = mShaderManager->GetDxcShader(CookTorrancePS);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(
				&psoDesc, 
				IID_PPV_ARGS(&mPSOs[Render::E_Raster][Model::Type::E_CookTorrance]))
			);
		}
	}
	{
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				auto vs = mShaderManager->GetDxcShader(DxrBlinnPhongVS);
				auto ps = mShaderManager->GetDxcShader(DxrBlinnPhongPS);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(
				&psoDesc, 
				IID_PPV_ARGS(&mPSOs[Render::E_Raytrace][Model::Type::E_BlinnPhong]))
			);
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
			psoDesc.pRootSignature = diffuseRootSig;
			{
				auto vs = mShaderManager->GetDxcShader(DxrCookTorranceVS);
				auto ps = mShaderManager->GetDxcShader(DxrCookTorrancePS);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = HDR_FORMAT;

			CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(
				&psoDesc, 
				IID_PPV_ARGS(&mPSOs[Render::E_Raytrace][Model::Type::E_CookTorrance]))
			);
		}
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_IntegrateSpecular].Get();
		{
			auto vs = mShaderManager->GetDxcShader(IntegrateSpecularVS);
			auto ps = mShaderManager->GetDxcShader(IntegrateSpecularPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = HDR_FORMAT;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS(&mIntegrateSpecularPSO))
		);
	}

	return true;
}

void BRDFClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhCopiedBackBufferSrvCpu = hCpu;
	mhCopiedBackBufferSrvGpu = hGpu;

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);

	BuildDescriptors();
}

bool BRDFClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void BRDFClass::CalcReflectanceWithoutSpecIrrad(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
		D3D12_GPU_DESCRIPTOR_HANDLE si_diffuse,
		Render::Type renderType) {
	cmdList->SetPipelineState(mPSOs[renderType][ModelType].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_CalcReflectanceEquation].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::CalcReflectanceEquation::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Shadow, si_shadow);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_AOCoeiff, si_aocoeiff);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CalcReflectanceEquation::ESI_Diffuse, si_diffuse);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void BRDFClass::IntegrateSpecularIrrad(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
		D3D12_GPU_DESCRIPTOR_HANDLE si_prefiltered,
		D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
		D3D12_GPU_DESCRIPTOR_HANDLE si_reflection) {
	cmdList->SetPipelineState(mIntegrateSpecularPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_IntegrateSpecular].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::IntegrateSpecular::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_BackBuffer, mhCopiedBackBufferSrvGpu);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_AOCoeiff, si_aocoeiff);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Prefiltered, si_prefiltered);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_BrdfLUT, si_brdf);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::IntegrateSpecular::ESI_Reflection, si_reflection);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
}

void BRDFClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = HDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	md3dDevice->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferSrvCpu);
}

bool BRDFClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = HDR_FORMAT;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckReturn(mCopiedBackBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		L"BRDF_CopiedBackBufferMap"
	));

	return true;
}