#ifndef __IRRADIANCEMAP_INL__
#define __IRRADIANCEMAP_INL__

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::EquirectangularMapSrv() const {
	return mhEquirectangularMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::TemporaryEquirectangularMapSrv() const {
	return mhTemporaryEquirectangularMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::EnvironmentCubeMapSrv() const {
	return mhEnvironmentCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::DiffuseIrradianceCubeMapSrv() const {
	return mhDiffuseIrradianceCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::PrefilteredEnvironmentCubeMapSrv() const {
	return mhPrefilteredEnvironmentCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::IntegratedBrdfMapSrv() const {
	return mhIntegratedBrdfMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::DiffuseIrradianceEquirectMapSrv() const {
	return mhDiffuseIrradianceEquirectMapGpuSrv;
}

#endif // __IRRADIANCEMAP_INL__