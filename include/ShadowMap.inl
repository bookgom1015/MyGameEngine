#ifndef __SHADOWMAP_INL__
#define __SHADOWMAP_INL__

constexpr UINT ShadowMap::Width() const {
	return mWidth;
}

constexpr UINT ShadowMap::Height() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT ShadowMap::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT ShadowMap::ScissorRect() const {
	return mScissorRect;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const {
	return mhGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const {
	return mhCpuDsv;
}

#endif // __SHADOWMAP_INL__