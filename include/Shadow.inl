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

GpuResource* Shadow::ShadowClass::Resource() {
	return mShadowMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Srv() const {
	return mhGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Shadow::ShadowClass::Dsv() const {
	return mhCpuDsv;
}

#endif // __SHADOWMAP_INL__