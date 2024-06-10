#ifndef __SHADOWMAP_INL__
#define __SHADOWMAP_INL__

constexpr UINT Shadow::ShadowClass::Width() const {
	return mTexWidth;
}

constexpr UINT Shadow::ShadowClass::Height() const {
	return mTexHeight;
}

constexpr D3D12_VIEWPORT Shadow::ShadowClass::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Shadow::ShadowClass::ScissorRect() const {
	return mScissorRect;
}

GpuResource* Shadow::ShadowClass::Resource(Shadow::Resource::Type type) {
	return mShadowMaps[type].get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Srv(Shadow::Descriptor::Type type) const {
	return mhGpuDescs[type];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Dsv() const {
	return mhCpuDsv;
}

BOOL* Shadow::ShadowClass::DebugShadowMap() {
	return &mDebugShadowMap;
}

#endif // __SHADOWMAP_INL__