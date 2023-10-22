#ifndef __RENDERER_INL__
#define __RENDERER_INL__

constexpr bool Renderer::DebuggingEnabled() const {
	return bDebuggingEnabled;
}

constexpr bool Renderer::ShadowEnabled() const {
	return bShadowEnabled;
}

constexpr bool Renderer::SsaoEnabled() const {
	return bSsaoEnabled;
}

constexpr bool Renderer::TaaEnabled() const {
	return bTaaEnabled;
}

constexpr bool Renderer::MotionBlurEnabled() const {
	return bMotionBlurEnabled;
}

constexpr bool Renderer::DepthOfFieldEnabled() const {
	return bDepthOfFieldEnabled;
}

constexpr bool Renderer::BloomEnabled() const {
	return bBloomEnabled;
}

constexpr bool Renderer::SsrEnabled() const {
	return bSsrEnabled;
}

constexpr bool Renderer::GammaCorrectionEnabled() const {
	return bGammaCorrectionEnabled;
}

constexpr bool Renderer::ToneMappingEnabled() const {
	return bToneMappingEnabled;
}

constexpr bool Renderer::PixelationEnabled() const {
	return bPixelationEnabled;
}

constexpr bool Renderer::SharpenEnabled() const {
	return bSharpenEnabled;
}

constexpr bool Renderer::RaytracingEnabled() const {
	return bRaytracing;
}

constexpr bool Renderer::IsInitialized() const {
	return bInitialized;
}

constexpr float Renderer::AspectRatio() const {
	return static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight);
}

#endif // __RENDERER_INL__