#pragma once

#include <d3dx12.h>

class GBuffer {
public:
	GBuffer();
	virtual ~GBuffer();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT colorFormat, DXGI_FORMAT normalFormat, DXGI_FORMAT depthFormat);

	__forceinline constexpr UINT Width() const;
	__forceinline constexpr UINT Height() const;
	
	ID3D12Resource* ColorMapResource();
	ID3D12Resource* NormalMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ColorMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthMapSrv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ColorMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize,
		ID3D12Resource* depth);

	bool OnResize(UINT width, UINT height, ID3D12Resource* depth);

private:
	void BuildDescriptors(ID3D12Resource* depth);
	bool BuildResource();

public:
	static const UINT NumRenderTargets = 2;

	static const float ColorMapClearValues[4];
	static const float NormalMapClearValues[4];

public:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	DXGI_FORMAT mColorFormat;
	DXGI_FORMAT mNormalFormat;
	DXGI_FORMAT mDepthFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mColorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhColorGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthGpuSrv;
};

#include "GBuffer.inl"