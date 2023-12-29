#ifndef __SVGF_INL__
#define __SVGF_INL__

const SVGF::LocalMeanVarianceResourcesType& SVGF::SVGFClass::LocalMeanVarianceResources() const {
	return mLocalMeanVarianceResources;
}
const SVGF::LocalMeanVarianceResourcesGpuDescriptors& SVGF::SVGFClass::LocalMeanVarianceResourcesGpuDescriptors() const {
	return mhLocalMeanVarianceResourcesGpus;
}

const SVGF::VarianceResourcesType& SVGF::SVGFClass::VarianceResources() const {
	return mVarianceResources;
}
const SVGF::VarianceResourcesGpuDescriptors& SVGF::SVGFClass::VarianceResourcesGpuDescriptors() const {
	return mhVarianceResourcesGpus;
}

GpuResource* SVGF::SVGFClass::DepthPartialDerivativeMapResource() {
	return mDepthPartialDerivative.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::DepthPartialDerivativeSrv() const {
	return mhDepthPartialDerivativeGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::DepthPartialDerivativeUav() const {
	return mhDepthPartialDerivativeGpuUav;
}

GpuResource* SVGF::SVGFClass::DisocclusionBlurStrengthResource() {
	return mDisocclusionBlurStrength.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::DisocclusionBlurStrengthSrv() const {
	return mhDisocclusionBlurStrengthGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::DisocclusionBlurStrengthUav() const {
	return mhDisocclusionBlurStrengthGpuUav;
}

GpuResource* SVGF::SVGFClass::TsppValueSquaredMeanRayHitDistance() {
	return mTsppValueSquaredMeanRayHitDistance.get();
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::TsppValueSquaredMeanRayHitDistanceSrv() const {
	return mhTsppValueSquaredMeanRayHitDistanceGpuSrv;
}
constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SVGF::SVGFClass::TsppValueSquaredMeanRayHitDistanceUav() const {
	return mhTsppValueSquaredMeanRayHitDistanceGpuUav;
}

#endif // __SVGF_INL__