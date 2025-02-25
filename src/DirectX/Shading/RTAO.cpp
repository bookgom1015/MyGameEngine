#include "DirectX/Shading/RTAO.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Infrastructure/ShaderTable.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

#undef max

using namespace DirectX;
using namespace RTAO;

namespace {
	const CHAR* const CS_RTAO = "CS_RTAO";

	const WCHAR* const RTAO_RayGenName			= L"RTAO_RayGen";
	const WCHAR* const RTAO_ClosestHitName		= L"RTAO_ClosestHit";
	const WCHAR* const RTAO_MissName			= L"RTAO_Miss";
	const WCHAR* const RTAO_HitGroupName		= L"RTAO_HitGroup";

	const CHAR* const RayGenShaderTableName		= "RayGenShaderTable";
	const CHAR* const MissShaderTableName		= "MissShaderTable";
	const CHAR* const HitGroupShaderTableName	= "HitGroupShaderTable";
}

RTAOClass::RTAOClass() {
	for (INT i = 0; i < Resource::AO::Count; ++i) {
		mAOResources[i] = std::make_unique<GpuResource>();
	}
	for (INT i = 0; i < 2; ++i) {
		for (INT j = 0; j < Resource::TemporalCache::Count; ++j) {
			mTemporalCaches[i][j] = std::make_unique<GpuResource>();
		}

		mTemporalAOCoefficients[i] = std::make_unique<GpuResource>();
	}
};

BOOL RTAOClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL RTAOClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"RTAO.hlsl";
	const auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3");
	CheckReturn(mShaderManager->CompileShader(shaderInfo, CS_RTAO));

	return TRUE;
}

BOOL RTAOClass::BuildRootSignatures(const StaticSamplers& samplers) {
	// Ray-traced ambient occlusion
	CD3DX12_DESCRIPTOR_RANGE texTables[5] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count] = {};
	slotRootParameter[RootSignature::ESI_AccelerationStructure].InitAsShaderResourceView(0);
	slotRootParameter[RootSignature::ECB_RtaoPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::ESI_Pos].InitAsDescriptorTable(			1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(			1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(			1, &texTables[index++]);
	slotRootParameter[RootSignature::EUO_AOCoefficient].InitAsDescriptorTable(	1, &texTables[index++]);
	slotRootParameter[RootSignature::EUO_RayHitDistance].InitAsDescriptorTable(	1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateRootSignature(device, globalRootSignatureDesc, &mRootSignature));
	}

	return TRUE;
}

BOOL RTAOClass::BuildPSO() {
	CD3DX12_STATE_OBJECT_DESC rtaoDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	const auto rtaoLib = rtaoDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	const auto rtaoShader = mShaderManager->GetDxcShader(CS_RTAO);
	const D3D12_SHADER_BYTECODE rtaoLibDxil = CD3DX12_SHADER_BYTECODE(rtaoShader->GetBufferPointer(), rtaoShader->GetBufferSize());
	rtaoLib->SetDXILLibrary(&rtaoLibDxil);
	LPCWSTR rtaoExports[] = { RTAO_RayGenName, RTAO_ClosestHitName, RTAO_MissName };
	rtaoLib->DefineExports(rtaoExports);

	const auto rtaoHitGroup = rtaoDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	rtaoHitGroup->SetClosestHitShaderImport(RTAO_ClosestHitName);
	rtaoHitGroup->SetHitGroupExport(RTAO_HitGroupName);
	rtaoHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	const auto shaderConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	const UINT payloadSize = 4; // tHit(FLOAT)
	const UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	const auto glbalRootSig = rtaoDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignature.Get());

	const auto pipelineConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	const UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckHRESULT(device->CreateStateObject(rtaoDxrPso, IID_PPV_ARGS(&mDxrPso)));
	}
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return TRUE;
}

BOOL RTAOClass::BuildShaderTables(UINT numRitems) {
#ifdef _DEBUG
	// A shader name look-up table for shader table debug print out.
	std::unordered_map<void*, std::wstring> shaderIdToStringMap;
#endif

	const UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		{
			void* const rayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(RTAO_RayGenName);

			ShaderTable rayGenShaderTable(device, 1, shaderIdentifierSize);
			CheckReturn(rayGenShaderTable.Initialze());
			rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));

#ifdef _DEBUG
			shaderIdToStringMap[rayGenShaderIdentifier] = RTAO_RayGenName;

			WLogln(L"RTAO - Ray Gen");
			rayGenShaderTable.DebugPrint(shaderIdToStringMap);
			WLogln(L"");
#endif

			mShaderTables[RayGenShaderTableName] = rayGenShaderTable.GetResource();
		}
		{
			void* const missShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(RTAO_MissName);

			ShaderTable missShaderTable(device, 1, shaderIdentifierSize);
			CheckReturn(missShaderTable.Initialze());
			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));

#ifdef _DEBUG
			shaderIdToStringMap[missShaderIdentifier] = RTAO_MissName;

			WLogln(L"RTAO - Miss");
			missShaderTable.DebugPrint(shaderIdToStringMap);
			WLogln(L"");
#endif

			mShaderTables[MissShaderTableName] = missShaderTable.GetResource();
		}
		{
			void* const hitGroupShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(RTAO_HitGroupName);

			ShaderTable hitGroupTable(device, numRitems, shaderIdentifierSize);
			CheckReturn(hitGroupTable.Initialze());
			for (UINT i = 0; i < numRitems; ++i)
				hitGroupTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));

#ifdef _DEBUG
			shaderIdToStringMap[hitGroupShaderIdentifier] = RTAO_HitGroupName;

			WLogln(L"RTAO - Hit Group");
			hitGroupTable.DebugPrint(shaderIdToStringMap);
			WLogln(L"");
#endif

			mShaderTables[HitGroupShaderTableName] = hitGroupTable.GetResource();
			mHitGroupShaderTableStrideInBytes = hitGroupTable.GetShaderRecordSize();
		}
	}

	return TRUE;
}

void RTAOClass::AllocateDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	for (UINT i = 0; i < Descriptor::AO::Count; ++i) {
		mhAOResourcesCpus[i] = hCpu.Offset(1, descSize);
		mhAOResourcesGpus[i] = hGpu.Offset(1, descSize);
	}
	for (UINT i = 0; i < 2; ++i) {
		for (UINT j = 0; j < Descriptor::TemporalCache::Count; ++j) {
			mhTemporalCachesCpus[i][j] = hCpu.Offset(1, descSize);
			mhTemporalCachesGpus[i][j] = hGpu.Offset(1, descSize);
		}
	}
	for (UINT i = 0; i < 2; ++i) {
		for (UINT j = 0; j < Descriptor::TemporalAOCoefficient::Count; ++j) {
			mhTemporalAOCoefficientsCpus[i][j] = hCpu.Offset(1, descSize);
			mhTemporalAOCoefficientsGpus[i][j] = hGpu.Offset(1, descSize);
		}
	}
}

BOOL RTAOClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12Util::Descriptor::View::Builder builder;

	// AO
	{
		{
			srvDesc.Format = AOCoefficientMapFormat;
			uavDesc.Format = AOCoefficientMapFormat;

			const auto resource = mAOResources[Resource::AO::E_AmbientCoefficient]->Resource();
			builder.Enqueue(resource, srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_AmbientCoefficient]);
			builder.Enqueue(resource, uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_AmbientCoefficient]);
		}
		{
			srvDesc.Format = RayHitDistanceFormat;
			uavDesc.Format = RayHitDistanceFormat;

			const auto resource = mAOResources[Resource::AO::E_RayHitDistance]->Resource();
			builder.Enqueue(resource, srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_RayHitDistance]);
			builder.Enqueue(resource, uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_RayHitDistance]);
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
			srvDesc.Format = CoefficientSquaredMeanMapFormat;
			uavDesc.Format = CoefficientSquaredMeanMapFormat;
			for (size_t i = 0; i < 2; ++i) {
				const auto resource = mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean]->Resource();
				const auto& cpus = mhTemporalCachesCpus[i];
				builder.Enqueue(resource, srvDesc, cpus[Descriptor::TemporalCache::ES_CoefficientSquaredMean]);
				builder.Enqueue(resource, uavDesc, cpus[Descriptor::TemporalCache::EU_CoefficientSquaredMean]);
			}
		}
	}
	// Temporal AO
	{
		srvDesc.Format = AOCoefficientMapFormat;
		uavDesc.Format = AOCoefficientMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			builder.Enqueue(mTemporalAOCoefficients[i]->Resource(), srvDesc, mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Srv]);
			builder.Enqueue(mTemporalAOCoefficients[i]->Resource(), uavDesc, mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Uav]);
		}
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL RTAOClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void RTAOClass::RunCalculatingAmbientOcclusion(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		UINT width, UINT height) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	const auto& aoCoeff = mAOResources[RTAO::Resource::AO::E_AmbientCoefficient].get();
	aoCoeff->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, aoCoeff);

	cmdList->SetComputeRootShaderResourceView(RootSignature::ESI_AccelerationStructure, accelStruct);
	cmdList->SetComputeRootConstantBufferView(RootSignature::ECB_RtaoPass, cbAddress);

	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Pos, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_AOCoefficient, mhAOResourcesGpus[RTAO::Descriptor::AO::EU_AmbientCoefficient]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_RayHitDistance, mhAOResourcesGpus[RTAO::Descriptor::AO::EU_RayHitDistance]);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables[RayGenShaderTableName];
	const auto& miss = mShaderTables[MissShaderTableName];
	const auto& hitGroup = mShaderTables[HitGroupShaderTableName];
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
	cmdList->DispatchRays(&dispatchDesc);

	aoCoeff->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, aoCoeff);
}

UINT RTAOClass::MoveToNextFrame() {
	mTemporalCurrentFrameResourceIndex = (mTemporalCurrentFrameResourceIndex + 1) % 2;
	return mTemporalCurrentFrameResourceIndex;
}

UINT RTAOClass::MoveToNextFrameTemporalAOCoefficient() {
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = (mTemporalCurrentFrameTemporalAOCeofficientResourceIndex + 1) % 2;
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

BOOL RTAOClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12Util::Descriptor::Resource::Builder builder;
	// AO
	{
		{
			texDesc.Format = AOCoefficientMapFormat;
			builder.Enqueue(
				mAOResources[Resource::AO::E_AmbientCoefficient].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				L"RTAO_AOCoefficientMap"
			);
		}
		{
			texDesc.Format = RayHitDistanceFormat;
			builder.Enqueue(
				mAOResources[Resource::AO::E_RayHitDistance].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				L"RTAO_RayHitDistanceMap"
			);
		}
	}
	// Temporal Cache
	{
		{
			texDesc.Format = TsppMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RTAO_TsppMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_Tspp].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					texDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
		{
			texDesc.Format = RayHitDistanceFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RTAO_TemporalRayHitDistanceMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					texDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
		{
			texDesc.Format = CoefficientSquaredMeanMapFormat;
			for (INT i = 0; i < 2; ++i) {
				std::wstring name = L"RTAO_AOCoefficientSquaredMeanMap_";
				name.append(std::to_wstring(i));
				builder.Enqueue(
					mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean].get(),
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					texDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					nullptr,
					name.c_str()
				);
			}
		}
	}
	// Temporal AO
	{
		texDesc.Format = AOCoefficientMapFormat;
		for (INT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << L"RTAO_TemporalAOCoefficientMap_" << i;
			builder.Enqueue(
				mTemporalAOCoefficients[i].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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