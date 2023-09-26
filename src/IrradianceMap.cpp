#include "IrradianceMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "GpuResource.h"

#include <ddraw.h>
#include <DDS.h>
#include <ScreenGrab/ScreenGrab12.h>
#include <string>

using namespace DirectX;
using namespace IrradianceMap;

namespace {
	const std::string ConvRectToCubeVS = "ConvRectToCubeVS";
	const std::string ConvRectToCubePS = "ConvRectToCubePS";

	const std::string DrawCubeVS = "DrawCubeVS";
	const std::string DrawCubePS = "DrawCubePS";

	const std::string DrawEquirectangularVS = "DrawEquirectangularVS";
	const std::string DrawEquirectangularPS = "DrawEquirectangularPS";

	const std::string ConvoluteDiffuseIrradianceVS = "ConvoluteDiffuseIrradianceVS";
	const std::string ConvoluteDiffuseIrradiancePS = "ConvoluteDiffuseIrradiancePS";

	const std::string ConvoluteSpecularIrradianceVS = "ConvoluteSpecularIrradianceVS";
	const std::string ConvoluteSpecularIrradiancePS = "ConvoluteSpecularIrradiancePS";

	const std::string IntegrateBrdfVS = "IntegrateBrdfVS";
	const std::string IntegrateBrdfPS = "IntegrateBrdfPS";

	const std::string SkySphereVS = "SkySphereVS";
	const std::string SkySpherePS = "SkySpherePS";

	const std::string ConvCubeToRectVS = "ConvCubeToRectVS";
	const std::string ConvCubeToRectPS = "ConvCubeToRectPS";

	const wchar_t GeneratedDiffuseIrradianceCubeMap[] = L"./../../assets/textures/gen_diffuse_irradiance_cubemap.dds";
	const wchar_t GeneratedEnvironmentCubeMap[] = L"./../../assets/textures/gen_environment_cubemap.dds";
}

namespace {
	const UINT CubeMapSize = 1024;

	const UINT EquirectangularMapWidth = 4096;
	const UINT EquirectangularMapHeight = 2048;

	const UINT IntegratedBrdfMapSize = 1024;
}

IrradianceMapClass::IrradianceMapClass() {
	mEquirectangularMap = std::make_unique<GpuResource>();
	mEnvironmentCubeMap = std::make_unique<GpuResource>();
	mDiffuseIrradianceCubeMap = std::make_unique<GpuResource>();
	mDiffuseIrradianceEquirectMap = std::make_unique<GpuResource>();
	mSpecularIrradianceCubeMap = std::make_unique<GpuResource>();
	mIntegratedBrdfMap = std::make_unique<GpuResource>();

	mCubeMapViewport = { 0.0f, 0.0f, static_cast<float>(CubeMapSize), static_cast<float>(CubeMapSize), 0.0f, 1.0f };
	mCubeMapScissorRect = { 0, 0, static_cast<int>(CubeMapSize), static_cast<int>(CubeMapSize) };

	mIrradEquirectMapViewport = { 0.0f, 0.0f, static_cast<float>(EquirectangularMapWidth), static_cast<float>(EquirectangularMapHeight), 0.0f, 1.0f };
	mIrradEquirectMapScissorRect = { 0, 0, static_cast<int>(EquirectangularMapWidth), static_cast<int>(EquirectangularMapHeight) };

	bNeedToUpdate = false;
	bNeedToSave = false;

	DrawCubeType = DrawCube::E_EnvironmentCube;
}

bool IrradianceMapClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	BuildResources(cmdList);

	return true;
}

bool IrradianceMapClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"ConvertEquirectangularToCubeMap.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ConvRectToCubeVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ConvRectToCubePS));
	}
	{
		const std::wstring actualPath = filePath + L"ConvertCubeMapToEquirectangular.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ConvCubeToRectVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ConvCubeToRectPS));
	}
	{
		const std::wstring actualPath = filePath + L"DrawCubeMap.hlsl";
		{
			DxcDefine defines[] = {
			{ L"SPHERICAL", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, DrawEquirectangularVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DrawEquirectangularPS));
		}
		{
			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
			CheckReturn(mShaderManager->CompileShader(vsInfo, DrawCubeVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DrawCubePS));
		}
	}
	{
		const std::wstring actualPath = filePath + L"ConvoluteDiffuseIrradiance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ConvoluteDiffuseIrradianceVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ConvoluteDiffuseIrradiancePS));
	}
	{
		const std::wstring actualPath = filePath + L"ConvoluteSpecularIrradiance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ConvoluteSpecularIrradianceVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ConvoluteSpecularIrradiancePS));
	}
	{
		const std::wstring actualPath = filePath + L"IntegrateBrdf.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, IntegrateBrdfVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, IntegrateBrdfPS));
	}
	{
		const std::wstring actualPath = filePath + L"SkySphere.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, SkySphereVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, SkySpherePS));
	}

	return true;
}

bool IrradianceMapClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ConvEquirectToCube::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::ECB_ConvEquirectToCube].InitAsConstantBufferView(0);
		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::EC_Consts].InitAsConstants(
			ConvEquirectToCube::RootConstantsLayout::Count,1);
		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvEquirectToCube]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ConvCubeToEquirect::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ConvCubeToEquirect::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvCubeToEquirect]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[DrawCube::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[DrawCube::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[DrawCube::RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[DrawCube::RootSignatureLayout::EC_Consts].InitAsConstants(DrawCube::RootConstantsLayout::Count, 2);
		slotRootParameter[DrawCube::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[DrawCube::RootSignatureLayout::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DrawCube]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ConvoluteDiffuseIrradiance::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ConvoluteDiffuseIrradiance::RootSignatureLayout::ECB_ConvEquirectToConv].InitAsConstantBufferView(0);
		slotRootParameter[ConvoluteDiffuseIrradiance::RootSignatureLayout::EC_Consts].InitAsConstants(
			ConvoluteDiffuseIrradiance::RootConstantsLayout::Count, 1);
		slotRootParameter[ConvoluteDiffuseIrradiance::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ConvoluteSpecularIrradiance::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ConvoluteSpecularIrradiance::RootSignatureLayout::ECB_ConvEquirectToConv].InitAsConstantBufferView(0);
		slotRootParameter[ConvoluteSpecularIrradiance::RootSignatureLayout::EC_Consts].InitAsConstants(
			ConvoluteSpecularIrradiance::RootConstantsLayout::Count, 1);
		slotRootParameter[ConvoluteSpecularIrradiance::RootSignatureLayout::ESI_Environment].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvoluteSpecularIrradiance]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[IntegrateBrdf::RootSignatureLayout::Count];

		slotRootParameter[IntegrateBrdf::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_IntegrateBrdf]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[DrawSkySphere::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[DrawSkySphere::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[DrawSkySphere::RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[DrawSkySphere::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DrawSkySphere]));
	}

	return true;
}

bool IrradianceMapClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat) {
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvEquirectToCube].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ConvRectToCubeVS);
			auto ps = mShaderManager->GetDxcShader(ConvRectToCubePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvEquirectToCube])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvCubeToEquirect].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ConvCubeToRectVS);
			auto ps = mShaderManager->GetDxcShader(ConvCubeToRectPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvCubeToEquirect])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_DrawCube].Get();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::SDRMapFormat;
		psoDesc.DepthStencilState.DepthEnable = FALSE;

		{
			auto vs = mShaderManager->GetDxcShader(DrawCubeVS);
			auto ps = mShaderManager->GetDxcShader(DrawCubePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawCube])));

		{
			auto vs = mShaderManager->GetDxcShader(DrawEquirectangularVS);
			auto ps = mShaderManager->GetDxcShader(DrawEquirectangularPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawEquirectangular])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ConvoluteDiffuseIrradianceVS);
			auto ps = mShaderManager->GetDxcShader(ConvoluteDiffuseIrradiancePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvoluteDiffuseIrradiance])));

		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvoluteSpecularIrradiance].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ConvoluteSpecularIrradianceVS);
			auto ps = mShaderManager->GetDxcShader(ConvoluteSpecularIrradiancePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvoluteSpecularIrradiance])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_IntegrateBrdf].Get();
		{
			auto vs = mShaderManager->GetDxcShader(IntegrateBrdfVS);
			auto ps = mShaderManager->GetDxcShader(IntegrateBrdfPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = IntegratedBrdfMapFormat;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_IntegrateBrdf])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = D3D12Util::DefaultPsoDesc(inputLayout, dsvFormat);
		skyPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DrawSkySphere].Get();
		{
			auto vs = mShaderManager->GetDxcShader(SkySphereVS);
			auto ps = mShaderManager->GetDxcShader(SkySpherePS);
			skyPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			skyPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		skyPsoDesc.NumRenderTargets = 1;
		skyPsoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		skyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		skyPsoDesc.DepthStencilState.StencilEnable = FALSE;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawSkySphere])));
	}

	return true;
}

void IrradianceMapClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhEquirectangularMapCpuSrv = hCpuSrv;
	mhEquirectangularMapGpuSrv = hGpuSrv;

	mhEnvironmentCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhEnvironmentCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDiffuseIrradianceCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDiffuseIrradianceCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDiffuseIrradianceEquirectMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDiffuseIrradianceEquirectMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhSpecularIrradianceCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhSpecularIrradianceCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhIntegratedBrdfMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhIntegratedBrdfMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDiffuseIrradianceEquirectMapCpuRtv = hCpuRtv;
	mhIntegratedBrdfMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		mhEnvironmentCubeMapCpuRtvs[i] = hCpuRtv.Offset(1, rtvDescSize);
	}
	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		mhDiffuseIrradianceCubeMapCpuRtvs[i] = hCpuRtv.Offset(1, rtvDescSize);
	}
	for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
		for (UINT faceID = 0; faceID < CubeMapFace::Count; ++faceID) {
			mhSpecularIrradianceCubeMapCpuRtvs[mipLevel][faceID] = hCpuRtv.Offset(1, rtvDescSize);
		}
	}

	BuildDescriptors();

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);
}


bool IrradianceMapClass::SetEquirectangularMap(ID3D12CommandQueue* const queue,  const std::string& file) {
	auto tex = std::make_unique<Texture>();

	std::wstring filename;
	filename.assign(file.begin(), file.end());

	auto index = filename.rfind(L'.');
	filename = filename.replace(filename.begin() + index, filename.end(), L".dds");

	ResourceUploadBatch resourceUpload(md3dDevice);

	resourceUpload.Begin();

	HRESULT status = DirectX::CreateDDSTextureFromFile(
		md3dDevice,
		resourceUpload,
		filename.c_str(),
		tex->Resource.ReleaseAndGetAddressOf()
	);

	auto finished = resourceUpload.End(queue);
	finished.wait();

	if (FAILED(status)) {
		std::wstringstream wsstream;
		wsstream << "Returned " << std::hex << status << "; when creating texture:  " << filename;
		WLogln(wsstream.str());
		return false;
	}

	auto& resource = tex->Resource;
	tex->Resource->SetName(L"EquirectangularMap");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	mEquirectangularMap->Swap(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	md3dDevice->CreateShaderResourceView(mEquirectangularMap->Resource(), &srvDesc, mhEquirectangularMapCpuSrv);

	bNeedToUpdate = true;

	return true;
}

bool IrradianceMapClass::Update(
		ID3D12CommandQueue* const queue,
		ID3D12DescriptorHeap* descHeap,
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbPass,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		RenderItem* box) {
	if (bNeedToSave) {
		Save(queue, mDiffuseIrradianceEquirectMap.get(), GeneratedDiffuseIrradianceCubeMap);
		bNeedToSave = false;
	}
	if (!bNeedToUpdate) return true;

	ID3D12DescriptorHeap* descriptorHeaps[] = { descHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	ConvertEquirectangularToCube(
		cmdList, 
		mEnvironmentCubeMap.get(), 
		cbConvEquirectToCube, 
		mhEquirectangularMapGpuSrv, 
		mhEnvironmentCubeMapCpuRtvs, 
		box
	);

	IntegrateBrdf(
		cmdList,
		cbPass
	);

	bool exists = Check();
	if (exists) {
		Load(
			queue, 
			mDiffuseIrradianceEquirectMap.get(), 
			mhDiffuseIrradianceEquirectMapCpuSrv, 
			GeneratedDiffuseIrradianceCubeMap, 
			L"DiffuseIrradianceMap");
		ConvertEquirectangularToCube(
			cmdList, 
			mDiffuseIrradianceCubeMap.get(),
			cbConvEquirectToCube, 
			mhDiffuseIrradianceEquirectMapGpuSrv, 
			mhDiffuseIrradianceCubeMapCpuRtvs, 
			box);
	}
	else {
		ConvoluteDiffuse(cmdList, cbConvEquirectToCube, box);
		ConvertCubeToEquirectangular(cmdList);

		bNeedToSave = true;
	}

	ConvoluteSpecular(
		cmdList,
		cbConvEquirectToCube,
		box
	);

	bNeedToUpdate = false;

	return true;
}

bool IrradianceMapClass::DrawCubeMap(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize,
		RenderItem* box,
		float mipLevel) {
	if (DrawCubeType == DrawCube::E_Equirectangular) cmdList->SetPipelineState(mPSOs[PipelineState::E_DrawEquirectangular].Get());
	else cmdList->SetPipelineState(mPSOs[PipelineState::E_DrawCube].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DrawCube].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(DrawCube::RootSignatureLayout::ECB_Pass, cbPassAddress);

	float values[DrawCube::RootConstantsLayout::Count] = { mipLevel };
	cmdList->SetGraphicsRoot32BitConstants(DrawCube::RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	switch (DrawCubeType) {
	case DrawCube::E_EnvironmentCube:
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Cube, mhEnvironmentCubeMapGpuSrv);
		break;
	case DrawCube::E_Equirectangular:
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Equirectangular, mhEquirectangularMapGpuSrv);
		break;
	case DrawCube::E_DiffuseIrradianceCube:
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Cube, mhDiffuseIrradianceCubeMapGpuSrv);
		break;
	case DrawCube::E_SpecularIrradianceCube:
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Cube, mhSpecularIrradianceCubeMapGpuSrv);
	}

	D3D12_GPU_VIRTUAL_ADDRESS boxObjCBAddress = cbObjAddress + box->ObjCBIndex * objCBByteSize;
	cmdList->SetGraphicsRootConstantBufferView(DrawCube::RootSignatureLayout::ECB_Obj, boxObjCBAddress);

	cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
	cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
	cmdList->IASetPrimitiveTopology(box->PrimitiveType);

	cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	return true;
}

bool IrradianceMapClass::DrawSkySphere(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize,
		RenderItem* sphere) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_DrawSkySphere].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DrawSkySphere].Get());
	
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, &dio_dsv);

	cmdList->SetGraphicsRootConstantBufferView(DrawSkySphere::RootSignatureLayout::ECB_Pass, cbPassAddress);
	cmdList->SetGraphicsRootDescriptorTable(DrawSkySphere::RootSignatureLayout::ESI_Cube, mhEnvironmentCubeMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 1, &sphere->Geometry->VertexBufferView());
	cmdList->IASetIndexBuffer(&sphere->Geometry->IndexBufferView());
	cmdList->IASetPrimitiveTopology(sphere->PrimitiveType);

	D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cbObjAddress + sphere->ObjCBIndex * objCBByteSize;
	cmdList->SetGraphicsRootConstantBufferView(DrawSkySphere::RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);

	cmdList->DrawIndexedInstanced(sphere->IndexCount, 1, sphere->StartIndexLocation, sphere->BaseVertexLocation, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	return true;
}


void IrradianceMapClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = D3D12Util::HDRMapFormat;
	
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		
		{
			srvDesc.TextureCube.MipLevels = 1;

			md3dDevice->CreateShaderResourceView(mEnvironmentCubeMap->Resource(), &srvDesc, mhEnvironmentCubeMapCpuSrv);
			md3dDevice->CreateShaderResourceView(mDiffuseIrradianceCubeMap->Resource(), &srvDesc, mhDiffuseIrradianceCubeMapCpuSrv);
		}
		{
			srvDesc.TextureCube.MipLevels = MaxMipLevel;

			md3dDevice->CreateShaderResourceView(mSpecularIrradianceCubeMap->Resource(), &srvDesc, mhSpecularIrradianceCubeMapCpuSrv);
		}
	}
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.MipLevels = 1;

		md3dDevice->CreateShaderResourceView(mDiffuseIrradianceEquirectMap->Resource(), &srvDesc, mhDiffuseIrradianceEquirectMapCpuSrv);
		md3dDevice->CreateShaderResourceView(mIntegratedBrdfMap->Resource(), &srvDesc, mhIntegratedBrdfMapCpuSrv);
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = D3D12Util::HDRMapFormat;	
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		rtvDesc.Texture2DArray.ArraySize = 1;

		{
			rtvDesc.Texture2DArray.MipSlice = 0;

			for (UINT i = 0; i < CubeMapFace::Count; ++i) {
				rtvDesc.Texture2DArray.FirstArraySlice = i;
			
				md3dDevice->CreateRenderTargetView(mEnvironmentCubeMap->Resource(), &rtvDesc, mhEnvironmentCubeMapCpuRtvs[i]);
				md3dDevice->CreateRenderTargetView(mDiffuseIrradianceCubeMap->Resource(), &rtvDesc, mhDiffuseIrradianceCubeMapCpuRtvs[i]);
			}
		}
		{
			for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
				rtvDesc.Texture2DArray.MipSlice = mipLevel;
			
				for (UINT faceID = 0; faceID < CubeMapFace::Count; ++faceID) {
					rtvDesc.Texture2DArray.FirstArraySlice = faceID;
			
					md3dDevice->CreateRenderTargetView(
						mSpecularIrradianceCubeMap->Resource(), 
						&rtvDesc,
						mhSpecularIrradianceCubeMapCpuRtvs[mipLevel][faceID]);
				}
			}
		}
	}
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		md3dDevice->CreateRenderTargetView(mDiffuseIrradianceEquirectMap->Resource(), &rtvDesc, mhDiffuseIrradianceEquirectMapCpuRtv);
		md3dDevice->CreateRenderTargetView(mIntegratedBrdfMap->Resource(), &rtvDesc, mhIntegratedBrdfMapCpuRtv);
	}
}

bool IrradianceMapClass::BuildResources(ID3D12GraphicsCommandList* const cmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Format = D3D12Util::HDRMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		texDesc.Width = CubeMapSize;
		texDesc.Height = CubeMapSize;
		texDesc.DepthOrArraySize = 6;
		texDesc.MipLevels = 1;

		CheckReturn(mEnvironmentCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"EnvironmentCubeMap"
		));
		CheckReturn(mDiffuseIrradianceCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DiffuseIrradianceCubeMap"
		));

		texDesc.MipLevels = 5;

		CheckReturn(mSpecularIrradianceCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"SpecularIrradianceCubeMap"
		));
	}
	{
		texDesc.Width = EquirectangularMapWidth;
		texDesc.Height = EquirectangularMapHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;

		CheckReturn(mDiffuseIrradianceEquirectMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DiffuseIrradianceEquirectMap"
		));
	}
	{
		texDesc.Width = IntegratedBrdfMapSize;
		texDesc.Height = IntegratedBrdfMapSize;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;

		CheckReturn(mIntegratedBrdfMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"IntegratedBrdfMap"
		));
	}

	return true;
}

bool IrradianceMapClass::Check() {
	std::string filePath;
	for (wchar_t ch : GeneratedDiffuseIrradianceCubeMap)
		filePath.push_back(static_cast<char>(ch));

	FILE* file;
	fopen_s(&file, filePath.c_str(), "r");
	if (file == NULL) return false;

	fclose(file);

	return true;
}

bool IrradianceMapClass::Load(
		ID3D12CommandQueue*const queue, 
		GpuResource*const dst, 
		D3D12_CPU_DESCRIPTOR_HANDLE hDesc, 
		LPCWSTR filename, 
		LPCWSTR setname) {
	auto tex = std::make_unique<Texture>();

	ResourceUploadBatch resourceUpload(md3dDevice);

	resourceUpload.Begin();

	HRESULT status = DirectX::CreateDDSTextureFromFile(
		md3dDevice,
		resourceUpload,
		filename,
		tex->Resource.ReleaseAndGetAddressOf()
	);

	auto finished = resourceUpload.End(queue);
	finished.wait();

	if (FAILED(status)) {
		std::wstringstream wsstream;
		wsstream << "Returned " << std::hex << status << "; when creating texture:  " << filename;
		WLogln(wsstream.str());
		return false;
	}

	auto& resource = tex->Resource;
	tex->Resource->SetName(setname);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	dst->Swap(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	md3dDevice->CreateShaderResourceView(dst->Resource(), &srvDesc, hDesc);

	return true;
}

bool IrradianceMapClass::Save(ID3D12CommandQueue* const queue, GpuResource* resource, LPCWSTR filepath) {
	CheckHRESULT(SaveDDSTextureToFile(
		queue, 
		resource->Resource(),
		filepath, 
		resource->State(), 
		resource->State()
	));

	return true;
}

void IrradianceMapClass::ConvertEquirectangularToCube(
	ID3D12GraphicsCommandList* const cmdList,
		GpuResource* resource,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
		D3D12_CPU_DESCRIPTOR_HANDLE* ro_outputs,
		RenderItem* box) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvEquirectToCube].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvEquirectToCube].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(ConvEquirectToCube::RootSignatureLayout::ECB_ConvEquirectToCube, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(ConvEquirectToCube::RootSignatureLayout::ESI_Equirectangular, si_equirectangular);

	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		cmdList->OMSetRenderTargets(1, &ro_outputs[i], true, nullptr);

		cmdList->SetGraphicsRoot32BitConstant(ConvEquirectToCube::RootSignatureLayout::EC_Consts, i, 0);

		cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(box->PrimitiveType);

		cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);
	}

	resource->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::ConvertCubeToEquirectangular(ID3D12GraphicsCommandList* const cmdList) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvCubeToEquirect].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvCubeToEquirect].Get());

	cmdList->RSSetViewports(1, &mIrradEquirectMapViewport);
	cmdList->RSSetScissorRects(1, &mIrradEquirectMapScissorRect);

	mDiffuseIrradianceEquirectMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &mhDiffuseIrradianceEquirectMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(ConvCubeToEquirect::RootSignatureLayout::ESI_Cube, mhDiffuseIrradianceCubeMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	mDiffuseIrradianceEquirectMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::ConvoluteDiffuse(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		RenderItem* box) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvoluteDiffuseIrradiance].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	mDiffuseIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(ConvoluteDiffuseIrradiance::RootSignatureLayout::ECB_ConvEquirectToConv, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(ConvoluteDiffuseIrradiance::RootSignatureLayout::ESI_Cube, mhEnvironmentCubeMapGpuSrv);

	float values[1] = { 0.025f };
	cmdList->SetGraphicsRoot32BitConstants(
		ConvoluteDiffuseIrradiance::RootSignatureLayout::EC_Consts,
		_countof(values), values, 
		ConvoluteDiffuseIrradiance::RootConstantsLayout::E_SampDelta
	);

	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		cmdList->OMSetRenderTargets(1, &mhDiffuseIrradianceCubeMapCpuRtvs[i], true, nullptr);
	
		cmdList->SetGraphicsRoot32BitConstant(
			ConvoluteDiffuseIrradiance::RootSignatureLayout::EC_Consts,
			i, 
			ConvoluteDiffuseIrradiance::RootConstantsLayout::E_FaceID
		);
	
		cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(box->PrimitiveType);
		
		cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);
	}

	mDiffuseIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::ConvoluteSpecular(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		RenderItem* box) {	
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvoluteSpecularIrradiance].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvoluteSpecularIrradiance].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	cmdList->SetGraphicsRootConstantBufferView(
		ConvoluteSpecularIrradiance::RootSignatureLayout::ECB_ConvEquirectToConv, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(
		ConvoluteSpecularIrradiance::RootSignatureLayout::ESI_Environment, mhEnvironmentCubeMapGpuSrv);
	
	mSpecularIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
		float values[1] = { static_cast<float>(0.16667f * mipLevel) };
		cmdList->SetGraphicsRoot32BitConstants(
			ConvoluteSpecularIrradiance::RootSignatureLayout::EC_Consts,
			_countof(values),
			values,
			ConvoluteSpecularIrradiance::RootConstantsLayout::E_Roughness
		);

		for (UINT faceID = 0; faceID < CubeMapFace::Count; ++faceID) {
			cmdList->OMSetRenderTargets(1, &mhSpecularIrradianceCubeMapCpuRtvs[mipLevel][faceID], true, nullptr);

			cmdList->SetGraphicsRoot32BitConstant(
				ConvoluteSpecularIrradiance::RootSignatureLayout::EC_Consts,
				faceID,
				ConvoluteSpecularIrradiance::RootConstantsLayout::E_FaceID
			);

			cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
			cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
			cmdList->IASetPrimitiveTopology(box->PrimitiveType);

			cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);
		}
	}

	mSpecularIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::IntegrateBrdf(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbPass) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_IntegrateBrdf].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_IntegrateBrdf].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	mIntegratedBrdfMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &mhIntegratedBrdfMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(
		IntegrateBrdf::RootSignatureLayout::ECB_Pass, cbPass);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	mIntegratedBrdfMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}