#ifndef __DXR_SHADOW_INL__
#define __DXR_SHADOW_INL__

D3D12_GPU_DESCRIPTOR_HANDLE DXR_Shadow::DXR_ShadowClass::Descriptor() const {
	return mhGpuDesc;
}

GpuResource* DXR_Shadow::DXR_ShadowClass::Resource() {
	return mResource.get();
}

#endif // __DXR_SHADOW_INL__