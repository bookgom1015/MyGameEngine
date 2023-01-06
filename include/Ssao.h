#pragma once

#include <d3dx12.h>

#include "MathHelper.h"
#include "FrameResource.h"

class Ssao {
public:
	Ssao();
	virtual ~Ssao();

public:
	bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height, DXGI_FORMAT ambientMapFormat);

	__forceinline constexpr UINT Width() const;
	__forceinline constexpr UINT Height() const;

	__forceinline constexpr D3D12_VIEWPORT Viewport() const;
	__forceinline constexpr D3D12_RECT ScissorRect() const;

	__forceinline ID3D12Resource* AmbientMap0Resource();
	__forceinline ID3D12Resource* AmbientMap1Resource();
	__forceinline ID3D12Resource* RandomVectorMapResource();
	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMap0Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AmbientMap0Rtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMap1Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AmbientMap1Rtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE RandomVectorMapSrv() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize);

	bool OnResize(UINT width, UINT height);

private:
	void BuildDescriptors();
	bool BuildResource();

	void BuildOffsetVectors();
	bool BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);

public:
	static const UINT NumRenderTargets = 2;

	static const float AmbientMapClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	DXGI_FORMAT mAmbientMapFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

	// Need two for ping-ponging during blur.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	DirectX::XMFLOAT4 mOffsets[14];

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
};

#include "Ssao.inl"