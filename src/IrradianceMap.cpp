#include "IrradianceMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "GpuResource.h"
#include "Vertex.h"
#include "EquirectangularConverter.h"

#include <ddraw.h>
#include <DDS.h>
#include <ScreenGrab/ScreenGrab12.h>
#include <string>

using namespace DirectX;
using namespace IrradianceMap;

namespace {
	const CHAR* const VS_DrawCube = "VS_DrawCube";
	const CHAR* const PS_DrawCube = "PS_DrawCube";

	const CHAR* const VS_DrawEquirectangular = "VS_DrawEquirectangular";
	const CHAR* const PS_DrawEquirectangular = "PS_DrawEquirectangular";

	const CHAR* const VS_ConvoluteDiffuseIrradiance = "VS_ConvoluteDiffuseIrradiance";
	const CHAR* const GS_ConvoluteDiffuseIrradiance = "GS_ConvoluteDiffuseIrradiance";
	const CHAR* const PS_ConvoluteDiffuseIrradiance = "PS_ConvoluteDiffuseIrradiance";

	const CHAR* const VS_ConvoluteSpecularIrradiance = "VS_ConvoluteSpecularIrradiance";
	const CHAR* const GS_ConvoluteSpecularIrradiance = "GS_ConvoluteSpecularIrradiance";
	const CHAR* const PS_ConvoluteSpecularIrradiance = "PS_ConvoluteSpecularIrradiance";

	const CHAR* const VS_IntegrateBRDF = "VS_IntegrateBRDF";
	const CHAR* const PS_IntegrateBRDF = "PS_IntegrateBRDF";

	const CHAR* const VS_SkySphere = "VS_SkySphere";
	const CHAR* const PS_SkySphere = "PS_SkySphere";

	const std::wstring GenDiffuseIrradianceCubeMap = L"./../../assets/textures/gen_diffuse_irradiance_cubemap.dds";
	const std::wstring GenPrefilteredEnvironmentCubeMaps[5] = {
		L"./../../assets/textures/gen_prefiltered_environment_cubemap_ml0.dds",
		L"./../../assets/textures/gen_prefiltered_environment_cubemap_ml1.dds",
		L"./../../assets/textures/gen_prefiltered_environment_cubemap_ml2.dds",
		L"./../../assets/textures/gen_prefiltered_environment_cubemap_ml3.dds",
		L"./../../assets/textures/gen_prefiltered_environment_cubemap_ml4.dds"
	};
	const std::wstring GenIntegratedBrdfMap = L"./../../assets/textures/gen_integrated_brdf_map.dds";
	const std::wstring GenEnvironmentCubeMap = L"./../../assets/textures/gen_environment_cubemap.dds";
}

namespace {
	const UINT CubeMapSize = 1024;

	const UINT EquirectangularMapWidth = 4096;
	const UINT EquirectangularMapHeight = 2048;

	const UINT IntegratedBrdfMapSize = 1024;
}

IrradianceMapClass::IrradianceMapClass() {
	mTemporaryEquirectangularMap = std::make_unique<GpuResource>();
	mEquirectangularMap = std::make_unique<GpuResource>();
	mEnvironmentCubeMap = std::make_unique<GpuResource>();
	mDiffuseIrradianceCubeMap = std::make_unique<GpuResource>();
	mDiffuseIrradianceEquirectMap = std::make_unique<GpuResource>();
	mPrefilteredEnvironmentCubeMap = std::make_unique<GpuResource>();
	mIntegratedBrdfMap = std::make_unique<GpuResource>();
	for (UINT i = 0; i < MaxMipLevel; ++i)
		mPrefilteredEnvironmentEquirectMaps[i] = std::make_unique<GpuResource>();

	mCubeMapViewport = { 0.0f, 0.0f, static_cast<FLOAT>(CubeMapSize), static_cast<FLOAT>(CubeMapSize), 0.0f, 1.0f };
	mCubeMapScissorRect = { 0, 0, static_cast<INT>(CubeMapSize), static_cast<INT>(CubeMapSize) };

	mIrradEquirectMapViewport = { 0.0f, 0.0f, static_cast<FLOAT>(EquirectangularMapWidth), static_cast<FLOAT>(EquirectangularMapHeight), 0.0f, 1.0f };
	mIrradEquirectMapScissorRect = { 0, 0, static_cast<INT>(EquirectangularMapWidth), static_cast<INT>(EquirectangularMapHeight) };
}

UINT IrradianceMapClass::Size() const {
	return CubeMapSize;
}

BOOL IrradianceMapClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	BuildResources(cmdList);

	return TRUE;
}

BOOL IrradianceMapClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"DebugCubeMap.hlsl";
		{
			DxcDefine defines[] = {
			{ L"SPHERICAL", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DrawEquirectangular));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DrawEquirectangular));
		}
		{
			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DrawCube));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DrawCube));
		}
	}
	{
		const std::wstring actualPath = filePath + L"ConvoluteDiffuseIrradiance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto gsInfo = D3D12ShaderInfo(actualPath.c_str(), L"GS", L"gs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ConvoluteDiffuseIrradiance));
		CheckReturn(mShaderManager->CompileShader(gsInfo, GS_ConvoluteDiffuseIrradiance));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ConvoluteDiffuseIrradiance));
	}
	{
		const std::wstring actualPath = filePath + L"ConvoluteSpecularIrradiance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto gsInfo = D3D12ShaderInfo(actualPath.c_str(), L"GS", L"gs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ConvoluteSpecularIrradiance));
		CheckReturn(mShaderManager->CompileShader(gsInfo, GS_ConvoluteSpecularIrradiance));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ConvoluteSpecularIrradiance));
	}
	{
		const std::wstring actualPath = filePath + L"IntegrateBRDF.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_IntegrateBRDF));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_IntegrateBRDF));
	}
	{
		const std::wstring actualPath = filePath + L"SkySphere.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_SkySphere));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_SkySphere));
	}

	return TRUE;
}

BOOL IrradianceMapClass::BuildRootSignature(const StaticSamplers& samplers) {
	// DebugCube
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::DebugCube::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[RootSignature::DebugCube::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::DebugCube::EC_Consts].InitAsConstants(RootSignature::DebugCube::RootConstant::Count, 2);
		slotRootParameter[RootSignature::DebugCube::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::DebugCube::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DebugCube]));
	}
	// ConvoluteDiffuseIrradiance
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ConvoluteDiffuseIrradiance::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[RootSignature::ConvoluteDiffuseIrradiance::ECB_ConvEquirectToConv].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ConvoluteDiffuseIrradiance::EC_Consts].InitAsConstants(
			RootSignature::ConvoluteDiffuseIrradiance::RootConstant::Count, 1);
		slotRootParameter[RootSignature::ConvoluteDiffuseIrradiance::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance]));
	}
	// ConvoluteSpecularIrradiance
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ConvoluteSpecularIrradiance::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[RootSignature::ConvoluteSpecularIrradiance::ECB_ConvEquirectToConv].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ConvoluteSpecularIrradiance::EC_Consts].InitAsConstants(
			RootSignature::ConvoluteSpecularIrradiance::RootConstant::Count, 1);
		slotRootParameter[RootSignature::ConvoluteSpecularIrradiance::ESI_Environment].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ConvolutePrefilteredIrradiance]));
	}
	// IntegrateBRDF
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::IntegrateBRDF::Count];

		slotRootParameter[RootSignature::IntegrateBRDF::ECB_Pass].InitAsConstantBufferView(0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_IntegrateBRDF]));
	}
	// DrawSkySphere
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::DrawSkySphere::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[RootSignature::DrawSkySphere::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::DrawSkySphere::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[RootSignature::DrawSkySphere::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DrawSkySphere]));
	}

	return TRUE;
}

BOOL IrradianceMapClass::BuildPSO() {
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { nullptr, 0 };
	// DebugCube
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayoutDesc, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_DebugCube].Get();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = SDR_FORMAT;
		psoDesc.DepthStencilState.DepthEnable = FALSE;

		{
			auto vs = mShaderManager->GetDxcShader(VS_DrawCube);
			auto ps = mShaderManager->GetDxcShader(PS_DrawCube);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DebugCube])));

		{
			auto vs = mShaderManager->GetDxcShader(VS_DrawEquirectangular);
			auto ps = mShaderManager->GetDxcShader(PS_DrawEquirectangular);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DebugEquirectangular])));
	}
	// ConvoluteDiffuseIrradiance & ConvoluteSpecularIrradiance
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayoutDesc, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ConvoluteDiffuseIrradiance);
			auto gs = mShaderManager->GetDxcShader(GS_ConvoluteDiffuseIrradiance);
			auto ps = mShaderManager->GetDxcShader(PS_ConvoluteDiffuseIrradiance);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = HDR_FORMAT;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvoluteDiffuseIrradiance])));

		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ConvolutePrefilteredIrradiance].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ConvoluteSpecularIrradiance);
			auto gs = mShaderManager->GetDxcShader(GS_ConvoluteSpecularIrradiance);
			auto ps = mShaderManager->GetDxcShader(PS_ConvoluteSpecularIrradiance);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvolutePrefilteredIrradiance])));
	}
	// IntegrateBRDF
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_IntegrateBRDF].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_IntegrateBRDF);
			auto ps = mShaderManager->GetDxcShader(PS_IntegrateBRDF);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = IntegratedBrdfMapFormat;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_IntegrateBRDF])));
	}
	// DrawSkySphere
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), DepthStencilBuffer::BufferFormat);
		skyPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DrawSkySphere].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_SkySphere);
			auto ps = mShaderManager->GetDxcShader(PS_SkySphere);
			skyPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			skyPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		skyPsoDesc.NumRenderTargets = 1;
		skyPsoDesc.RTVFormats[0] = HDR_FORMAT;
		skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		skyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		skyPsoDesc.DepthStencilState.StencilEnable = FALSE;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawSkySphere])));
	}

	return TRUE;
}

void IrradianceMapClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhEquirectangularMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhEquirectangularMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhTemporaryEquirectangularMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhTemporaryEquirectangularMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhEnvironmentCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhEnvironmentCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDiffuseIrradianceCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDiffuseIrradianceCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhDiffuseIrradianceEquirectMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDiffuseIrradianceEquirectMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhPrefilteredEnvironmentCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhPrefilteredEnvironmentCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	mhIntegratedBrdfMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhIntegratedBrdfMapGpuSrv = hGpuSrv.Offset(1, descSize);

	for (UINT i = 0; i < MaxMipLevel; ++i) {
		mhPrefilteredEnvironmentEquirectMapCpuSrvs[i] = hCpuSrv.Offset(1, descSize);
		mhPrefilteredEnvironmentEquirectMapGpuSrvs[i] = hGpuSrv.Offset(1, descSize);
	}
	
	mhDiffuseIrradianceEquirectMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);
	mhIntegratedBrdfMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
		mhEnvironmentCubeMapCpuRtvs[mipLevel] = hCpuRtv.Offset(1, rtvDescSize);
	}
	mhDiffuseIrradianceCubeMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);
	for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
		mhPrefilteredEnvironmentCubeMapCpuRtvs[mipLevel] = hCpuRtv.Offset(1, rtvDescSize);
	}
	for (UINT i = 0; i < MaxMipLevel; ++i) {
		mhPrefilteredEnvironmentEquirectMapCpuRtvs[i] = hCpuRtv.Offset(1, descSize);
	}
	for (UINT i = 0; i < MaxMipLevel; ++i) {
		mhEquirectangularMapCpuRtvs[i] = hCpuRtv.Offset(1, rtvDescSize);
	}

	BuildDescriptors();
}

BOOL IrradianceMapClass::SetEquirectangularMap(ID3D12CommandQueue* const queue, const std::string& file) {
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
		return FALSE;
	}

	mTemporaryEquirectangularMap->Swap(tex->Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mTemporaryEquirectangularMap->Resource()->SetName(L"Irradiance_TemporaryEquirectangularMap");

	bNeedToUpdate = TRUE;

	return TRUE;
}

BOOL IrradianceMapClass::Update(
		ID3D12CommandQueue* const queue,
		ID3D12DescriptorHeap* descHeap,
		ID3D12GraphicsCommandList* const cmdList,
		EquirectangularConverter::EquirectangularConverterClass* converter,
		D3D12_GPU_VIRTUAL_ADDRESS cbPass,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		MipmapGenerator::MipmapGeneratorClass* const generator) {
	if (mNeedToSave & Save::E_DiffuseIrradiance) {
		Save(queue, mDiffuseIrradianceEquirectMap.get(), GenDiffuseIrradianceCubeMap);

		mNeedToSave = static_cast<Save::Type>(mNeedToSave & ~Save::E_DiffuseIrradiance);

		WLogln(L"Saved diffuse irradiance cubemap.");
	}
	if (mNeedToSave & Save::E_IntegratedBRDF) {
		Save(queue, mIntegratedBrdfMap.get(), GenIntegratedBrdfMap);

		mNeedToSave = static_cast<Save::Type>(mNeedToSave & ~Save::E_IntegratedBRDF);

		WLogln(L"Saved integrated BRDF map.");
	}
	for (UINT i = 0; i < MaxMipLevel; ++i) {
		Save::Type type = static_cast<Save::Type>(Save::E_PrefilteredL0 << i);
		if (mNeedToSave & type) {
			Save(queue, mPrefilteredEnvironmentEquirectMaps[i].get(), GenPrefilteredEnvironmentCubeMaps[i]);

			mNeedToSave = static_cast<Save::Type>(mNeedToSave & ~type);

			WLogln(L"Saved prefiltered environment cubemap mip-level: ", std::to_wstring(i), L".");
		}
	}

	if (!bNeedToUpdate) return TRUE;

	{
		auto desc = mTemporaryEquirectangularMap->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Format = desc.Format;
		md3dDevice->CreateShaderResourceView(mTemporaryEquirectangularMap->Resource(), &srvDesc, mhTemporaryEquirectangularMapCpuSrv);
	}

	ID3D12DescriptorHeap* descriptorHeaps[] = { descHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	{
		generator->GenerateMipmap(
			cmdList, 
			mEquirectangularMap.get(),
			mhTemporaryEquirectangularMapGpuSrv,
			mhEquirectangularMapCpuRtvs, 
			EquirectangularMapWidth, 
			EquirectangularMapHeight, 
			MaxMipLevel);
	
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.Texture2D.MipLevels = MaxMipLevel;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Format = HDR_FORMAT;
			md3dDevice->CreateShaderResourceView(mEquirectangularMap->Resource(), &srvDesc, mhEquirectangularMapCpuSrv);
		}

		converter->ConvertEquirectangularToCube(
			cmdList,
			CubeMapSize,
			CubeMapSize,
			mEnvironmentCubeMap.get(),
			cbConvEquirectToCube,
			mhEquirectangularMapGpuSrv,
			mhEnvironmentCubeMapCpuRtvs,
			MaxMipLevel
		);
	}
	{
		BOOL exists = Check(GenDiffuseIrradianceCubeMap);
		if (exists) {
			WLogln(L"Pre-generated diffuse irradiance cubemap detected. Loading it...");

			Load(
				queue,
				mDiffuseIrradianceEquirectMap.get(),
				mhDiffuseIrradianceEquirectMapCpuSrv,
				GenDiffuseIrradianceCubeMap,
				L"DiffuseIrradianceMap");
			converter->ConvertEquirectangularToCube(
				cmdList,
				mCubeMapViewport,
				mCubeMapScissorRect,
				mDiffuseIrradianceCubeMap.get(),
				cbConvEquirectToCube,
				mhDiffuseIrradianceEquirectMapGpuSrv,
				mhDiffuseIrradianceCubeMapCpuRtv);
		}
		else {
			WLogln(L"Pre-generated diffuse irradiance cubemap doesn't detected. Generating new one...");\

			GenerateDiffuseIrradiance(cmdList, cbConvEquirectToCube);
			converter->ConvertCubeToEquirectangular(
				cmdList, 
				mIrradEquirectMapViewport, 
				mIrradEquirectMapScissorRect,
				mDiffuseIrradianceEquirectMap.get(),
				mhDiffuseIrradianceEquirectMapCpuRtv,
				mhDiffuseIrradianceCubeMapGpuSrv);

			mNeedToSave = static_cast<Save::Type>(mNeedToSave | Save::E_DiffuseIrradiance);
		}
	}
	{
		BOOL exists = Check(GenIntegratedBrdfMap);
		if (exists) {
			WLogln(L"Pre-generated integrated BRDF map detected. Loading it...");

			Load(
				queue,
				mIntegratedBrdfMap.get(),
				mhIntegratedBrdfMapCpuSrv,
				GenIntegratedBrdfMap,
				L"IntegratedBrdfMap");
		}
		else {
			WLogln(L"Pre-generated integrated BRDF map doesn't detected. Generating new one..."); \

				GenerateIntegratedBrdf(
				cmdList,
				cbPass
			);

			mNeedToSave = static_cast<Save::Type>(mNeedToSave | Save::E_IntegratedBRDF);
		}
	}
	{
		BOOL defected = false;
		for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
			BOOL exists = Check(GenPrefilteredEnvironmentCubeMaps[mipLevel]);
			if (!exists) {
				mNeedToSave = static_cast<Save::Type>(mNeedToSave | (Save::E_PrefilteredL0 << mipLevel));
				defected = true;
			}
		}

		if (defected) {			
			WLogln(L"Pre-generated prefiltered environment cubemap doesn't detected. Generating new one...");

			GeneratePrefilteredEnvironment(cmdList, cbConvEquirectToCube);

			for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
				if (mNeedToSave & (Save::E_PrefilteredL0 << mipLevel)) {
					UINT width = static_cast<UINT>(EquirectangularMapWidth / std::pow(2.0, mipLevel));
					UINT height = static_cast<UINT>(EquirectangularMapHeight / std::pow(2.0, mipLevel));

					D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height) , 0.0f, 1.0f };
					D3D12_RECT rect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };

					converter->ConvertCubeToEquirectangular(
						cmdList,
						viewport,
						rect,
						mPrefilteredEnvironmentEquirectMaps[mipLevel].get(),
						mhPrefilteredEnvironmentEquirectMapCpuRtvs[mipLevel],
						mhPrefilteredEnvironmentCubeMapGpuSrv,
						mipLevel);
				}
			}
		}
		else {
			WLogln(L"Pre-generated prefiltered environment cubemap detected. Loading it...");

			for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
				std::wstring name = L"PrefilteredEnvironmentEquirectMap_";
				name.append(std::to_wstring(mipLevel));

				UINT size = static_cast<UINT>(CubeMapSize / std::pow(2.0, mipLevel));

				D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(size), static_cast<FLOAT>(size) , 0.0f, 1.0f };
				D3D12_RECT rect = { 0, 0, static_cast<INT>(size), static_cast<INT>(size) };

				Load(
					queue,
					mPrefilteredEnvironmentEquirectMaps[mipLevel].get(),
					mhPrefilteredEnvironmentEquirectMapCpuSrvs[mipLevel],
					GenPrefilteredEnvironmentCubeMaps[mipLevel],
					name.c_str());
				converter->ConvertEquirectangularToCube(
					cmdList,
					viewport,
					rect,
					mPrefilteredEnvironmentCubeMap.get(),
					cbConvEquirectToCube,
					mhPrefilteredEnvironmentEquirectMapGpuSrvs[mipLevel],
					mhPrefilteredEnvironmentCubeMapCpuRtvs[mipLevel]);
			}
		}
	}

	WLogln(L"");

	bNeedToUpdate = FALSE;

	return TRUE;
}

BOOL IrradianceMapClass::DebugCubeMap(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		FLOAT mipLevel) {
	if (DrawCubeType == DebugCube::E_Equirectangular) cmdList->SetPipelineState(mPSOs[PipelineState::E_DebugEquirectangular].Get());
	else cmdList->SetPipelineState(mPSOs[PipelineState::E_DebugCube].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DebugCube].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::DebugCube::ECB_Pass, cbPassAddress);

	FLOAT values[RootSignature::DebugCube::RootConstant::Count] = { mipLevel };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::DebugCube::EC_Consts, _countof(values), values, 0);

	switch (DrawCubeType) {
	case DebugCube::E_EnvironmentCube:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCube::ESI_Cube, mhEnvironmentCubeMapGpuSrv);
		break;
	case DebugCube::E_Equirectangular:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCube::ESI_Equirectangular, mhEquirectangularMapGpuSrv);
		break;
	case DebugCube::E_DiffuseIrradianceCube:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCube::ESI_Cube, mhDiffuseIrradianceCubeMapGpuSrv);
		break;
	case DebugCube::E_PrefilteredIrradianceCube:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCube::ESI_Cube, mhPrefilteredEnvironmentCubeMapGpuSrv);
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(36, 1, 0, 0);

	return TRUE;
}

BOOL IrradianceMapClass::DrawSkySphere(
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

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::DrawSkySphere::ECB_Pass, cbPassAddress);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::DrawSkySphere::ESI_Cube, mhEnvironmentCubeMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 1, &sphere->Geometry->VertexBufferView());
	cmdList->IASetIndexBuffer(&sphere->Geometry->IndexBufferView());
	cmdList->IASetPrimitiveTopology(sphere->PrimitiveType);

	D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cbObjAddress + sphere->ObjCBIndex * objCBByteSize;
	cmdList->SetGraphicsRootConstantBufferView(RootSignature::DrawSkySphere::ECB_Obj, currRitemObjCBAddress);

	cmdList->DrawIndexedInstanced(sphere->IndexCount, 1, sphere->StartIndexLocation, sphere->BaseVertexLocation, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	return TRUE;
}

void IrradianceMapClass::BuildDescriptors() {	
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = HDR_FORMAT;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		
		{
			srvDesc.TextureCube.MipLevels = 1;

			md3dDevice->CreateShaderResourceView(mDiffuseIrradianceCubeMap->Resource(), &srvDesc, mhDiffuseIrradianceCubeMapCpuSrv);
		}
		{
			srvDesc.TextureCube.MipLevels = MaxMipLevel;

			md3dDevice->CreateShaderResourceView(mEnvironmentCubeMap->Resource(), &srvDesc, mhEnvironmentCubeMapCpuSrv);
			md3dDevice->CreateShaderResourceView(mPrefilteredEnvironmentCubeMap->Resource(), &srvDesc, mhPrefilteredEnvironmentCubeMapCpuSrv);
		}
	}
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = HDR_FORMAT;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.MipLevels = 1;

		{
			md3dDevice->CreateShaderResourceView(mDiffuseIrradianceEquirectMap->Resource(), &srvDesc, mhDiffuseIrradianceEquirectMapCpuSrv);
		}
		{
			for (UINT i = 0; i < MaxMipLevel; ++i)
				md3dDevice->CreateShaderResourceView(
					mPrefilteredEnvironmentEquirectMaps[i]->Resource(), &srvDesc, mhPrefilteredEnvironmentEquirectMapCpuSrvs[i]);
		}
		{
			srvDesc.Format = IntegratedBrdfMapFormat;
			md3dDevice->CreateShaderResourceView(mIntegratedBrdfMap->Resource(), &srvDesc, mhIntegratedBrdfMapCpuSrv);
		}
	}	
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = HDR_FORMAT;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = 0;

		{
			rtvDesc.Texture2DArray.ArraySize = 6;

			{
				rtvDesc.Texture2DArray.MipSlice = 0;

				md3dDevice->CreateRenderTargetView(mDiffuseIrradianceCubeMap->Resource(), &rtvDesc, mhDiffuseIrradianceCubeMapCpuRtv);
			}
			for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
				rtvDesc.Texture2DArray.MipSlice = mipLevel;

				md3dDevice->CreateRenderTargetView(mEnvironmentCubeMap->Resource(), &rtvDesc, mhEnvironmentCubeMapCpuRtvs[mipLevel]);
			}
			for (UINT mipLevel = 0; mipLevel < MaxMipLevel; ++mipLevel) {
				rtvDesc.Texture2DArray.MipSlice = mipLevel;

				md3dDevice->CreateRenderTargetView(
					mPrefilteredEnvironmentCubeMap->Resource(),
					&rtvDesc,
					mhPrefilteredEnvironmentCubeMapCpuRtvs[mipLevel]);
			}
		}
	}
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = HDR_FORMAT;
		rtvDesc.Texture2D.PlaneSlice = 0;

		for (UINT i = 0; i < MaxMipLevel; ++i) {
			rtvDesc.Texture2D.MipSlice = i;

			md3dDevice->CreateRenderTargetView(mEquirectangularMap->Resource(), &rtvDesc, mhEquirectangularMapCpuRtvs[i]);
		}
		{
			rtvDesc.Texture2D.MipSlice = 0;
			md3dDevice->CreateRenderTargetView(mDiffuseIrradianceEquirectMap->Resource(), &rtvDesc, mhDiffuseIrradianceEquirectMapCpuRtv);
		}
		for (UINT i = 0; i < MaxMipLevel; ++i) {
			md3dDevice->CreateRenderTargetView(
				mPrefilteredEnvironmentEquirectMaps[i]->Resource(), &rtvDesc, mhPrefilteredEnvironmentEquirectMapCpuRtvs[i]);
		}
		{
			rtvDesc.Format = IntegratedBrdfMapFormat;
			rtvDesc.Texture2D.MipSlice = 0;
			md3dDevice->CreateRenderTargetView(mIntegratedBrdfMap->Resource(), &rtvDesc, mhIntegratedBrdfMapCpuRtv);
		}
	}
}

BOOL IrradianceMapClass::BuildResources(ID3D12GraphicsCommandList* const cmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		texDesc.Width = CubeMapSize;
		texDesc.Height = CubeMapSize;
		texDesc.DepthOrArraySize = 6;

		texDesc.MipLevels = 1;
		texDesc.Format = DiffuseIrradCubeMapFormat;
		CheckReturn(mDiffuseIrradianceCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Irradiance_DiffuseIrradianceCubeMap"
		));

		texDesc.MipLevels = MaxMipLevel;
		texDesc.Format = EnvCubeMapFormat;
		CheckReturn(mEnvironmentCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Irradiance_EnvironmentCubeMap"
		));

		texDesc.MipLevels = MaxMipLevel;
		texDesc.Format = PrefilteredEnvCubeMapFormat;
		CheckReturn(mPrefilteredEnvironmentCubeMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Irradiance_PrefilteredEnvironmentCubeMap"
		));
	}
	{
		texDesc.Width = EquirectangularMapWidth;
		texDesc.Height = EquirectangularMapHeight;
		texDesc.DepthOrArraySize = 1;

		texDesc.MipLevels = 1;
		texDesc.Format = DiffuseIrradEquirectMapFormat;
		CheckReturn(mDiffuseIrradianceEquirectMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Irradiance_DiffuseIrradianceEquirectangularMap"
		));

		texDesc.MipLevels = MaxMipLevel;
		texDesc.Format = EquirectMapFormat;
		CheckReturn(mEquirectangularMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Irradiance_EquirectangularMap"
		));
	}
	{
		texDesc.Format = PrefilteredEnvEquirectMapFormat;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;

		wchar_t name[] = L"Irradiance_PrefilteredEnvironmentEquirectangularMap_";
		for (UINT i = 0; i < MaxMipLevel; ++i) {
			double denom = 1 / std::pow(2, i);
			texDesc.Width = static_cast<UINT>(EquirectangularMapWidth * denom);
			texDesc.Height = static_cast<UINT>(EquirectangularMapHeight * denom);

			std::wstringstream wsstream;
			wsstream << name << (i + 1);

			CheckReturn(mPrefilteredEnvironmentEquirectMaps[i]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				wsstream.str().c_str()
			));
		}
	}
	{
		texDesc.Format = IntegratedBrdfMapFormat;
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
			L"Irradiance_IntegratedBRDFMap"
		));
	}

	return TRUE;
}

BOOL IrradianceMapClass::Check(const std::wstring& filepath) {
	std::string filePath;
	for (WCHAR ch : filepath)
		filePath.push_back(static_cast<CHAR>(ch));

	FILE* file;
	fopen_s(&file, filePath.c_str(), "r");
	if (file == NULL) return FALSE;

	fclose(file);

	return TRUE;
}

BOOL IrradianceMapClass::Load(
		ID3D12CommandQueue*const queue, 
		GpuResource*const dst, 
		D3D12_CPU_DESCRIPTOR_HANDLE hDesc, 
		const std::wstring& filepath,
		LPCWSTR setname) {
	auto tex = std::make_unique<Texture>();

	ResourceUploadBatch resourceUpload(md3dDevice);

	resourceUpload.Begin();

	HRESULT status = DirectX::CreateDDSTextureFromFile(
		md3dDevice,
		resourceUpload,
		filepath.c_str(),
		tex->Resource.ReleaseAndGetAddressOf()
	);

	auto finished = resourceUpload.End(queue);
	finished.wait();

	if (FAILED(status)) {
		std::wstringstream wsstream;
		wsstream << "Returned " << std::hex << status << "; when creating texture:  " << filepath;
		WLogln(wsstream.str());
		return FALSE;
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

	return TRUE;
}

BOOL IrradianceMapClass::Save(ID3D12CommandQueue* const queue, GpuResource* resource, const std::wstring& filepath) {
	CheckHRESULT(SaveDDSTextureToFile(
		queue, 
		resource->Resource(),
		filepath.c_str(), 
		resource->State(), 
		resource->State()
	));

	return TRUE;
}

void IrradianceMapClass::GenerateDiffuseIrradiance(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvoluteDiffuseIrradiance].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvoluteDiffuseIrradiance].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	mDiffuseIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ConvoluteDiffuseIrradiance::ECB_ConvEquirectToConv, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ConvoluteDiffuseIrradiance::ESI_Cube, mhEnvironmentCubeMapGpuSrv);

	FLOAT values[1] = { 0.025f };
	cmdList->SetGraphicsRoot32BitConstants(
		RootSignature::ConvoluteDiffuseIrradiance::EC_Consts,
		_countof(values), values, 
		RootSignature::ConvoluteDiffuseIrradiance::RootConstant::E_SampDelta
	);

	cmdList->OMSetRenderTargets(1, &mhDiffuseIrradianceCubeMapCpuRtv, TRUE, nullptr);
	
	cmdList->SetGraphicsRoot32BitConstant(
		RootSignature::ConvoluteDiffuseIrradiance::EC_Consts,
		0, 
		RootSignature::ConvoluteDiffuseIrradiance::RootConstant::E_FaceID
	);
	
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(36, 1, 0, 0);

	mDiffuseIrradianceCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::GeneratePrefilteredEnvironment(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube) {	
	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvolutePrefilteredIrradiance].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ConvolutePrefilteredIrradiance].Get());

	cmdList->SetGraphicsRootConstantBufferView(
		RootSignature::ConvoluteSpecularIrradiance::ECB_ConvEquirectToConv, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(
		RootSignature::ConvoluteSpecularIrradiance::ESI_Environment, mhEnvironmentCubeMapGpuSrv);
	
	mPrefilteredEnvironmentCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	for (UINT mipLevel = 0; mipLevel < IrradianceMap::MaxMipLevel; ++mipLevel) {
		UINT size = static_cast<UINT>(CubeMapSize / std::pow(2.0, mipLevel));

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(size), static_cast<FLOAT>(size), 0.0f, 1.0f };
		D3D12_RECT rect = { 0, 0, static_cast<INT>(size), static_cast<INT>(size) };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &rect);

		cmdList->SetGraphicsRoot32BitConstant(
			RootSignature::ConvoluteSpecularIrradiance::EC_Consts,
			mipLevel,
			RootSignature::ConvoluteSpecularIrradiance::RootConstant::E_MipLevel
		);

		FLOAT values[1] = { static_cast<FLOAT>(0.16667f * mipLevel) };
		cmdList->SetGraphicsRoot32BitConstants(
			RootSignature::ConvoluteSpecularIrradiance::EC_Consts,
			_countof(values),
			values,
			RootSignature::ConvoluteSpecularIrradiance::RootConstant::E_Roughness
		);

		cmdList->OMSetRenderTargets(1, &mhPrefilteredEnvironmentCubeMapCpuRtvs[mipLevel], TRUE, nullptr);

		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(36, 1, 0, 0);
	}

	mPrefilteredEnvironmentCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void IrradianceMapClass::GenerateIntegratedBrdf(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbPass) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_IntegrateBRDF].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_IntegrateBRDF].Get());

	cmdList->RSSetViewports(1, &mCubeMapViewport);
	cmdList->RSSetScissorRects(1, &mCubeMapScissorRect);

	mIntegratedBrdfMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &mhIntegratedBrdfMapCpuRtv, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(
		RootSignature::IntegrateBRDF::ECB_Pass, cbPass);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	mIntegratedBrdfMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}