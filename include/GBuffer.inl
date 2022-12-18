#ifndef __GBUFFER_INL__
#define __GBUFFER_INL__

constexpr UINT GBuffer::Width() const {
	return mWidth;
}

constexpr UINT GBuffer::Height() const {
	return mHeight;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::ColorMapSrv() const {
	return mhColorGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::NormalMapSrv() const {
	return mhNormalGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::DepthMapSrv() const {
	return mhDepthGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::ColorMapRtv() const {
	return mhColorCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::NormalMapRtv() const {
	return mhNormalCpuRtv;
}

#endif // __GBUFFER_INL__