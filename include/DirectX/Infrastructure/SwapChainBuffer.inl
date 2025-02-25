#ifndef __SWAPCHAINBUFFER_INL__
#define __SWAPCHAINBUFFER_INL__

GpuResource* SwapChainBuffer::SwapChainBufferClass::BackBuffer(UINT index) const {
	return mSwapChainBuffer[index].get();
}

GpuResource* SwapChainBuffer::SwapChainBufferClass::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurrBackBuffer].get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferRtv() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferSrv() const {
	return mhBackBufferGpuSrvs[mCurrBackBuffer];
}

constexpr UINT SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferIndex() const {
	return mCurrBackBuffer;
}

#endif // __SWAPCHAINBUFFER_INL__