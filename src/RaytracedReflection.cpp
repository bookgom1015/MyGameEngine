#include "RaytracedReflection.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"
#include "GpuResource.h"
#include "ShaderTable.h"
#include "D3D12Util.h"
#include "DxMesh.h"
#include "RenderItem.h"

#include <DirectXMath.h>

using namespace RaytracedReflection;
using namespace DirectX;

namespace {
	const std::string ReflectionRayCS = "ReflectionRayCS";

	const wchar_t* RadianceRayGenName		= L"RadianceRayGen";
	const wchar_t* RadianceClosestHitName	= L"RadianceClosestHit";
	const wchar_t* RadianceMissName			= L"RadianceMiss";
	const wchar_t* RadianceHitGroupName		= L"RadianceHitGroup";

	const wchar_t* ShadowClosestHitName = L"ShadowClosestHit";
	const wchar_t* ShadowMissName		= L"ShadowMiss";
	const wchar_t* ShadowHitGroupName	= L"ShadowHitGroup";

	const wchar_t* MissNames[RaytracedReflection::Ray::Count] = { RadianceMissName, ShadowMissName };
	const wchar_t* ClosestHitNames[RaytracedReflection::Ray::Count] = { RadianceClosestHitName, ShadowClosestHitName };
	const wchar_t* HitGroupNames[RaytracedReflection::Ray::Count] = { RadianceHitGroupName, ShadowHitGroupName };

	const char* RayGenShaderTableName	= "RayGenShaderTable";
	const char* MissShaderTableName		= "MissShaderTable";
	const char* HitGroupShaderTableName = "HitGroupShaderTable";
}

RaytracedReflectionClass::RaytracedReflectionClass() {
	for (UINT i = 0; i < Resource::Reflection::Count; ++i) 
		mReflectionMaps[i] = std::make_unique<GpuResource>();
	for (UINT i = 0; i < 2; ++i) {
		for (UINT j = 0; j < Resource::TemporalCache::Count; ++j)
			mTemporalCaches[i][j] = std::make_unique<GpuResource>();
		mTemporalReflectionMaps[i] = std::make_unique<GpuResource>();
	}
}

BOOL RaytracedReflectionClass::Initialize(
		ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(cmdList, width, height));

	return TRUE;
}

BOOL RaytracedReflectionClass::CompileShaders(const std::wstring& filePath) {
	{
		DxcDefine defines[] = {
				{ L"COOK_TORRANCE", L"1" }
		};

		const auto path = filePath + L"ReflectionRay.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(shaderInfo, ReflectionRayCS));
	}

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildRootSignatures(const StaticSamplers& samplers) {
	// Global
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[12];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
		texTables[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
		texTables[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);
		texTables[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 10);
		texTables[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		texTables[11].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Global::Count];
		slotRootParameter[RootSignature::Global::EC_Rr].InitAsConstants(RootSignature::Global::RootConstantsLayout::Count, 0);
		slotRootParameter[RootSignature::Global::ECB_Pass].InitAsConstantBufferView(1);
		slotRootParameter[RootSignature::Global::ECB_Rr].InitAsConstantBufferView(2);
		slotRootParameter[RootSignature::Global::EAS_BVH].InitAsShaderResourceView(0);
		slotRootParameter[RootSignature::Global::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::Global::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::Global::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::Global::ESI_RMS].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::Global::ESI_Position].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::Global::ESI_DiffIrrad].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::Global::ESI_AOCoeiff].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[RootSignature::Global::ESI_Prefiltered].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[RootSignature::Global::ESI_BrdfLUT].InitAsDescriptorTable(1, &texTables[8]);
		slotRootParameter[RootSignature::Global::ESI_TexMaps].InitAsDescriptorTable(1, &texTables[9]);
		slotRootParameter[RootSignature::Global::EUO_Reflection].InitAsDescriptorTable(1, &texTables[10]);
		slotRootParameter[RootSignature::Global::EUO_RayHitDist].InitAsDescriptorTable(1, &texTables[11]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_Global]));
	}
	// Local
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Local::Count];
		slotRootParameter[RootSignature::Local::ECB_Obj].InitAsConstantBufferView(0, 1);
		slotRootParameter[RootSignature::Local::ECB_Mat].InitAsConstantBufferView(1, 1);
		slotRootParameter[RootSignature::Local::ESB_Vertices].InitAsShaderResourceView(0, 1);
		slotRootParameter[RootSignature::Local::ESB_Indices].InitAsShaderResourceView(1, 1);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_Local]));
	}

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildPSO() {
	CD3DX12_STATE_OBJECT_DESC reflectionDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto reflectionLib = reflectionDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto reflectionShader = mShaderManager->GetDxcShader(ReflectionRayCS);
	D3D12_SHADER_BYTECODE reflectionLibDxil = CD3DX12_SHADER_BYTECODE(reflectionShader->GetBufferPointer(), reflectionShader->GetBufferSize());
	reflectionLib->SetDXILLibrary(&reflectionLibDxil);
	LPCWSTR exports[] = { RadianceRayGenName, RadianceClosestHitName, RadianceMissName, ShadowClosestHitName, ShadowMissName };
	reflectionLib->DefineExports(exports);

	for (UINT i = 0; i < Ray::Count; ++i) {
		auto hitGroup = reflectionDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetClosestHitShaderImport(ClosestHitNames[i]);
		hitGroup->SetHitGroupExport(HitGroupNames[i]);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
	}

	auto shaderConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4) + 4 + 4; // Color(float4) + tHit(float) + IsHit(BOOL)
	UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = reflectionDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	auto localRootSig = reflectionDxrPso.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Local].Get());

	auto rootSigAssociation = reflectionDxrPso.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	rootSigAssociation->SetSubobjectToAssociate(*localRootSig);
	rootSigAssociation->AddExports(HitGroupNames);

	auto pipelineConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 2;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(reflectionDxrPso, IID_PPV_ARGS(&mDxrPso)));
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildShaderTables(
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat) {
#ifdef _DEBUG
	// A shader name look-up table for shader table debug print out.
	std::unordered_map<void*, std::wstring> shaderIdToStringMap;
#endif

	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	{
		void* radianceRayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(RadianceRayGenName);
		ShaderTable rayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
		CheckReturn(rayGenShaderTable.Initialze());
		rayGenShaderTable.push_back(ShaderRecord(radianceRayGenShaderIdentifier, shaderIdentifierSize));

#ifdef _DEBUG
		shaderIdToStringMap[radianceRayGenShaderIdentifier] = RadianceRayGenName;

		WLogln(L"Raytraced Reflection - Ray Gen");
		rayGenShaderTable.DebugPrint(shaderIdToStringMap);
		WLogln(L"");
#endif

		mShaderTables[RayGenShaderTableName] = rayGenShaderTable.GetResource();
	}
	{
		void* missShaderIds[Ray::Count];
		for (UINT i = 0; i < Ray::Count; ++i)
			missShaderIds[i] = mDxrPsoProp->GetShaderIdentifier(MissNames[i]);

		ShaderTable missShaderTable(md3dDevice, Ray::Count, shaderIdentifierSize);
		CheckReturn(missShaderTable.Initialze());
		
		for (auto shaderId : missShaderIds)
			missShaderTable.push_back(ShaderRecord(shaderId, shaderIdentifierSize));

#ifdef _DEBUG
		for (UINT i = 0; i < Ray::Count; ++i)
			shaderIdToStringMap[missShaderIds[i]] = MissNames[i];

		WLogln(L"Raytraced Reflection - Miss");
		missShaderTable.DebugPrint(shaderIdToStringMap);
		WLogln(L"");
#endif
		mMissShaderTableStrideInBytes = missShaderTable.GetShaderRecordSize();
		mShaderTables[MissShaderTableName] = missShaderTable.GetResource();
	}
	{
		void* hitGroupShaderIds[Ray::Count];
		for (UINT i = 0; i < Ray::Count; ++i) 
			hitGroupShaderIds[i] = mDxrPsoProp->GetShaderIdentifier(HitGroupNames[i]);

		UINT shaderRecordSize = shaderIdentifierSize + sizeof(RootSignature::Local::RootArguments);

		auto ritemsSize =  static_cast<UINT>(ritems.size());

		ShaderTable hitGroupTable(md3dDevice, ritemsSize * Ray::Count, shaderRecordSize);
		CheckReturn(hitGroupTable.Initialze());

		UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
		UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));
		
		mShadowRayOffset = ritemsSize;

		for (auto shaderId : hitGroupShaderIds) {
			for (const auto ritem : ritems) {
				RootSignature::Local::RootArguments rootArgs;
				rootArgs.CB_Object = cb_obj + ritem->ObjCBIndex * objCBByteSize;
				rootArgs.CB_Material = cb_mat + ritem->Material->MatCBIndex * matCBByteSize;
				rootArgs.SB_Vertices = ritem->Geometry->VertexBufferGPU->GetGPUVirtualAddress();
				rootArgs.AB_Indices = ritem->Geometry->IndexBufferGPU->GetGPUVirtualAddress();

				ShaderRecord hitGroupShaderRecord = ShaderRecord(shaderId, shaderIdentifierSize, &rootArgs, sizeof(rootArgs));

				hitGroupTable.push_back(hitGroupShaderRecord);
			}
		}

#ifdef _DEBUG
		for (UINT i = 0; i < Ray::Count; ++i)
			shaderIdToStringMap[hitGroupShaderIds[i]] = HitGroupNames[i];

		WLogln(L"Raytraced Reflection - Hit Group")
		hitGroupTable.DebugPrint(shaderIdToStringMap);
		WLogln(L"");
#endif
		mHitGroupShaderTableStrideInBytes = hitGroupTable.GetShaderRecordSize();
		mShaderTables[HitGroupShaderTableName] = hitGroupTable.GetResource();
	}

	return TRUE;
}

void RaytracedReflectionClass::BuildDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	for (UINT type = 0; type < Descriptor::Reflection::Count; ++type) {
		mhReflectionMapCpus[type] = hCpu.Offset(1, descSize);
		mhReflectionMapGpus[type] = hGpu.Offset(1, descSize);
	}
	for (UINT i = 0; i < 2; ++i) {
		for (UINT type = 0; type < Descriptor::TemporalCache::Count; ++type) {
			mhTemporalCachesCpus[i][type] = hCpu.Offset(1, descSize);
			mhTemporalCachesGpus[i][type] = hGpu.Offset(1, descSize);
		}
		for (UINT type = 0; type < Descriptor::TemporalReflection::Count; ++type) {
			mhTemporalReflectionMapCpus[i][type] = hCpu.Offset(1, descSize);
			mhTemporalReflectionMapGpus[i][type] = hGpu.Offset(1, descSize);
		}
	}

	BuildDescriptors();	
}

BOOL RaytracedReflectionClass::OnResize(ID3D12GraphicsCommandList*const cmdList, UINT width, UINT height) {
	CheckReturn(BuildResources(cmdList, width, height));
	BuildDescriptors();

	return TRUE;
}

void RaytracedReflectionClass::CalcReflection(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_rr,
		D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_diffIrrad,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
		D3D12_GPU_DESCRIPTOR_HANDLE si_prefiltered,
		D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
		D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
		UINT width, UINT height,
		FLOAT radius) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	static UINT count = 0;
	++count;
	cmdList->SetComputeRoot32BitConstants(RootSignature::Global::EC_Rr, 1, &count, RootSignature::Global::RootConstantsLayout::E_FrameCount);

	cmdList->SetComputeRoot32BitConstants(RootSignature::Global::EC_Rr, 1, &mShadowRayOffset, RootSignature::Global::RootConstantsLayout::E_ShadowRayOffset);
	cmdList->SetComputeRoot32BitConstants(RootSignature::Global::EC_Rr, 1, &radius, RootSignature::Global::RootConstantsLayout::E_ReflectionRadius);

	cmdList->SetComputeRootConstantBufferView(RootSignature::Global::ECB_Pass, cb_pass);
	cmdList->SetComputeRootConstantBufferView(RootSignature::Global::ECB_Rr, cb_rr);
	cmdList->SetComputeRootShaderResourceView(RootSignature::Global::EAS_BVH, as_bvh);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_BackBuffer, si_backBuffer);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_RMS, si_rms);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Position, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_DiffIrrad, si_diffIrrad);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_AOCoeiff, si_aocoeiff);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Prefiltered, si_prefiltered);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_BrdfLUT, si_brdf);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_TexMaps, si_texMaps);

	const auto reflection = mReflectionMaps[Resource::Reflection::E_Reflection].get();
	const auto rayHitDist = mReflectionMaps[Resource::Reflection::E_RayHitDistance].get();
	GpuResource* resources[] = { reflection, rayHitDist };

	reflection->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	rayHitDist->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarriers(cmdList, resources, _countof(resources));

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::EUO_Reflection, mhReflectionMapGpus[Descriptor::Reflection::EU_Reflection]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::EUO_RayHitDist, mhReflectionMapGpus[Descriptor::Reflection::EU_RayHitDistance]);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables[RayGenShaderTableName];
	const auto& miss = mShaderTables[MissShaderTableName];
	const auto& hitGroup = mShaderTables[HitGroupShaderTableName];
	dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGen->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGen->GetDesc().Width;
	dispatchDesc.MissShaderTable.StartAddress = miss->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = miss->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = mMissShaderTableStrideInBytes;
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = mHitGroupShaderTableStrideInBytes;
	dispatchDesc.Width = width;
	dispatchDesc.Height = height;
	dispatchDesc.Depth = 1;
	cmdList->DispatchRays(&dispatchDesc);

	reflection->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	rayHitDist->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarriers(cmdList, resources, _countof(resources));
}

UINT RaytracedReflectionClass::MoveToNextFrame() {
	mTemporalCurrentFrameResourceIndex = (mTemporalCurrentFrameResourceIndex + 1) % 2;
	return mTemporalCurrentFrameResourceIndex;
}

UINT RaytracedReflectionClass::MoveToNextFrameTemporalReflection() {
	mTemporalCurrentFrameTemporalReflectionResourceIndex = (mTemporalCurrentFrameTemporalReflectionResourceIndex + 1) % 2;
	return mTemporalCurrentFrameTemporalReflectionResourceIndex;
}

void RaytracedReflectionClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	// Reflection
	{
		{
			srvDesc.Format = ReflectionMapFormat;
			uavDesc.Format = ReflectionMapFormat;
			const auto resource = mReflectionMaps[Resource::Reflection::E_Reflection]->Resource();
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhReflectionMapCpus[Descriptor::Reflection::ES_Reflection]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhReflectionMapCpus[Descriptor::Reflection::EU_Reflection]);
		}
		{
			srvDesc.Format = RayHitDistanceFormat;
			uavDesc.Format = RayHitDistanceFormat;
			const auto resource = mReflectionMaps[Resource::Reflection::E_RayHitDistance]->Resource();
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhReflectionMapCpus[Descriptor::Reflection::ES_RayHitDistance]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhReflectionMapCpus[Descriptor::Reflection::EU_RayHitDistance]);
		}
	}
	// Temporal Cache
	{
		{
			srvDesc.Format = TsppMapFormat;
			uavDesc.Format = TsppMapFormat;
			for (size_t i = 0; i < 2; ++i) {
				auto resource = mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Resource();
				auto& cpus = mhTemporalCachesCpus[i];
				md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_Tspp]);
				md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_Tspp]);
			}
		}
		{
			srvDesc.Format = RayHitDistanceFormat;
			uavDesc.Format = RayHitDistanceFormat;
			for (size_t i = 0; i < 2; ++i) {
				auto resource = mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Resource();
				auto& cpus = mhTemporalCachesCpus[i];
				md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_RayHitDistance]);
				md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_RayHitDistance]);
			}
		}
		{
			srvDesc.Format = ReflectionSquaredMeanMapFormat;
			uavDesc.Format = ReflectionSquaredMeanMapFormat;
			for (size_t i = 0; i < 2; ++i) {
				auto resource = mTemporalCaches[i][Resource::TemporalCache::E_ReflectionSquaredMean]->Resource();
				auto& cpus = mhTemporalCachesCpus[i];
				md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_ReflectionSquaredMean]);
				md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_ReflectionSquaredMean]);
			}
		}
	}
	// Temporal Reflection
	{
		srvDesc.Format = ReflectionMapFormat;
		uavDesc.Format = ReflectionMapFormat;
		for (UINT i = 0; i < 2; ++i) {
			const auto resource = mTemporalReflectionMaps[i]->Resource();
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhTemporalReflectionMapCpus[i][Descriptor::TemporalReflection::E_Srv]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhTemporalReflectionMapCpus[i][Descriptor::TemporalReflection::E_Uav]);
		}
	}
}

BOOL RaytracedReflectionClass::BuildResources(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Reflection
	{
		{
			rscDesc.Format = ReflectionMapFormat;
			CheckReturn(mReflectionMaps[Resource::Reflection::E_Reflection]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				L"RR_RaytracedReflectionMap"
			));
		}
		{
			rscDesc.Format = RayHitDistanceFormat;
			CheckReturn(mReflectionMaps[Resource::Reflection::E_RayHitDistance]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				L"RR_RayHitDistanceMap"
			));
		}
		
	}
	// Temporal Cache
	{
		{
			rscDesc.Format = TsppMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_TsppMap_";
				name.append(std::to_wstring(i));
				CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Initialize(
					md3dDevice,
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				));
			}
		}
		{
			rscDesc.Format = RayHitDistanceFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_TemporalRayHitDistanceMap_";
				name.append(std::to_wstring(i));
				CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Initialize(
					md3dDevice,
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				));
			}
		}
		{
			rscDesc.Format = ReflectionSquaredMeanMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_ReflectionSquaredMeanMap_";
				name.append(std::to_wstring(i));
				CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_ReflectionSquaredMean]->Initialize(
					md3dDevice,
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				));
			}
		}
	}
	// Temporal Reflection
	{
		rscDesc.Format = ReflectionMapFormat;
		for (UINT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << "RR_TemporalRaytracedReflectionMap_" << i;
			CheckReturn(mTemporalReflectionMaps[i]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				wsstream.str().c_str()
			));
		}
	}

	return TRUE;
}
