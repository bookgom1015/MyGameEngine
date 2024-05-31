#include "DXR_Shadow.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "ShaderTable.h"
#include "MathHelper.h"
#include "HlslCompaction.h"

using namespace DXR_Shadow;

namespace {
	const CHAR* const CS_ShadowRay				= "CS_ShadowRay";

	const WCHAR* const ShadowRayGen				= L"ShadowRayGen"; 
	const WCHAR* const ShadowClosestHit			= L"ShadowClosestHit";
	const WCHAR* const ShadowMiss				= L"ShadowMiss";
	const WCHAR* const ShadowHitGroup			= L"ShadowHitGroup";

	const CHAR* const ShadowRayGenShaderTable	= "ShadowRayGenShaderTalbe";
	const CHAR* const ShadowMissShaderTable		= "ShadowMissShaderTalbe";
	const CHAR* const ShadowHitGroupShaderTable	= "ShadowHitGroupShaderTalbe";
}

DXR_ShadowClass::DXR_ShadowClass() {
	mResource = std::make_unique<GpuResource>();
}

BOOL DXR_ShadowClass::Initialize(
		ID3D12Device5* const device, 
		ID3D12GraphicsCommandList* const cmdList, 
		ShaderManager* const manager, 
		UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResource(cmdList, width, height));

	return TRUE;
}

BOOL DXR_ShadowClass::CompileShaders(const std::wstring& filePath) {
	const auto fullPath = filePath + L"ShadowRay.hlsl";
	auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"", L"lib_6_3");
	CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_ShadowRay));

	return TRUE;
}

BOOL DXR_ShadowClass::BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Global::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[4];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::Global::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::Global::ESI_AccelerationStructure].InitAsShaderResourceView(0);
		slotRootParameter[RootSignature::Global::ESI_Position].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::Global::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::Global::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::Global::EUO_Shadow].InitAsDescriptorTable(1, &texTables[3]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSignatureDesc, &mRootSignatures[RootSignature::E_Global]));
	}

	return TRUE;
}

BOOL DXR_ShadowClass::BuildPSO() {
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	CD3DX12_STATE_OBJECT_DESC dxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto shadowRayLib = dxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto shadowRayShader = mShaderManager->GetDxcShader(CS_ShadowRay);
	D3D12_SHADER_BYTECODE shadowRayLibDxil = CD3DX12_SHADER_BYTECODE(shadowRayShader->GetBufferPointer(), shadowRayShader->GetBufferSize());
	shadowRayLib->SetDXILLibrary(&shadowRayLibDxil);
	LPCWSTR shadowRayExports[] = { ShadowRayGen, ShadowClosestHit, ShadowMiss };
	shadowRayLib->DefineExports(shadowRayExports);

	auto shadowHitGroup = dxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	shadowHitGroup->SetClosestHitShaderImport(ShadowClosestHit);
	shadowHitGroup->SetHitGroupExport(ShadowHitGroup);
	shadowHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4; // IsHit(BOOL)
	UINT attribSize = sizeof(DirectX::XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = dxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	auto pipelineConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(dxrPso, IID_PPV_ARGS(&mPSO)));
	CheckHRESULT(mPSO->QueryInterface(IID_PPV_ARGS(&mPSOProp)));

	return TRUE;
}

BOOL DXR_ShadowClass::BuildShaderTables(UINT numRitems) {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	void* shadowRayGenShaderIdentifier = mPSOProp->GetShaderIdentifier(ShadowRayGen);
	void* shadowMissShaderIdentifier = mPSOProp->GetShaderIdentifier(ShadowMiss);
	void* shadowHitGroupShaderIdentifier = mPSOProp->GetShaderIdentifier(ShadowHitGroup);

	{
		ShaderTable rayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
		CheckReturn(rayGenShaderTable.Initialze());
		rayGenShaderTable.push_back(ShaderRecord(shadowRayGenShaderIdentifier, shaderIdentifierSize));
		mShaderTables[ShadowRayGenShaderTable] = rayGenShaderTable.GetResource();
	}
	{
		ShaderTable missShaderTable(md3dDevice, 1, shaderIdentifierSize);
		CheckReturn(missShaderTable.Initialze());
		missShaderTable.push_back(ShaderRecord(shadowMissShaderIdentifier, shaderIdentifierSize));
		mShaderTables[ShadowMissShaderTable] = missShaderTable.GetResource();
	}
	{
		ShaderTable hitGroupTable(md3dDevice, numRitems, shaderIdentifierSize);
		CheckReturn(hitGroupTable.Initialze());
		for (UINT i = 0; i < numRitems; ++i)
			hitGroupTable.push_back(ShaderRecord(shadowHitGroupShaderIdentifier, shaderIdentifierSize));
		mHitGroupShaderTableStrideInBytes = hitGroupTable.GetShaderRecordSize();
		mShaderTables[ShadowHitGroupShaderTable] = hitGroupTable.GetResource();
	}

	return TRUE;
}

void DXR_ShadowClass::Run(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		UINT width, UINT height) {
	cmdList->SetPipelineState1(mPSO.Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	cmdList->SetComputeRootShaderResourceView(RootSignature::Global::ESI_AccelerationStructure, as_bvh);
	cmdList->SetComputeRootConstantBufferView(RootSignature::Global::ECB_Pass, cb_pass);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Position, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Depth, si_depth);

	const auto shadow = mResource.get();

	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, shadow);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::EUO_Shadow, mhGpuDesc);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables[ShadowRayGenShaderTable];
	const auto& miss = mShaderTables[ShadowMissShaderTable];
	const auto& hitGroup = mShaderTables[ShadowHitGroupShaderTable];
	dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGen->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGen->GetDesc().Width;
	dispatchDesc.MissShaderTable.StartAddress = miss->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = miss->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = mHitGroupShaderTableStrideInBytes;
	dispatchDesc.Width = width;
	dispatchDesc.Height = height;
	dispatchDesc.Depth = 1;

	cmdList->SetPipelineState1(mPSO.Get());
	cmdList->DispatchRays(&dispatchDesc);
	
	shadow->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, shadow);
}


void DXR_ShadowClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhCpuDesc = hCpu.Offset(1, descSize);
	mhGpuDesc = hGpu.Offset(1, descSize);
	mhCpuDesc = hCpu.Offset(1, descSize);
	mhGpuDesc = hGpu.Offset(1, descSize);

	BuildDescriptors();	
}

BOOL DXR_ShadowClass::OnResize(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height) {
	CheckReturn(BuildResource(cmdList, width, height));
	BuildDescriptors();

	return TRUE;
}

void DXR_ShadowClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	srvDesc.Format = ShadowMapFormat;
	uavDesc.Format = ShadowMapFormat;

	auto resource = mResource->Resource();
	md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhCpuDesc);
	md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhCpuDesc);
}

BOOL DXR_ShadowClass::BuildResource(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height) {
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Format = ShadowMapFormat;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;

	for (INT i = 0; i < 2; ++i) {

		CheckReturn(mResource->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DXR_Shadow_ShadowMap"
		));
	}

	return TRUE;
}