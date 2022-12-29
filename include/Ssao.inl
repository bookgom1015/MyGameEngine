#ifndef __SSAO_INL__
#define __SSAO_INL__

constexpr UINT Ssao::Width() const {
	return mWidth;
}

constexpr UINT Ssao::Height() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT Ssao::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Ssao::ScissorRect() const {
	return mScissorRect;
}

ID3D12Resource* Ssao::AmbientMap0Resource() {
	return mAmbientMap0.Get();
}

ID3D12Resource* Ssao::AmbientMap1Resource() {
	return mAmbientMap1.Get();
}

ID3D12Resource* Ssao::RandomVectorMapResource() {
	return mRandomVectorMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::AmbientMap0Srv() const {
	return mhAmbientMap0GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::AmbientMap0Rtv() const {
	return mhAmbientMap0CpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::AmbientMap1Srv() const {
	return mhAmbientMap1GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::AmbientMap1Rtv() const {
	return mhAmbientMap1CpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::RandomVectorMapSrv() const {
	return mhRandomVectorMapGpuSrv;
}

#endif // __SSAO_INL__