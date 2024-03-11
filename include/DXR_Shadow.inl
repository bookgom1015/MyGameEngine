#ifndef __DXR_SHADOW_INL__
#define __DXR_SHADOW_INL__

D3D12_GPU_DESCRIPTOR_HANDLE DXR_Shadow::DXR_ShadowClass::Descriptor(Descriptors::Type type) const {
	return mhGpuDescs[type];
}

GpuResource* DXR_Shadow::DXR_ShadowClass::Resource(Resources::Type type) {
	return mResources[type].get();
}

#endif // __DXR_SHADOW_INL__