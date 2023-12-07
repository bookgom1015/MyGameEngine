#pragma once

#include "Renderer.h"
#include "SwapChainBuffer.h"
#include "DepthStencilBuffer.h"

class DxLowRenderer {
private:
	using Adapters = std::multimap<size_t, IDXGIAdapter*>;

public:
	DxLowRenderer();
	virtual ~DxLowRenderer();

protected:
	__forceinline constexpr UINT GetRtvDescriptorSize() const;
	__forceinline constexpr UINT GetDsvDescriptorSize() const;
	__forceinline constexpr UINT GetCbvSrvUavDescriptorSize() const;

	__forceinline constexpr UINT64 GetCurrentFence() const;

	__forceinline constexpr BOOL AllowTearing() const;

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	HRESULT GetDeviceRemovedReason() const;

protected:
	BOOL LowInitialize(HWND hwnd, UINT width, UINT height);
	void LowCleanUp();
	
	BOOL LowOnResize(UINT width, UINT height);

	virtual BOOL CreateRtvAndDsvDescriptorHeaps();
	UINT64 IncreaseFence();

	BOOL FlushCommandQueue();

private:
	BOOL InitDirect3D(UINT width, UINT height);

	void SortAdapters(Adapters& adapters);

	BOOL CreateDebugObjects();
	BOOL CreateCommandObjects();
	BOOL CreateSwapChain(UINT width, UINT height);

	void BuildDescriptors();

protected:
	static const INT SwapChainBufferCount = 2;

	static const D3D_DRIVER_TYPE D3DDriverType = D3D_DRIVER_TYPE_HARDWARE;

	Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavHeap;

	Microsoft::WRL::ComPtr<ID3D12InfoQueue1> mInfoQueue;
	
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList;

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	HWND mhMainWnd;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuCbvSrvUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuCbvSrvUav;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;

	std::unique_ptr<SwapChainBuffer::SwapChainBufferClass> mSwapChainBuffer;
	std::unique_ptr<DepthStencilBuffer::DepthStencilBufferClass> mDepthStencilBuffer;

private:
	BOOL bIsCleanedUp;

	BOOL bAllowTearing;

	Microsoft::WRL::ComPtr<ID3D12Debug> mDebugController;
	DWORD mCallbakCookie;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	UINT mdxgiFactoryFlags;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvSrvUavDescriptorSize;

	UINT64 mCurrentFence;
};

#include "DxLowRenderer.inl"