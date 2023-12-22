#ifndef __RTAO_INL__
#define __RTAO_INL__

const Rtao::AOResourcesType& Rtao::RtaoClass::AOResources() const {
	return mAOResources;
}
const Rtao::AOResourcesGpuDescriptors& Rtao::RtaoClass::AOResourcesGpuDescriptors() const {
	return mhAOResourcesGpus;
}

const Rtao::TemporalCachesType& Rtao::RtaoClass::TemporalCaches() const {
	return mTemporalCaches;
}
const Rtao::TemporalCachesGpuDescriptors& Rtao::RtaoClass::TemporalCachesGpuDescriptors() const {
	return mhTemporalCachesGpus;
}

const Rtao::LocalMeanVarianceResourcesType& Rtao::RtaoClass::LocalMeanVarianceResources() const {
	return mLocalMeanVarianceResources;
}
const Rtao::LocalMeanVarianceResourcesGpuDescriptors& Rtao::RtaoClass::LocalMeanVarianceResourcesGpuDescriptors() const {
	return mhLocalMeanVarianceResourcesGpus;
}

const Rtao::AOVarianceResourcesType& Rtao::RtaoClass::AOVarianceResources() const {
	return mAOVarianceResources;
}
const Rtao::AOVarianceResourcesGpuDescriptors& Rtao::RtaoClass::AOVarianceResourcesGpuDescriptors() const {
	return mhAOVarianceResourcesGpus;
}

GpuResource* Rtao::RtaoClass::TsppCoefficientSquaredMeanRayHitDistance() {
	return mTsppCoefficientSquaredMeanRayHitDistance.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::TsppCoefficientSquaredMeanRayHitDistanceSrv() const {
	return mhTsppCoefficientSquaredMeanRayHitDistanceGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::TsppCoefficientSquaredMeanRayHitDistanceUav() const {
	return mhTsppCoefficientSquaredMeanRayHitDistanceGpuUav;
}

GpuResource* Rtao::RtaoClass::DisocclusionBlurStrengthResource() {
	return mDisocclusionBlurStrength.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::DisocclusionBlurStrengthSrv() const {
	return mhDisocclusionBlurStrengthGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::DisocclusionBlurStrengthUav() const {
	return mhDisocclusionBlurStrengthGpuUav;
}

const Rtao::TemporalAOCoefficientsType& Rtao::RtaoClass::TemporalAOCoefficients() {
	return mTemporalAOCoefficients;
}
const Rtao::TemporalAOCoefficientsGpuDescriptors& Rtao::RtaoClass::TemporalAOCoefficientsGpuDescriptors() const {
	return mhTemporalAOCoefficientsGpus;
}

GpuResource* Rtao::RtaoClass::DepthPartialDerivativeMapResource() {
	return mDepthPartialDerivative.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::DepthPartialDerivativeSrv() const {
	return mhDepthPartialDerivativeGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::DepthPartialDerivativeUav() const {
	return mhDepthPartialDerivativeGpuUav;
}

constexpr UINT Rtao::RtaoClass::TemporalCurrentFrameResourceIndex() const {
	return mTemporalCurrentFrameResourceIndex;
}

constexpr UINT Rtao::RtaoClass::TemporalCurrentFrameTemporalAOCoefficientResourceIndex() const {
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::ResolvedAOCoefficientSrv() const {
	return mhTemporalAOCoefficientsGpus[mTemporalCurrentFrameTemporalAOCeofficientResourceIndex][Rtao::Descriptor::TemporalAOCoefficient::Srv];
}

#endif // __RTAO_INL__