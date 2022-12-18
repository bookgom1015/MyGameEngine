#include "GBuffer.h"
#include "Logger.h"

const float GBuffer::ColorMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const float GBuffer::NormalMapClearValues[4] = { 0.0f, 0.0f, 1.0f, 0.0f };

GBuffer::GBuffer() {}

GBuffer::~GBuffer() {}

bool GBuffer::Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT colorFormat, DXGI_FORMAT normalFormat, DXGI_FORMAT depthFormat) {
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mColorFormat = colorFormat;
	mNormalFormat = normalFormat;
	mDepthFormat = depthFormat;

	CheckReturn(BuildResource());

	return true;
}

ID3D12Resource* GBuffer::ColorMapResource() {
	return mColorMap.Get();
}

ID3D12Resource* GBuffer::NormalMapResource() {
	return mNormalMap.Get();
}

void GBuffer::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
	UINT descSize, UINT rtvDescSize,
	ID3D12Resource* depth) {
	mhColorCpuSrv = hCpuSrv;
	mhColorGpuSrv = hGpuSrv;
	mhColorCpuRtv = hCpuRtv;

	mhNormalCpuSrv = hCpuSrv.Offset(1, descSize);
	mhNormalGpuSrv = hGpuSrv.Offset(1, descSize);
	mhNormalCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhDepthCpuSrv = hCpuSrv.Offset(1, descSize);
	mhDepthGpuSrv = hGpuSrv.Offset(1, descSize);

	BuildDescriptors(depth);
}

bool GBuffer::OnResize(UINT width, UINT height, ID3D12Resource* depth) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResource());
		BuildDescriptors(depth);
	}

	return true;
}

void GBuffer::BuildDescriptors(ID3D12Resource* depth) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	// Creates a shader resource view for color map.
	srvDesc.Format = mColorFormat;
	md3dDevice->CreateShaderResourceView(mColorMap.Get(), &srvDesc, mhColorCpuSrv);

	// Creates a render target view for color map.
	rtvDesc.Format = mColorFormat;
	md3dDevice->CreateRenderTargetView(mColorMap.Get(), &rtvDesc, mhColorCpuRtv);

	// Create a shader resource view for normal map.
	srvDesc.Format = mNormalFormat;
	md3dDevice->CreateShaderResourceView(mNormalMap.Get(), &srvDesc, mhNormalCpuSrv);

	// Create a render target view for normal map.
	rtvDesc.Format = mNormalFormat;
	md3dDevice->CreateRenderTargetView(mNormalMap.Get(), &rtvDesc, mhNormalCpuRtv);

	// Create a shader resource view for depth map.
	srvDesc.Format = mDepthFormat;
	md3dDevice->CreateShaderResourceView(depth, &srvDesc, mhDepthCpuSrv);
}

bool GBuffer::BuildResource() {
	mColorMap = nullptr;
	mNormalMap = nullptr;

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	//
	// Creates a resource for color map.
	//
	rscDesc.Format = mColorFormat;

	CD3DX12_CLEAR_VALUE optClear(mColorFormat, ColorMapClearValues);

	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mColorMap.GetAddressOf())
	));
	
	//
	// Creates a resource for normal map.
	//
	rscDesc.Format = mNormalFormat;

	float normalClearColor[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	optClear = CD3DX12_CLEAR_VALUE(mNormalFormat, NormalMapClearValues);

	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mNormalMap.GetAddressOf())
	));

	return true;
}