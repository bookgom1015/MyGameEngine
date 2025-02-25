#ifndef __DXLOWRENDERER_INL__
#define __DXLOWRENDERER_INL__

constexpr UINT DxLowRenderer::GetRtvDescriptorSize() const {
	return mRtvDescriptorSize;
}

constexpr UINT DxLowRenderer::GetDsvDescriptorSize() const {
	return mDsvDescriptorSize;
}

constexpr UINT DxLowRenderer::GetCbvSrvUavDescriptorSize() const {
	return mCbvSrvUavDescriptorSize;
}

constexpr UINT64 DxLowRenderer::GetCurrentFence() const {
	return mCurrentFence;
}

constexpr BOOL DxLowRenderer::AllowTearing() const {
	return bAllowTearing;
}

#endif // __DXLOWRENDERER_INL__