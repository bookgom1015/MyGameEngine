#include "VolumetricLight.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"
#include "Light.h"

using namespace VolumetricLight;

namespace {
	const CHAR* const CS_CalcScatteringAndDensity = "CS_CalcScatteringAndDensity";
}

VolumetricLightClass::VolumetricLightClass() {
	mFrustumMap = std::make_unique<GpuResource>();
}

BOOL VolumetricLightClass::Initialize(
		ID3D12Device* device, ShaderManager* const manager,
		UINT width, UINT height, UINT depth) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;
	mDepth = depth;

	CheckReturn(BuildResources());

	return TRUE;
}

BOOL VolumetricLightClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"CalculateScatteringAndDensityCS.hlsl";
		auto csInfo = D3D12ShaderInfo(actualPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_CalcScatteringAndDensity));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildRootSignature(const StaticSamplers& samplers) {
	// CalcScatteringAndDensity
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcScatteringAndDensity::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[4]; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 2);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		index = 0;

		slotRootParameter[RootSignature::CalcScatteringAndDensity::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EC_Consts].InitAsConstants(RootSignature::CalcScatteringAndDensity::RootConstant::Count, 1);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepth].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_CalcScatteringAndDensity]));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	{
		auto cs = mShaderManager->GetDxcShader(CS_CalcScatteringAndDensity);
		psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_CalcScatteringAndDensity])));

	return TRUE;
}

void VolumetricLightClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
		FLOAT nearZ, FLOAT farZ) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_CalcScatteringAndDensity].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get());
	
	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcScatteringAndDensity::ECB_Pass, cb_pass);

	FLOAT values[RootSignature::CalcScatteringAndDensity::RootConstant::Count] = { nearZ, farZ, 4 };
	cmdList->SetComputeRoot32BitConstants(RootSignature::CalcScatteringAndDensity::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepth, si_zdepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube, si_zdepthCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube, si_faceIDCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume, mhFrustumMapGpuUav);
	
	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mWidth), Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mHeight), Default::ThreadGroup::Height), 
		D3D12Util::CeilDivide(static_cast<UINT>(mDepth), Default::ThreadGroup::Depth)
	);

	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());
}

void VolumetricLightClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhFrustumMapCpuSrv = hCpu.Offset(1, descSize);
	mhFrustumMapGpuSrv = hGpu.Offset(1, descSize);

	mhFrustumMapCpuUav = hCpu.Offset(1, descSize);
	mhFrustumMapGpuUav = hGpu.Offset(1, descSize);

	BuildDescriptors();
}

void VolumetricLightClass::BuildDescriptors() {
	const auto& frustum = mFrustumMap->Resource();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Format = FrustumMapFormat;
	srvDesc.Texture3D.MostDetailedMip = 0;
	srvDesc.Texture3D.MipLevels = 1;
	srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(frustum, &srvDesc, mhFrustumMapCpuSrv);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Format = FrustumMapFormat;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.WSize = mDepth;
	uavDesc.Texture3D.FirstWSlice = 0;

	md3dDevice->CreateUnorderedAccessView(frustum, nullptr, &uavDesc, mhFrustumMapCpuUav);
}

BOOL VolumetricLightClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// Scattering and density map
	{
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Width = mWidth;
		texDesc.Height = mHeight;
		texDesc.DepthOrArraySize = mDepth;
		texDesc.Format = FrustumMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CheckReturn(mFrustumMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"VolumetricLight_FrustumMap"
		));
	}

	return TRUE;
}