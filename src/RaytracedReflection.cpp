#include "RaytracedReflection.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"
#include "GpuResource.h"
#include "ShaderTable.h"
#include "D3D12Util.h"

#include <DirectXMath.h>

using namespace RaytracedReflection;
using namespace DirectX;

namespace {
	const std::string ReflectionRayCS = "ReflectionRayCS";
}

RaytracedReflectionClass::RaytracedReflectionClass() {
	mReflectionMap = std::make_unique<GpuResource>();
	mReflectionUploadBuffer = std::make_unique<GpuResource>();
}

bool RaytracedReflectionClass::Initialize(
		ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	CheckReturn(BuildResources(cmdList));

	return true;
}

bool RaytracedReflectionClass::CompileShaders(const std::wstring& filePath) {
	{
		const auto path = filePath + L"ReflectionRay.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, ReflectionRayCS));
	}

	return true;
}

bool RaytracedReflectionClass::BuildRootSignatures(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[3];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];
	slotRootParameter[RootSignature::ECB_Rr].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::EAS_BVH].InitAsShaderResourceView(0);
	slotRootParameter[RootSignature::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignature::ESI_NormalDepth].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignature::EUO_Reflection].InitAsDescriptorTable(1, &texTables[2]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(
		md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return true;
}

bool RaytracedReflectionClass::BuildPSO() {
	CD3DX12_STATE_OBJECT_DESC reflectionDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto reflectionLib = reflectionDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto reflectionShader = mShaderManager->GetDxcShader(ReflectionRayCS);
	D3D12_SHADER_BYTECODE reflectionLibDxil = CD3DX12_SHADER_BYTECODE(reflectionShader->GetBufferPointer(), reflectionShader->GetBufferSize());
	reflectionLib->SetDXILLibrary(&reflectionLibDxil);
	LPCWSTR reflectionExports[] = { L"ReflectionRayGen", L"ReflectionClosestHit", L"ReflectionMiss" };
	reflectionLib->DefineExports(reflectionExports);

	auto reflectionHitGroup = reflectionDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	reflectionHitGroup->SetClosestHitShaderImport(L"ReflectionClosestHit");
	reflectionHitGroup->SetHitGroupExport(L"ReflectionHitGroup");
	reflectionHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4) /* tHit */;
	UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = reflectionDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignature.Get());

	auto pipelineConfig = reflectionDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(reflectionDxrPso, IID_PPV_ARGS(&mDxrPso)));
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return true;
}

bool RaytracedReflectionClass::BuildShaderTables() {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	void* reflectionRayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"ReflectionRayGen");
	void* reflectionMissShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"ReflectionMiss");
	void* reflectionHitGroupShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"ReflectionHitGroup");

	ShaderTable reflectionRayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(reflectionRayGenShaderTable.Initialze());
	reflectionRayGenShaderTable.push_back(ShaderRecord(reflectionRayGenShaderIdentifier, shaderIdentifierSize));
	mShaderTables["reflectionRayGen"] = reflectionRayGenShaderTable.GetResource();

	ShaderTable reflectionMissShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(reflectionMissShaderTable.Initialze());
	reflectionMissShaderTable.push_back(ShaderRecord(reflectionMissShaderIdentifier, shaderIdentifierSize));
	mShaderTables["reflectionMiss"] = reflectionMissShaderTable.GetResource();

	ShaderTable reflectionHitGroupTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(reflectionHitGroupTable.Initialze());
	reflectionHitGroupTable.push_back(ShaderRecord(reflectionHitGroupShaderIdentifier, shaderIdentifierSize));
	mShaderTables["reflectionHitGroup"] = reflectionHitGroupTable.GetResource();

	return true;
}

void RaytracedReflectionClass::BuildDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhReflectionMapCpuSrv = hCpu;
	mhReflectionMapGpuSrv = hGpu;
	mhReflectionMapCpuUav = hCpu.Offset(1, descSize);
	mhReflectionMapGpuUav = hGpu.Offset(1, descSize);

	BuildDescriptors();

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
}

bool RaytracedReflectionClass::OnResize(ID3D12GraphicsCommandList*const cmdList, UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources(cmdList));
		BuildDescriptors();
	}

	return true;
}

void RaytracedReflectionClass::Run(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_rr,
		D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::ECB_Rr, cb_rr);
	cmdList->SetComputeRootShaderResourceView(RootSignature::EAS_BVH, as_bvh);

	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_BackBuffer, si_backBuffer);
	cmdList->SetComputeRootDescriptorTable(RootSignature::ESI_NormalDepth, si_normalDepth);

	mReflectionMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, mReflectionMap->Resource());
	cmdList->SetComputeRootDescriptorTable(RootSignature::EUO_Reflection, mhReflectionMapGpuUav);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables["reflectionRayGen"];
	const auto& miss = mShaderTables["reflectionMiss"];
	const auto& hitGroup = mShaderTables["reflectionHitGroup"];
	dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGen->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGen->GetDesc().Width;
	dispatchDesc.MissShaderTable.StartAddress = miss->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = miss->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
	dispatchDesc.Width = mWidth;
	dispatchDesc.Height = mHeight;
	dispatchDesc.Depth = 1;
	cmdList->DispatchRays(&dispatchDesc);

	mReflectionMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, mReflectionMap->Resource());
}

void RaytracedReflectionClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = ReflectionMapFormat;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = ReflectionMapFormat;

	auto resource = mReflectionMap->Resource();
	md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhReflectionMapCpuSrv);
	md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhReflectionMapCpuUav);
}

bool RaytracedReflectionClass::BuildResources(ID3D12GraphicsCommandList* cmdList) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = ReflectionMapFormat;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	{
		CheckReturn(mReflectionMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			L"RaytracedReflectionMap"
		));
	}

		const UINT num2DSubresources = rscDesc.DepthOrArraySize * rscDesc.MipLevels;
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mReflectionMap->Resource(), 0, num2DSubresources);
	
		CheckReturn(mReflectionUploadBuffer->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr
		));
	
		const UINT size = mWidth * mHeight * 8;
		std::vector<BYTE> data(size);
	
		for (UINT i = 0; i < size; i += 4) {
			data[i] = data[i + 1] = data[i + 2] = data[i + 3] = 0;	// rgba-channels(normal) = 0 / 128;
		}
	
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = data.data();
		subResourceData.RowPitch = mWidth * 4;
		subResourceData.SlicePitch = subResourceData.RowPitch * mHeight;
		
		UpdateSubresources(
			cmdList,
			mReflectionMap->Resource(),
			mReflectionUploadBuffer->Resource(),
			0,
			0,
			num2DSubresources,
			&subResourceData
		);
	
		mReflectionMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return true;
}
