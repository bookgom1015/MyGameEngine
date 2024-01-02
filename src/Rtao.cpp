#include "Rtao.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "ShaderTable.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

#undef max

using namespace DirectX;
using namespace Rtao;

namespace {
	std::string RtaoCS = "RtaoCS";
}

RtaoClass::RtaoClass() {
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

BOOL RtaoClass::Initialize(ID3D12Device5* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL RtaoClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"Rtao.hlsl";
	auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3");
	CheckReturn(mShaderManager->CompileShader(shaderInfo, RtaoCS));

	return TRUE;
}

BOOL RtaoClass::BuildRootSignatures(const StaticSamplers& samplers) {
	// Ray-traced ambient occlusion
	CD3DX12_DESCRIPTOR_RANGE texTables[5];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];
	slotRootParameter[RootSignature::ESI_AccelerationStructure].InitAsShaderResourceView(0);
	slotRootParameter[RootSignature::ECB_RtaoPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::ESI_Pos].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignature::EUO_AOCoefficient].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignature::EUO_RayHitDistance].InitAsDescriptorTable(1, &texTables[4]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(
		md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return TRUE;
}

BOOL RtaoClass::BuildPSO() {
	CD3DX12_STATE_OBJECT_DESC rtaoDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto rtaoLib = rtaoDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto rtaoShader = mShaderManager->GetDxcShader(RtaoCS);
	D3D12_SHADER_BYTECODE rtaoLibDxil = CD3DX12_SHADER_BYTECODE(rtaoShader->GetBufferPointer(), rtaoShader->GetBufferSize());
	rtaoLib->SetDXILLibrary(&rtaoLibDxil);
	LPCWSTR rtaoExports[] = { L"RtaoRayGen", L"RtaoClosestHit", L"RtaoMiss" };
	rtaoLib->DefineExports(rtaoExports);

	auto rtaoHitGroup = rtaoDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	rtaoHitGroup->SetClosestHitShaderImport(L"RtaoClosestHit");
	rtaoHitGroup->SetHitGroupExport(L"RtaoHitGroup");
	rtaoHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4; // tHit(FLOAT)
	UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = rtaoDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignature.Get());

	auto pipelineConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(rtaoDxrPso, IID_PPV_ARGS(&mDxrPso)));
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return TRUE;
}

BOOL RtaoClass::BuildShaderTables(UINT numRitems) {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	void* rtaoRayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoRayGen");
	void* rtaoMissShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoMiss");
	void* rtaoHitGroupShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoHitGroup");

	{
		ShaderTable rtaoRayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
		CheckReturn(rtaoRayGenShaderTable.Initialze());
		rtaoRayGenShaderTable.push_back(ShaderRecord(rtaoRayGenShaderIdentifier, shaderIdentifierSize));
		mShaderTables["rtaoRayGen"] = rtaoRayGenShaderTable.GetResource();
	}
	{
		ShaderTable rtaoMissShaderTable(md3dDevice, 1, shaderIdentifierSize);
		CheckReturn(rtaoMissShaderTable.Initialze());
		rtaoMissShaderTable.push_back(ShaderRecord(rtaoMissShaderIdentifier, shaderIdentifierSize));
		mShaderTables["rtaoMiss"] = rtaoMissShaderTable.GetResource();
	}
	{
		ShaderTable rtaoHitGroupTable(md3dDevice, numRitems, shaderIdentifierSize);
		CheckReturn(rtaoHitGroupTable.Initialze());		
		for (UINT i = 0; i < numRitems; ++i)
			rtaoHitGroupTable.push_back(ShaderRecord(rtaoHitGroupShaderIdentifier, shaderIdentifierSize));
		mShaderTables["rtaoHitGroup"] = rtaoHitGroupTable.GetResource();
	}

	return TRUE;
}

void RtaoClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
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

	BuildDescriptors();	
}

BOOL RtaoClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void RtaoClass::RunCalculatingAmbientOcclusion(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_aoCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_rayHitDistance,
		UINT width, UINT height) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	const auto& aoCoeff = mAOResources[Rtao::Resource::AO::E_AmbientCoefficient].get();
	aoCoeff->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, aoCoeff);

	cmdList->SetComputeRootShaderResourceView(RootSignature::ESI_AccelerationStructure, accelStruct);
	cmdList->SetComputeRootConstantBufferView(RootSignature::ECB_RtaoPass, cbAddress);

	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Pos, si_pos);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_AOCoefficient, uo_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_RayHitDistance, uo_rayHitDistance);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables["rtaoRayGen"];
	const auto& miss = mShaderTables["rtaoMiss"];
	const auto& hitGroup = mShaderTables["rtaoHitGroup"];
	dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGen->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGen->GetDesc().Width;
	dispatchDesc.MissShaderTable.StartAddress = miss->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = miss->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
	dispatchDesc.Width = width;
	dispatchDesc.Height = height;
	dispatchDesc.Depth = 1;
	cmdList->DispatchRays(&dispatchDesc);

	aoCoeff->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, aoCoeff);
}

UINT RtaoClass::MoveToNextFrame() {
	mTemporalCurrentFrameResourceIndex = (mTemporalCurrentFrameResourceIndex + 1) % 2;
	return mTemporalCurrentFrameResourceIndex;
}

UINT RtaoClass::MoveToNextFrameTemporalAOCoefficient() {
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = (mTemporalCurrentFrameTemporalAOCeofficientResourceIndex + 1) % 2;
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

void RtaoClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	{
		srvDesc.Format = AOCoefficientMapFormat;
		uavDesc.Format = AOCoefficientMapFormat;

		auto resource = mAOResources[Resource::AO::E_AmbientCoefficient]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_AmbientCoefficient]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_AmbientCoefficient]);
	}
	{
		srvDesc.Format = RayHitDistanceFormat;
		uavDesc.Format = RayHitDistanceFormat;

		auto resource = mAOResources[Resource::AO::E_RayHitDistance]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_RayHitDistance]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_RayHitDistance]);
	}

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
		srvDesc.Format = CoefficientSquaredMeanMapFormat;
		uavDesc.Format = CoefficientSquaredMeanMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_CoefficientSquaredMean]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_CoefficientSquaredMean]);
		}
	}
	{
		srvDesc.Format = AOCoefficientMapFormat;
		uavDesc.Format = AOCoefficientMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			md3dDevice->CreateShaderResourceView(
				mTemporalAOCoefficients[i]->Resource(), &srvDesc,
				mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Srv]
			);
			md3dDevice->CreateUnorderedAccessView(
				mTemporalAOCoefficients[i]->Resource(), nullptr, &uavDesc,
				mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Uav]
			);
		}
	}
}

BOOL RtaoClass::BuildResources(UINT width, UINT height) {
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

	{
		texDesc.Format = AOCoefficientMapFormat;
		CheckReturn(mAOResources[Resource::AO::E_AmbientCoefficient]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"AOCoefficientMap"
		));
	}
	{
		texDesc.Format = RayHitDistanceFormat;
		CheckReturn(mAOResources[Resource::AO::E_RayHitDistance]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RayHitDistanceMap"
		));
	}
	{
		texDesc.Format = TsppMapFormat;
		for (INT i = 0; i < 2; ++i) {
			std::wstring name = L"RTAO_TsppMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));
		}
	}
	{
		texDesc.Format = RayHitDistanceFormat;
		for (INT i = 0; i < 2; ++i) {
			std::wstring name = L"TemporalRayHitDistanceMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));
		}
	}
	{
		texDesc.Format = CoefficientSquaredMeanMapFormat;
		for (INT i = 0; i < 2; ++i) {
			std::wstring name = L"AOCoefficientSquaredMeanMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));			
		}
	}
	{
		texDesc.Format = AOCoefficientMapFormat;
		for (INT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << L"TemporalAOCoefficientMap_" << i;
			CheckReturn(mTemporalAOCoefficients[i]->Initialize(
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

	//{
	//	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	//	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mPrevFrameNormalDepth->Resource(), 0, num2DSubresources);
	//
	//	CheckReturn(mPrevFrameNormalDepthUploadBuffer->Initialize(
	//		md3dDevice,
	//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//		D3D12_HEAP_FLAG_NONE,
	//		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
	//		D3D12_RESOURCE_STATE_COPY_SOURCE,
	//		nullptr
	//	));
	//
	//	const UINT size = mWidth * mHeight * 4;
	//	std::vector<BYTE> data(size);
	//
	//	for (UINT i = 0; i < size; i += 4) {
	//		data[i] = data[i + 1] = data[i + 2] = 0;	// rgb-channels(normal) = 0 / 128;
	//		data[i + 3] = 127;							// a-channel(depth) = 127 / 128;
	//	}
	//
	//	D3D12_SUBRESOURCE_DATA subResourceData = {};
	//	subResourceData.pData = data.data();
	//	subResourceData.RowPitch = mWidth * 4;
	//	subResourceData.SlicePitch = subResourceData.RowPitch * mHeight;
	//	
	//	UpdateSubresources(
	//		cmdList,
	//		mPrevFrameNormalDepth->Resource(),
	//		mPrevFrameNormalDepthUploadBuffer->Resource(),
	//		0,
	//		0,
	//		num2DSubresources,
	//		&subResourceData
	//	);
	//
	//	mPrevFrameNormalDepth->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//}

	return TRUE;
}