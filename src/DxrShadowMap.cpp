#include "DxrShadowMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "ShaderTable.h"
#include "MathHelper.h"

using namespace DxrShadowMap;

namespace {
	const std::string ShadowRayCS	= "ShadowRayCS";

	const wchar_t* ShadowRayGen		= L"ShadowRayGen"; 
	const wchar_t* ShadowClosestHit	= L"ShadowClosestHit";
	const wchar_t* ShadowMiss		= L"ShadowMiss";
	const wchar_t* ShadowHitGroup	= L"ShadowHitGroup";

	const char* ShadowRayGenShaderTable		= "ShadowRayGenShaderTalbe";
	const char* ShadowMissShaderTable		= "ShadowMissShaderTalbe";
	const char* ShadowHitGroupShaderTable	= "ShadowHitGroupShaderTalbe";
}

DxrShadowMapClass::DxrShadowMapClass() {
	mResources[Resources::EShadow0] = std::make_unique<GpuResource>();
	mResources[Resources::EShadow1] = std::make_unique<GpuResource>();
}

bool DxrShadowMapClass::Initialize(
		ID3D12Device5* const device, 
		ID3D12GraphicsCommandList* const cmdList, 
		ShaderManager* const manager, 
		UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

	CheckReturn(BuildResource(cmdList));

	return true;
}

bool DxrShadowMapClass::CompileShaders(const std::wstring& filePath) {
	const auto fullPath = filePath + L"ShadowRay.hlsl";
	auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"", L"lib_6_3");
	CheckReturn(mShaderManager->CompileShader(shaderInfo, ShadowRayCS));

	return true;
}

bool DxrShadowMapClass::BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[DxrShadowMap::RootSignature::Global::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::Global::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::Global::ESI_AccelerationStructure].InitAsShaderResourceView(0);
		slotRootParameter[RootSignature::Global::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::Global::EUO_Shadow].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSignatureDesc, &mRootSignatures[RootSignature::E_Global]));
	}

	return true;
}

bool DxrShadowMapClass::BuildPso() {
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	CD3DX12_STATE_OBJECT_DESC dxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto shadowRayLib = dxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto shadowRayShader = mShaderManager->GetDxcShader(ShadowRayCS);
	D3D12_SHADER_BYTECODE shadowRayLibDxil = CD3DX12_SHADER_BYTECODE(shadowRayShader->GetBufferPointer(), shadowRayShader->GetBufferSize());
	shadowRayLib->SetDXILLibrary(&shadowRayLibDxil);
	LPCWSTR shadowRayExports[] = { ShadowRayGen, ShadowClosestHit, ShadowMiss };
	shadowRayLib->DefineExports(shadowRayExports);

	auto shadowHitGroup = dxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	shadowHitGroup->SetClosestHitShaderImport(ShadowClosestHit);
	shadowHitGroup->SetHitGroupExport(ShadowHitGroup);
	shadowHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4; // IsHit(bool)
	UINT attribSize = sizeof(DirectX::XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = dxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	auto pipelineConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(dxrPso, IID_PPV_ARGS(&mPSO)));
	CheckHRESULT(mPSO->QueryInterface(IID_PPV_ARGS(&mPSOProp)));

	return true;
}

bool DxrShadowMapClass::BuildShaderTables(UINT numInst) {
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
		ShaderTable hitGroupTable(md3dDevice, numInst, shaderIdentifierSize);
		CheckReturn(hitGroupTable.Initialze());
		for (UINT i = 0; i < numInst; ++i)
			hitGroupTable.push_back(ShaderRecord(shadowHitGroupShaderIdentifier, shaderIdentifierSize));
		mHitGroupShaderTableStrideInBytes = hitGroupTable.GetShaderRecordSize();
		mShaderTables[ShadowHitGroupShaderTable] = hitGroupTable.GetResource();
	}

	return true;
}

void DxrShadowMapClass::Run(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE i_vertices,
		D3D12_GPU_DESCRIPTOR_HANDLE i_indices,
		D3D12_GPU_DESCRIPTOR_HANDLE i_depth) {
	cmdList->SetPipelineState1(mPSO.Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_Global].Get());

	cmdList->SetComputeRootShaderResourceView(RootSignature::Global::ESI_AccelerationStructure, accelStruct);
	cmdList->SetComputeRootConstantBufferView(RootSignature::Global::ECB_Pass, cbAddress);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::ESI_Depth, i_depth);

	const auto shadow0 = mResources[Resources::EShadow0].get();
	const auto shadow1 = mResources[Resources::EShadow1].get();

	shadow0->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	shadow1->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->SetComputeRootDescriptorTable(RootSignature::Global::EUO_Shadow, mhGpuDescs[Descriptors::EU_Shadow0]);

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
	dispatchDesc.Width = mWidth;
	dispatchDesc.Height = mHeight;
	dispatchDesc.Depth = 1;

	cmdList->SetPipelineState1(mPSO.Get());
	cmdList->DispatchRays(&dispatchDesc);
	
	shadow0->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	shadow1->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}


void DxrShadowMapClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhCpuDescs[Descriptors::ES_Shadow0] = hCpu;
	mhGpuDescs[Descriptors::ES_Shadow0] = hGpu;
	mhCpuDescs[Descriptors::EU_Shadow0] = hCpu;
	mhGpuDescs[Descriptors::EU_Shadow0] = hGpu;

	mhCpuDescs[Descriptors::ES_Shadow1] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptors::ES_Shadow1] = hGpu.Offset(1, descSize);
	mhCpuDescs[Descriptors::EU_Shadow1] = hCpu.Offset(1, descSize);
	mhGpuDescs[Descriptors::EU_Shadow1] = hGpu.Offset(1, descSize);

	BuildDescriptors();

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
}

bool DxrShadowMapClass::OnResize(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

		CheckReturn(BuildResource(cmdList));
		BuildDescriptors();
	}

	return true;
}

void DxrShadowMapClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	srvDesc.Format = ShadowFormat;
	uavDesc.Format = ShadowFormat;

	auto pRawResource = mResources[DxrShadowMap::Resources::EShadow0]->Resource();
	md3dDevice->CreateShaderResourceView(pRawResource, &srvDesc, mhCpuDescs[Descriptors::ES_Shadow0]);
	md3dDevice->CreateUnorderedAccessView(pRawResource, nullptr, &uavDesc, mhCpuDescs[Descriptors::EU_Shadow0]);

	auto pSmoothedResource = mResources[DxrShadowMap::Resources::EShadow1]->Resource();
	md3dDevice->CreateShaderResourceView(pSmoothedResource, &srvDesc, mhCpuDescs[Descriptors::ES_Shadow1]);
	md3dDevice->CreateUnorderedAccessView(pSmoothedResource, nullptr, &uavDesc, mhCpuDescs[Descriptors::EU_Shadow1]);
}

bool DxrShadowMapClass::BuildResource(ID3D12GraphicsCommandList* const cmdList) {
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Format = ShadowFormat;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;

	for (int i = 0; i < 2; ++i) {
		std::wstringstream wsstream;
		wsstream << "DXR_ShadowMap" << i;

		CheckReturn(mResources[DxrShadowMap::Resources::EShadow0 + i]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			wsstream.str().c_str()
		));
	}

	return true;
}