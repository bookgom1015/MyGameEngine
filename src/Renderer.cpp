#include "Renderer.h"

#include <assert.h>

void Renderer::Pick(FLOAT x, FLOAT y) {}

void Renderer::SetCamera(Camera* cam) {
	mCamera = cam;
}

void Renderer::EnableDebugging(BOOL state) {
	bDebuggingEnabled = state;
}

void Renderer::EnableShadow(BOOL state) {
	bShadowEnabled = state;
}

void Renderer::EnableSsao(BOOL state) {
	bSsaoEnabled = state;
}

void Renderer::EnableTaa(BOOL state) {
	if (!bTaaEnabled && state) bInitiatingTaa = true;
	bTaaEnabled = state;
}

void Renderer::EnableMotionBlur(BOOL state) {
	bMotionBlurEnabled = state;
}

void Renderer::EnableDepthOfField(BOOL state) {
	bDepthOfFieldEnabled = state;
}

void Renderer::EnableBloom(BOOL state) {
	bBloomEnabled = state;
}

void Renderer::EnableSsr(BOOL state) {
	bSsrEnabled = state;
}

void Renderer::EnableGammaCorrection(BOOL state) {
	bGammaCorrectionEnabled = state;
}

void Renderer::EnableToneMapping(BOOL state) {
	bToneMappingEnabled = state;
}

void Renderer::EnablePixelation(BOOL state) {
	bPixelationEnabled = state;
}

void Renderer::EnableSharpen(BOOL state) {
	bSharpenEnabled = state;
}

void Renderer::EnableRaytracing(BOOL state) {
	bRaytracing = state;
}

void Renderer::ShowImGui(BOOL state) {
	bShowImGui = state;
}