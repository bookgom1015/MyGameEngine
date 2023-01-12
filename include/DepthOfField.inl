#ifndef __DEPTHOFFIELD_INL__
#define __DEPTHOFFIELD_INL__

constexpr UINT DepthOfField::CocMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::CocMapHeight() const {
	return mHeight;
}

constexpr UINT DepthOfField::BokehMapWidth() const {
	return mReducedWidth;
}

constexpr UINT DepthOfField::BokehMapHeight() const {
	return mReducedHeight;
}

constexpr UINT DepthOfField::DofMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::DofMapHeight() const {
	return mHeight;
}

constexpr D3D12_VIEWPORT DepthOfField::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT DepthOfField::ScissorRect() const {
	return mScissorRect;
}

constexpr DXGI_FORMAT DepthOfField::BokehMapFormat() const {
	return mBackBufferFormat;
}

ID3D12Resource* DepthOfField::CocMapResource() {
	return mCocMap.Get();
}

ID3D12Resource* DepthOfField::BokehMapResource() {
	return mBokehMap.Get();
}

ID3D12Resource* DepthOfField::DofMapResource() {
	return mDofMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::CocMapSrv() const {
	return mhCocMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::CocMapRtv() const {
	return mhCocMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::BokehMapSrv() const {
	return mhBokehMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::BokehMapRtv() const {
	return mhBokehMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DofMapRtv() const {
	return mhDofMapCpuRtv;
}

#endif // __DEPTHOFFIELD_INL__