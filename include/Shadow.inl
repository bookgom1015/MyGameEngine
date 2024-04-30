#ifndef __SHADOWMAP_INL__
#define __SHADOWMAP_INL__

constexpr UINT Shadow::ShadowClass::Width() const {
	return mWidth;
}

constexpr UINT Shadow::ShadowClass::Height() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT Shadow::ShadowClass::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Shadow::ShadowClass::ScissorRect() const {
	return mScissorRect;
}

GpuResource* Shadow::ShadowClass::Resource(UINT index) {
	if (index >= NumDepthStenciles) index = 0;
	return mShadowMaps[index].get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Srv(UINT index) const {
	if (index >= NumDepthStenciles) index = 0;
	return mhGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Dsv(UINT index) const {
	if (index >= NumDepthStenciles) index = 0;
	return mhCpuDsvs[index];
}

BOOL* Shadow::ShadowClass::DebugShadowMap(UINT index) {
	if (index >= NumDepthStenciles) index = 0;
	return &mDebugShadowMaps[index];
}

#endif // __SHADOWMAP_INL__