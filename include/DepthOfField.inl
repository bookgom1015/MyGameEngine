#ifndef __DEPTHOFFIELD_INL__
#define __DEPTHOFFIELD_INL__

constexpr UINT DepthOfField::CocMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::CocMapHeight() const {
	return mHeight;
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

ID3D12Resource* DepthOfField::CocMapResource() {
	return mCocMap.Get();
}

ID3D12Resource* DepthOfField::DofMapResource() {
	return mDofMap.Get();
}

ID3D12Resource* DepthOfField::DofBlurMapResource() {
	return mDofBlurMap.Get();
}

ID3D12Resource* DepthOfField::FocusDistanceBufferResource() {
	return mFocusDistanceBuffer.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::CocMapSrv() const {
	return mhCocMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::CocMapRtv() const {
	return mhCocMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DofMapSrv() const {
	return mhDofMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DofMapRtv() const {
	return mhDofMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DofBlurMapSrv() const {
	return mhDofBlurMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DofBlurMapRtv() const {
	return mhDofBlurMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::FocusDistanceBufferUav() const {
	return mhFocusDistanceGpuUav;
}

#endif // __DEPTHOFFIELD_INL__