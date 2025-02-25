#include "DirectX/Shading/ZDepth.h"
#include "Common/Debug/Logger.h"

using namespace ZDepth;

ZDepthClass::ZDepthClass() {
	for (UINT i = 0; i < MaxLights; ++i) {
		mZDepthMaps[i] = std::make_unique<GpuResource>();
		mFaceIDCubeMaps[i] = std::make_unique<GpuResource>();
	}
}

BOOL ZDepthClass::Initialize(Locker<ID3D12Device5>* const device, UINT texW, UINT texH) {
	md3dDevice = device;

	mTexWidth = texW;
	mTexHeight = texH;

	return TRUE;
}

void ZDepthClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
		UINT descSize, UINT dsvDescSize) {
	for (UINT i = 0; i < MaxLights; ++i) {
		mhZDepthCpuDescs[i] = hCpu.Offset(1, descSize);
		mhZDepthGpuDescs[i] = hGpu.Offset(1, descSize);
	}
	for (UINT i = 0; i < MaxLights; ++i) {
		mhZDepthCubeCpuDescs[i] = hCpu.Offset(1, descSize);
		mhZDepthCubeGpuDescs[i] = hGpu.Offset(1, descSize);
	}
	for (UINT i = 0; i < MaxLights; ++i) {
		mhFaceIDCubeCpuDescs[i] = hCpu.Offset(1, descSize);
		mhFaceIDCubeGpuDescs[i] = hGpu.Offset(1, descSize);
	}
}

BOOL ZDepthClass::AddLight(UINT lightType, UINT index) {
	if (lightType == LightType::E_Tube || lightType == LightType::E_Rect) return TRUE;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Width = mTexWidth;
	texDesc.Height = mTexHeight;
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	const BOOL needCubemap = lightType == LightType::E_Point || lightType == LightType::E_Spot;

	D3D12Util::Descriptor::Resource::Builder builder;
	if (needCubemap) {
		texDesc.DepthOrArraySize = 6;

		{
			texDesc.Format = Shadow::ZDepthMapFormat;

			std::wstring name = L"ZDepth_ZDepthCubeMap_";
			name.append(std::to_wstring(index));

			builder.Enqueue(
				mZDepthMaps[index].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			);
		}
		{
			texDesc.Format = Shadow::FaceIDCubeMapFormat;

			std::wstring name = L"ZDepth_FaceIDCubeMap_";
			name.append(std::to_wstring(index));

			builder.Enqueue(
				mFaceIDCubeMaps[index].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			);
		}
	}
	else {
		texDesc.Format = Shadow::ZDepthMapFormat;
		texDesc.DepthOrArraySize = 1;

		std::wstring name = L"ZDepth_ZDepthMap_";
		name.append(std::to_wstring(index));

		builder.Enqueue(
			mZDepthMaps[index].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			name.c_str()
		);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}
	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		D3D12Util::Descriptor::View::Builder builder_view;
		AllocateDescriptors(builder_view, index, needCubemap);
		builder_view.Build(device);
	}

	return TRUE;
}

void ZDepthClass::AllocateDescriptors(D3D12Util::Descriptor::View::Builder& builder, UINT index, BOOL needCubemap) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (needCubemap) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.ArraySize = 6;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.f;

		{
			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			const auto& zdepthCube = mZDepthMaps[index]->Resource();

			builder.Enqueue(zdepthCube, srvDesc, mhZDepthCubeCpuDescs[index]);
		}
		{
			srvDesc.Format = Shadow::FaceIDCubeMapFormat;
			const auto& faceIDCube = mFaceIDCubeMaps[index]->Resource();

			builder.Enqueue(faceIDCube, srvDesc, mhFaceIDCubeCpuDescs[index]);
		}
	}
	else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

		const auto& zdepth = mZDepthMaps[index]->Resource();
		builder.Enqueue(zdepth, srvDesc, mhZDepthCpuDescs[index]);
	}
}