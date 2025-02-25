#ifndef __ZDEPTH_INL__
#define __ZDEPTH_INL__

GpuResource* ZDepth::ZDepthClass::ZDepthMap(UINT index) const {
	return mZDepthMaps[index].get();
}

GpuResource* ZDepth::ZDepthClass::FaceIDCubeMap(UINT index) const {
	return mFaceIDCubeMaps[index].get();
}

D3D12_GPU_DESCRIPTOR_HANDLE ZDepth::ZDepthClass::ZDepthSrvs() const {
	return mhZDepthGpuDescs[0];
}

D3D12_GPU_DESCRIPTOR_HANDLE ZDepth::ZDepthClass::ZDepthCubeSrvs() const {
	return mhZDepthCubeGpuDescs[0];
}

D3D12_GPU_DESCRIPTOR_HANDLE ZDepth::ZDepthClass::FaceIDCubeSrvs() const {
	return mhFaceIDCubeGpuDescs[0];
}

#endif // __ZDEPTH_INL__