#include "DxrShadowMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "ShaderTable.h"
#include "MathHelper.h"

using namespace DxrShadowMap;

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

	CheckReturn(BuildResource(cmdList));

	return true;
}

bool DxrShadowMapClass::CompileShaders(const std::wstring& filePath) {
	const auto fullPath = filePath + L"ShadowRay.hlsl";
	auto shaderInfo = D3D12ShaderInfo(fullPath.c_str(), L"", L"lib_6_3");
	CheckReturn(mShaderManager->CompileShader(shaderInfo, "shadowRay"));

	return true;
}

bool DxrShadowMapClass::BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[DxrShadowMap::RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[4];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, geometryBufferCount, 0, 1);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, geometryBufferCount, 0, 2);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	slotRootParameter[DxrShadowMap::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[DxrShadowMap::RootSignatureLayout::ESI_AccelerationStructure].InitAsShaderResourceView(0);
	slotRootParameter[DxrShadowMap::RootSignatureLayout::ESB_Vertices].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[DxrShadowMap::RootSignatureLayout::EAB_Indices].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[DxrShadowMap::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[DxrShadowMap::RootSignatureLayout::EUO_Shadow].InitAsDescriptorTable(1, &texTables[3]);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return true;
}

bool DxrShadowMapClass::BuildPso() {
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	CD3DX12_STATE_OBJECT_DESC shadowDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto shadowRayLib = shadowDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto shadowRayShader = mShaderManager->GetDxcShader("shadowRay");
	D3D12_SHADER_BYTECODE shadowRayLibDxil = CD3DX12_SHADER_BYTECODE(shadowRayShader->GetBufferPointer(), shadowRayShader->GetBufferSize());
	shadowRayLib->SetDXILLibrary(&shadowRayLibDxil);
	LPCWSTR shadowRayExports[] = { L"ShadowRayGen", L"ShadowClosestHit", L"ShadowMiss" };
	shadowRayLib->DefineExports(shadowRayExports);

	auto shadowHitGroup = shadowDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	shadowHitGroup->SetClosestHitShaderImport(L"ShadowClosestHit");
	shadowHitGroup->SetHitGroupExport(L"ShadowHitGroup");
	shadowHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = shadowDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(DirectX::XMFLOAT4);	// for pixel color
	UINT attribSize = sizeof(DirectX::XMFLOAT2);	// for barycentrics
	shaderConfig->Config(payloadSize, attribSize);

	//auto localRootSig = dxrPso.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	//localRootSig->SetRootSignature(mRootSignatures["dxr_local"].Get());
	//{
	//	auto rootSigAssociation = dxrPso.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	//	rootSigAssociation->SetSubobjectToAssociate(*localRootSig);
	//	rootSigAssociation->AddExport(L"HitGroup");
	//}

	auto glbalRootSig = shadowDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignature.Get());

	auto pipelineConfig = shadowDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(shadowDxrPso, IID_PPV_ARGS(&mPSO)));
	CheckHRESULT(mPSO->QueryInterface(IID_PPV_ARGS(&mPSOProp)));

	return true;
}

bool DxrShadowMapClass::BuildShaderTables() {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	void* shadowRayGenShaderIdentifier = mPSOProp->GetShaderIdentifier(L"ShadowRayGen");
	void* shadowMissShaderIdentifier = mPSOProp->GetShaderIdentifier(L"ShadowMiss");
	void* shadowHitGroupShaderIdentifier = mPSOProp->GetShaderIdentifier(L"ShadowHitGroup");

	ShaderTable shadowRayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(shadowRayGenShaderTable.Initialze());
	shadowRayGenShaderTable.push_back(ShaderRecord(shadowRayGenShaderIdentifier, shaderIdentifierSize));
	mShaderTables["shadowRayGen"] = shadowRayGenShaderTable.GetResource();

	ShaderTable shadowMissShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(shadowMissShaderTable.Initialze());
	shadowMissShaderTable.push_back(ShaderRecord(shadowMissShaderIdentifier, shaderIdentifierSize));
	mShaderTables["shadowMiss"] = shadowMissShaderTable.GetResource();

	ShaderTable shadowHitGroupTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(shadowHitGroupTable.Initialze());
	shadowHitGroupTable.push_back(ShaderRecord(shadowHitGroupShaderIdentifier, shaderIdentifierSize));
	mShaderTables["shadowHitGroup"] = shadowHitGroupTable.GetResource();

	return true;
}

void DxrShadowMapClass::Run(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_VIRTUAL_ADDRESS objSBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS matSBAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE i_vertices,
		D3D12_GPU_DESCRIPTOR_HANDLE i_indices,
		D3D12_GPU_DESCRIPTOR_HANDLE i_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE o_shadow,
		UINT width, UINT height) {
}


void DxrShadowMapClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow0] = hCpu;
	mhResourcesGpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow0] = hGpu;
	mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow0] = hCpu;
	mhResourcesGpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow0] = hGpu;

	mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow1] = hCpu.Offset(1, descSize);
	mhResourcesGpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow1] = hGpu.Offset(1, descSize);
	mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow1] = hCpu.Offset(1, descSize);
	mhResourcesGpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow1] = hGpu.Offset(1, descSize);

	BuildDescriptors();

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
}

bool DxrShadowMapClass::OnResize(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResource(cmdList));
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
	md3dDevice->CreateShaderResourceView(pRawResource, &srvDesc, mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow0]);
	md3dDevice->CreateUnorderedAccessView(pRawResource, nullptr, &uavDesc, mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow0]);

	auto pSmoothedResource = mResources[DxrShadowMap::Resources::EShadow1]->Resource();
	md3dDevice->CreateShaderResourceView(pSmoothedResource, &srvDesc, mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::ES_Shadow1]);
	md3dDevice->CreateUnorderedAccessView(pSmoothedResource, nullptr, &uavDesc, mhResourcesCpuDescriptors[DxrShadowMap::Resources::Descriptors::EU_Shadow1]);
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