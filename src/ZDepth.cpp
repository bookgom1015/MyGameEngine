#include "ZDepth.h"
#include "Logger.h"

using namespace ZDepth;

ZDepthClass::ZDepthClass() {
	for (UINT i = 0; i < MaxLights; ++i)
		mZDepthMaps[i] = std::make_unique<GpuResource>();
}

BOOL ZDepthClass::Initialize(ID3D12Device* const device, UINT texW, UINT texH) {
	md3dDevice = device;

	mTexWidth = texW;
	mTexHeight = texH;

	return TRUE;
}

void ZDepthClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
		UINT descSize, UINT dsvDescSize) {
	for (UINT i = 0; i < MaxLights; ++i) {
		mhCpuDescs[i] = hCpu.Offset(1, descSize);
		mhGpuDescs[i] = hGpu.Offset(1, descSize);

		mhCpuDsvs[i] = hCpuDsv.Offset(1, dsvDescSize);
	}
}

BOOL ZDepthClass::AddLight(UINT lightType, UINT index) {
	if (lightType == LightType::E_Tube || lightType == LightType::E_Rect) return TRUE;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Format = Shadow::ZDepthMapFormat;
	texDesc.Width = mTexWidth;
	texDesc.Height = mTexHeight;
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = Shadow::ZDepthMapFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	BOOL needCubemap = lightType == LightType::E_Point || lightType == LightType::E_Spot;

	if (needCubemap) {
		texDesc.DepthOrArraySize = 6;

		std::wstring name = L"Shadow_ZDepthCubeMap_";
		name.append(std::to_wstring(index));

		CheckReturn(mZDepthMaps[index]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&optClear,
			name.c_str()
		));
	}
	else {
		texDesc.DepthOrArraySize = 1;

		std::wstring name = L"Shadow_ZDepthMap_";
		name.append(std::to_wstring(index));

		CheckReturn(mZDepthMaps[index]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_DEPTH_READ,
			&optClear,
			name.c_str()
		));
	}

	BuildDescriptor(index, needCubemap);

	return TRUE;
}

void ZDepthClass::BuildDescriptor(UINT index, BOOL needCubemap) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

	if (needCubemap) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.ArraySize = 6;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Format = Shadow::ZDepthMapFormat;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2DArray.ArraySize = 6;
		dsvDesc.Texture2DArray.FirstArraySlice = 0;
		dsvDesc.Texture2DArray.MipSlice = 0;

		const auto& depthCube = mZDepthMaps[index]->Resource();

		md3dDevice->CreateDepthStencilView(depthCube, &dsvDesc, mhCpuDsvs[index]);
		md3dDevice->CreateShaderResourceView(depthCube, &srvDesc, mhCpuDescs[index]);
	}
	else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = Shadow::ZDepthMapFormat;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		const auto& depth = mZDepthMaps[index]->Resource();

		md3dDevice->CreateDepthStencilView(depth, &dsvDesc, mhCpuDsvs[index]);
		md3dDevice->CreateShaderResourceView(depth, &srvDesc, mhCpuDescs[index]);
	}
}