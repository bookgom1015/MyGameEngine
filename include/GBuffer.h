#pragma once

#include <d3dx12.h>

class GBuffer {
public:
	GBuffer();
	virtual ~GBuffer();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, 
		DXGI_FORMAT colorMapFormat, DXGI_FORMAT normalMapFormat, DXGI_FORMAT depthMapFormat, DXGI_FORMAT specularMapFormat);

	__forceinline constexpr UINT Width() const;
	__forceinline constexpr UINT Height() const;
	
	ID3D12Resource* ColorMapResource();
	ID3D12Resource* AlbedoMapResource();
	ID3D12Resource* NormalMapResource();
	ID3D12Resource* SpecularMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ColorMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AlbedoMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthMapSrv() const;
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SpecularMapSrv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ColorMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AlbedoMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SpecularMapRtv() const;

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
	static const UINT NumRenderTargets = 4;

	static const float ColorMapClearValues[4];
	static const float AlbedoMapClearValues[4];
	static const float NormalMapClearValues[4];
	static const float SpecularMapClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	DXGI_FORMAT mColorMapFormat;
	DXGI_FORMAT mNormalMapFormat;
	DXGI_FORMAT mDepthMapFormat;
	DXGI_FORMAT mSpecularMapFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mColorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAlbedoMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSpecularMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhColorMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAlbedoMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhSpecularMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuRtv;
};

#include "GBuffer.inl"