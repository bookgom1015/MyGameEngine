#include "DirectX/Shading/RaytracedReflection.h"
#include "Common/Debug/Logger.h"
#include "Common/Render/RenderItem.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Infrastructure/ShaderTable.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"
#include "DxMesh.h"

#include <DirectXMath.h>

using namespace RaytracedReflection;
using namespace DirectX;

namespace {
	const CHAR* const CS_ReflectionRay			= "CS_RaytracedReflection";

	const WCHAR* const RadianceRayGenName		= L"RadianceRayGen";
	const WCHAR* const RadianceClosestHitName	= L"RadianceClosestHit";
	const WCHAR* const RadianceMissName			= L"RadianceMiss";
	const WCHAR* const RadianceHitGroupName		= L"RadianceHitGroup";

	const WCHAR* const ShadowClosestHitName		= L"ShadowClosestHit";
	const WCHAR* const ShadowMissName			= L"ShadowMiss";
	const WCHAR* const ShadowHitGroupName		= L"ShadowHitGroup";

	const WCHAR* const MissNames[RaytracedReflection::Ray::Count]		= { RadianceMissName, ShadowMissName };
	const WCHAR* const ClosestHitNames[RaytracedReflection::Ray::Count]	= { RadianceClosestHitName, ShadowClosestHitName };
	const WCHAR* HitGroupNames[RaytracedReflection::Ray::Count]			= { RadianceHitGroupName, ShadowHitGroupName };

	const CHAR* const RayGenShaderTableName		= "RayGenShaderTable";
	const CHAR* const MissShaderTableName		= "MissShaderTable";
	const CHAR* const HitGroupShaderTableName	= "HitGroupShaderTable";
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

BOOL RaytracedReflectionClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL RaytracedReflectionClass::CompileShaders(const std::wstring& filePath) {
	{
		DxcDefine defines[] = {
			{ L"COOK_TORRANCE", L"1" }
		};

		const auto path = filePath + L"RaytracedReflection.hlsl";
		const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_ReflectionRay));
	}

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildRootSignatures(const StaticSamplers& samplers) {
	D3D12Util::Descriptor::RootSignature::Builder builder;
	// Global
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[11] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_TEXTURE_MAPS, 9);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Global::Count] = {};
		slotRootParameter[RootSignature::Global::EC_Rr].InitAsConstants(RootSignature::Global::RootConstantsLayout::Count, 0);
		slotRootParameter[RootSignature::Global::ECB_Pass].InitAsConstantBufferView(1);
		slotRootParameter[RootSignature::Global::EAS_BVH].InitAsShaderResourceView(0);
		slotRootParameter[RootSignature::Global::ESI_BackBuffer].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_Normal].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_Depth].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_RMS].InitAsDescriptorTable(		1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_Position].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_DiffIrrad].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_Prefiltered].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_BrdfLUT].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::ESI_TexMaps].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::EUO_Reflection].InitAsDescriptorTable(	1, &texTables[index++]);
		slotRootParameter[RootSignature::Global::EUO_RayHitDist].InitAsDescriptorTable(	1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);

		builder.Enqueue(globalRootSignatureDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_Global]));
	}
	// Local
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Local::Count] = {};
		slotRootParameter[RootSignature::Local::ECB_Obj].InitAsConstantBufferView(0, 1);
		slotRootParameter[RootSignature::Local::ECB_Mat].InitAsConstantBufferView(1, 1);
		slotRootParameter[RootSignature::Local::ESB_Vertices].InitAsShaderResourceView(0, 1);
		slotRootParameter[RootSignature::Local::ESB_Indices].InitAsShaderResourceView(1, 1);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
		);

		builder.Enqueue(globalRootSignatureDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_Local]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildPSO() {
	CD3DX12_STATE_OBJECT_DESC reflectionDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	const auto reflectionLib = reflectionDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	const auto reflectionShader = mShaderManager->GetDxcShader(CS_ReflectionRay);
	const D3D12_SHADER_BYTECODE reflectionLibDxil = CD3DX12_SHADER_BYTECODE(reflectionShader->GetBufferPointer(), reflectionShader->GetBufferSize());
	reflectionLib->SetDXILLibrary(&reflectionLibDxil);
	LPCWSTR exports[] = { RadianceRayGenName, RadianceClosestHitName, RadianceMissName, ShadowClosestHitName, ShadowMissName };
	reflectionLib->DefineExports(exports);

	for (UINT i = 0; i < Ray::Count; ++i) {
		const auto hitGroup = reflectionDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetClosestHitShaderImport(ClosestHitNames[i]);
		hitGroup->SetHitGroupExport(HitGroupNames[i]);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
	}

	const auto shaderConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	const UINT payloadSize = sizeof(XMFLOAT4) + 4 + 4; // Color(float4) + tHit(float) + IsHit(BOOL)
	const UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	const auto glbalRootSig = reflectionDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	const auto localRootSig = reflectionDxrPso.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Local].Get());

	const auto rootSigAssociation = reflectionDxrPso.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	rootSigAssociation->SetSubobjectToAssociate(*localRootSig);
	rootSigAssociation->AddExports(HitGroupNames);

	const auto pipelineConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	const UINT maxRecursionDepth = 2;
	pipelineConfig->Config(maxRecursionDepth);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckHRESULT(device->CreateStateObject(reflectionDxrPso, IID_PPV_ARGS(&mDxrPso)));
	}
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return TRUE;
}

BOOL RaytracedReflectionClass::BuildShaderTables(
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
		UINT objCBByteSize,
		UINT matCBByteSize) {
#ifdef _DEBUG
	// A shader name look-up table for shader table debug print out.
	std::unordered_map<void*, std::wstring> shaderIdToStringMap;
#endif

	const UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);
		{
			void* const radianceRayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(RadianceRayGenName);
			ShaderTable rayGenShaderTable(device, 1, shaderIdentifierSize);
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

			ShaderTable missShaderTable(device, Ray::Count, shaderIdentifierSize);
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

			const auto ritemsSize = static_cast<UINT>(ritems.size());

			ShaderTable hitGroupTable(device, ritemsSize * Ray::Count, shaderRecordSize);
			CheckReturn(hitGroupTable.Initialze());

			mShadowRayOffset = ritemsSize;

			for (auto shaderId : hitGroupShaderIds) {
				for (const auto ritem : ritems) {
					RootSignature::Local::RootArguments rootArgs;
					rootArgs.CB_Object = cb_obj + static_cast<UINT64>(ritem->ObjCBIndex) * static_cast<UINT64>(objCBByteSize);
					rootArgs.CB_Material = cb_mat + static_cast<UINT64>(ritem->Material->MatCBIndex) * static_cast<UINT64>(matCBByteSize);
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
	}

	return TRUE;
}

void RaytracedReflectionClass::AllocateDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
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
}

BOOL RaytracedReflectionClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12Util::Descriptor::View::Builder builder;
	// Reflection
	{
		{
			srvDesc.Format = ReflectionMapFormat;
			uavDesc.Format = ReflectionMapFormat;
			const auto resource = mReflectionMaps[Resource::Reflection::E_Reflection]->Resource();
			builder.Enqueue(resource, srvDesc, mhReflectionMapCpus[Descriptor::Reflection::ES_Reflection]);
			builder.Enqueue(resource, uavDesc, mhReflectionMapCpus[Descriptor::Reflection::EU_Reflection]);
		}
		{
			srvDesc.Format = RayHitDistanceFormat;
			uavDesc.Format = RayHitDistanceFormat;
			const auto resource = mReflectionMaps[Resource::Reflection::E_RayHitDistance]->Resource();
			builder.Enqueue(resource, srvDesc, mhReflectionMapCpus[Descriptor::Reflection::ES_RayHitDistance]);
			builder.Enqueue(resource, uavDesc, mhReflectionMapCpus[Descriptor::Reflection::EU_RayHitDistance]);
		}
	}
	// Temporal Cache
	{
		{
			srvDesc.Format = TsppMapFormat;
			uavDesc.Format = TsppMapFormat;
			for (size_t i = 0; i < 2; ++i) {
				const auto resource = mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Resource();
				const auto& cpus = mhTemporalCachesCpus[i];
				builder.Enqueue(resource, srvDesc, cpus[Descriptor::TemporalCache::ES_Tspp]);
				builder.Enqueue(resource, uavDesc, cpus[Descriptor::TemporalCache::EU_Tspp]);
			}
		}
		{
			srvDesc.Format = RayHitDistanceFormat;
			uavDesc.Format = RayHitDistanceFormat;
			for (size_t i = 0; i < 2; ++i) {
				const auto resource = mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Resource();
				const auto& cpus = mhTemporalCachesCpus[i];
				builder.Enqueue(resource, srvDesc, cpus[Descriptor::TemporalCache::ES_RayHitDistance]);
				builder.Enqueue(resource, uavDesc, cpus[Descriptor::TemporalCache::EU_RayHitDistance]);
			}
		}
		{
			srvDesc.Format = ReflectionSquaredMeanMapFormat;
			uavDesc.Format = ReflectionSquaredMeanMapFormat;
			for (size_t i = 0; i < 2; ++i) {
				const auto resource = mTemporalCaches[i][Resource::TemporalCache::E_ReflectionSquaredMean]->Resource();
				const auto& cpus = mhTemporalCachesCpus[i];
				builder.Enqueue(resource, srvDesc, cpus[Descriptor::TemporalCache::ES_ReflectionSquaredMean]);
				builder.Enqueue(resource, uavDesc, cpus[Descriptor::TemporalCache::EU_ReflectionSquaredMean]);
			}
		}
	}
	// Temporal Reflection
	{
		srvDesc.Format = ReflectionMapFormat;
		uavDesc.Format = ReflectionMapFormat;
		for (UINT i = 0; i < 2; ++i) {
			const auto resource = mTemporalReflectionMaps[i]->Resource();
			builder.Enqueue(resource, srvDesc, mhTemporalReflectionMapCpus[i][Descriptor::TemporalReflection::E_Srv]);
			builder.Enqueue(resource, uavDesc, mhTemporalReflectionMapCpus[i][Descriptor::TemporalReflection::E_Uav]);
		}
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL RaytracedReflectionClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void RaytracedReflectionClass::CalcReflection(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_diffIrrad,
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
	cmdList->SetComputeRootShaderResourceView(RootSignature::Global::EAS_BVH, as_bvh);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_BackBuffer, si_backBuffer);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_RMS, si_rms);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Position, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_DiffIrrad, si_diffIrrad);
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

BOOL RaytracedReflectionClass::BuildResources(UINT width, UINT height) {
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

	D3D12Util::Descriptor::Resource::Builder builder;
	// Reflection
	{
		{
			rscDesc.Format = ReflectionMapFormat;
			builder.Enqueue(
				mReflectionMaps[Resource::Reflection::E_Reflection].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				L"RR_RaytracedReflectionMap"
			);
		}
		{
			rscDesc.Format = RayHitDistanceFormat;
			builder.Enqueue(
				mReflectionMaps[Resource::Reflection::E_RayHitDistance].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				L"RR_RayHitDistanceMap"
			);
		}
		
	}
	// Temporal Cache
	{
		{
			rscDesc.Format = TsppMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_TsppMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_Tspp].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
		{
			rscDesc.Format = RayHitDistanceFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_TemporalRayHitDistanceMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
		{
			rscDesc.Format = ReflectionSquaredMeanMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RR_ReflectionSquaredMeanMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_ReflectionSquaredMean].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					rscDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
	}
	// Temporal Reflection
	{
		rscDesc.Format = ReflectionMapFormat;
		for (UINT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << "RR_TemporalRaytracedReflectionMap_" << i;
			builder.Enqueue(
				mTemporalReflectionMaps[i].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				wsstream.str().c_str()
			);
		}
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}
