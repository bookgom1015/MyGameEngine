#pragma once

#include <d3dx12.h>

class DepthOfField {
public:
	DepthOfField();
	virtual ~DepthOfField();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat);
	
	__forceinline constexpr UINT CocMapWidth() const;
	__forceinline constexpr UINT CocMapHeight() const;

	__forceinline constexpr UINT BokehMapWidth() const;
	__forceinline constexpr UINT BokehMapHeight() const;

	__forceinline constexpr UINT DofMapWidth() const;
	__forceinline constexpr UINT DofMapHeight() const;

	__forceinline constexpr D3D12_VIEWPORT Viewport() const;
	__forceinline constexpr D3D12_RECT ScissorRect() const;

	__forceinline constexpr DXGI_FORMAT BokehMapFormat() const;

	__forceinline ID3D12Resource* CocMapResource();
	__forceinline ID3D12Resource* BokehMapResource();
	__forceinline ID3D12Resource* DofMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE CocMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BokehMapSrv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE CocMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BokehMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DofMapRtv() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize);

	bool OnResize(UINT width, UINT height);

public:
	void BuildDescriptors();
	bool BuildResource();

public:
	static const UINT NumRenderTargets = 3;

	static const DXGI_FORMAT CocMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;

	static const float CocMapClearValues[4];
	static const float BokehMapClearValues[4];
	static const float DofMapClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	UINT mDivider;
	UINT mReducedWidth;
	UINT mReducedHeight;

	DXGI_FORMAT mBackBufferFormat;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	Microsoft::WRL::ComPtr<ID3D12Resource> mCocMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBokehMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDofMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhCocMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBokehMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDofMapCpuRtv;
};

constexpr UINT DepthOfField::CocMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::CocMapHeight() const {
	return mHeight;
}

constexpr UINT DepthOfField::BokehMapWidth() const {
	return mReducedWidth;
}

constexpr UINT DepthOfField::BokehMapHeight() const {
	return mReducedHeight;
}

constexpr UINT DepthOfField::DofMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::DofMapHeight() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT DepthOfField::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT DepthOfField::ScissorRect() const {
	return mScissorRect;
}

constexpr DXGI_FORMAT DepthOfField::BokehMapFormat() const {
	return mBackBufferFormat;
}

ID3D12Resource* DepthOfField::CocMapResource() {
	return mCocMap.Get();
}

ID3D12Resource* DepthOfField::BokehMapResource() {
	return mBokehMap.Get();
}

ID3D12Resource* DepthOfField::DofMapResource() {
	return mDofMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::CocMapSrv() const {
	return mhCocMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::BokehMapSrv() const {
	return mhBokehMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::CocMapRtv() const {
	return mhCocMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::BokehMapRtv() const {
	return mhBokehMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DofMapRtv() const {
	return mhDofMapCpuRtv;
}

#include "DepthOfField.inl"