#ifndef __SSAO_INL__
#define __SSAO_INL__

constexpr UINT Ssao::SsaoClass::Width() const {
	return mWidth;
}

constexpr UINT Ssao::SsaoClass::Height() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT Ssao::SsaoClass::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Ssao::SsaoClass::ScissorRect() const {
	return mScissorRect;
}

ID3D12Resource* Ssao::SsaoClass::AOCoefficientMapResource(UINT index) {
	return mAOCoefficientMaps[index].Get();
}

ID3D12Resource* Ssao::SsaoClass::RandomVectorMapResource() {
	return mRandomVectorMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::SsaoClass::AOCoefficientMapSrv(UINT index) const {
	return mhAOCoefficientMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssao::SsaoClass::AOCoefficientMapRtv(UINT index) const {
	return mhAOCoefficientMapCpuRtvs[index];
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssao::SsaoClass::RandomVectorMapSrv() const {
	return mhRandomVectorMapGpuSrv;
}

#endif // __SSAO_INL__