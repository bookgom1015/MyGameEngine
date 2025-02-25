#ifndef __DXR_GEOMETRYBUFFER_INL__
#define __DXR_GEOMETRYBUFFER_INL__

constexpr D3D12_GPU_DESCRIPTOR_HANDLE DXR_GeometryBuffer::DXR_GeometryBufferClass::VerticesSrv() const {
	return mhGpuSrv;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXR_GeometryBuffer::DXR_GeometryBufferClass::IndicesSrv() const {
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mhGpuSrv, GeometryBufferCount, mDescSize);
}

#endif // __DXR_GEOMETRYBUFFER_INL__