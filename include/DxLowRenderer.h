#pragma once

#include "Renderer.h"

class DxLowRenderer {
private:
	using Adapters = std::multimap<size_t, IDXGIAdapter*>;

public:
	DxLowRenderer();
	virtual ~DxLowRenderer();

protected:
	bool LowInitialize(HWND hwnd, UINT width, UINT height);
	void LowCleanUp();
	
	bool LowOnResize(UINT width, UINT height);

	virtual bool CreateRtvAndDsvDescriptorHeaps();

	__forceinline constexpr UINT GetRtvDescriptorSize() const;
	__forceinline constexpr UINT GetDsvDescriptorSize() const;
	__forceinline constexpr UINT GetCbvSrvUavDescriptorSize() const;

	__forceinline constexpr UINT64 GetCurrentFence() const;
	UINT64 IncreaseFence();

	void NextBackBuffer();
		
	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	HRESULT GetDeviceRemovedReason() const;

	bool FlushCommandQueue();

private:
	bool InitDirect3D(UINT width, UINT height);

	void SortAdapters(Adapters& adapters);

	bool CreateDebugObjects();
	bool CreateCommandObjects();
	bool CreateSwapChain(UINT width, UINT height);

protected:
	static const int SwapChainBufferCount = 2;
	static const D3D_DRIVER_TYPE D3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	static const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	Microsoft::WRL::ComPtr<ID3D12InfoQueue1> mInfoQueue;
	
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList;

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

private:
	bool bIsCleanedUp;

	HWND mhMainWnd;

	Microsoft::WRL::ComPtr<ID3D12Debug> mDebugController;
	DWORD mCallbakCookie;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	UINT mdxgiFactoryFlags;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvSrvUavDescriptorSize;

	UINT64 mCurrentFence;

	UINT mRefreshRate;

	int mCurrBackBuffer;
};

#include "DxLowRenderer.inl"