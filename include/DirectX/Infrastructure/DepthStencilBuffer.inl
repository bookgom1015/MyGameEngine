#ifndef __DEPTHSTENCILBUFFER_INL__
#define __DEPTHSTENCILBUFFER_INL__

GpuResource* DepthStencilBuffer::DepthStencilBufferClass::Resource() {
	return mDepthStencilBuffer.get();
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::DepthStencilBufferClass::Dsv() const {
	return mhCpuDsv;
}

#endif