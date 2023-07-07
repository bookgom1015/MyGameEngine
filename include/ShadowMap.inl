#ifndef __SHADOWMAP_INL__
#define __SHADOWMAP_INL__

constexpr UINT ShadowMap::ShadowMapClass::Width() const {
	return mWidth;
}

constexpr UINT ShadowMap::ShadowMapClass::Height() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT ShadowMap::ShadowMapClass::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT ShadowMap::ShadowMapClass::ScissorRect() const {
	return mScissorRect;
}

GpuResource* ShadowMap::ShadowMapClass::Resource() {
	return mShadowMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::ShadowMapClass::Srv() const {
	return mhGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::ShadowMapClass::Dsv() const {
	return mhCpuDsv;
}

#endif // __SHADOWMAP_INL__