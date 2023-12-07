#ifndef __RENDERER_INL__
#define __RENDERER_INL__

constexpr BOOL Renderer::DebuggingEnabled() const {
	return bDebuggingEnabled;
}

constexpr BOOL Renderer::ShadowEnabled() const {
	return bShadowEnabled;
}

constexpr BOOL Renderer::SsaoEnabled() const {
	return bSsaoEnabled;
}

constexpr BOOL Renderer::TaaEnabled() const {
	return bTaaEnabled;
}

constexpr BOOL Renderer::MotionBlurEnabled() const {
	return bMotionBlurEnabled;
}

constexpr BOOL Renderer::DepthOfFieldEnabled() const {
	return bDepthOfFieldEnabled;
}

constexpr BOOL Renderer::BloomEnabled() const {
	return bBloomEnabled;
}

constexpr BOOL Renderer::SsrEnabled() const {
	return bSsrEnabled;
}

constexpr BOOL Renderer::GammaCorrectionEnabled() const {
	return bGammaCorrectionEnabled;
}

constexpr BOOL Renderer::ToneMappingEnabled() const {
	return bToneMappingEnabled;
}

constexpr BOOL Renderer::PixelationEnabled() const {
	return bPixelationEnabled;
}

constexpr BOOL Renderer::SharpenEnabled() const {
	return bSharpenEnabled;
}

constexpr BOOL Renderer::RaytracingEnabled() const {
	return bRaytracing;
}

constexpr BOOL Renderer::IsInitialized() const {
	return bInitialized;
}

constexpr FLOAT Renderer::AspectRatio() const {
	return static_cast<FLOAT>(mClientWidth) / static_cast<FLOAT>(mClientHeight);
}

#endif // __RENDERER_INL__