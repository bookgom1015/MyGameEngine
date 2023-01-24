#pragma once

#include <d3dx12.h>

class Ssr {
public:
	Ssr();
	virtual ~Ssr();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat);

	__forceinline constexpr UINT SsrMapWidth() const;
	__forceinline constexpr UINT SsrMapHeight() const;

	__forceinline constexpr UINT SsrTempMapWidth() const;
	__forceinline constexpr UINT SsrTempMapHeight() const;

	__forceinline constexpr D3D12_VIEWPORT Viewport() const;
	__forceinline constexpr D3D12_RECT ScissorRect() const;

	__forceinline constexpr DXGI_FORMAT Format() const;

	__forceinline ID3D12Resource* SsrMap0Resource();
	__forceinline ID3D12Resource* SsrMap1Resource();
	__forceinline ID3D12Resource* SsrTempMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SsrMap0Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SsrMap0Rtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SsrMap1Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SsrMap1Rtv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SsrTempMapRtv() const;

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

	static const float ClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mSsrMapWidth;
	UINT mSsrMapHeight;

	UINT mSsrTempMapWidth;
	UINT mSsrTempMapHeight;

	UINT mDivider;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	DXGI_FORMAT mBackBufferFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mSsrMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSsrMap1;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSsrTempMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSsrMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhSsrMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSsrMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSsrMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhSsrMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSsrMap1CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSsrTempMapCpuRtv;
};

constexpr UINT Ssr::SsrMapWidth() const {
	return mSsrMapWidth;
}

constexpr UINT Ssr::SsrMapHeight() const {
	return mSsrMapHeight;
}

constexpr UINT Ssr::SsrTempMapWidth() const {
	return mSsrTempMapWidth;
}

constexpr UINT Ssr::SsrTempMapHeight() const {
	return mSsrTempMapHeight;
}

constexpr D3D12_VIEWPORT Ssr::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Ssr::ScissorRect() const {
	return mScissorRect;
}

constexpr DXGI_FORMAT Ssr::Format() const {
	return mBackBufferFormat;
}

ID3D12Resource* Ssr::SsrMap0Resource() {
	return mSsrMap0.Get();
}

ID3D12Resource* Ssr::SsrMap1Resource() {
	return mSsrMap1.Get();
}

ID3D12Resource* Ssr::SsrTempMapResource() {
	return mSsrTempMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssr::SsrMap0Srv() const {
	return mhSsrMap0GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrMap0Rtv() const {
	return mhSsrMap0CpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssr::SsrMap1Srv() const {
	return mhSsrMap1GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrMap1Rtv() const {
	return mhSsrMap1CpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrTempMapRtv() const {
	return mhSsrTempMapCpuRtv;
}