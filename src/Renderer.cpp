#include "Renderer.h"

void Renderer::SetCamera(Camera* cam) {
	mCamera = cam;
}

void Renderer::EnableDebugging(bool state) {
	bDebuggingEnabled = state;
}

void Renderer::EnableShadow(bool state) {
	bShadowEnabled = state;
}

void Renderer::EnableSsao(bool state) {
	bSsaoEnabled = state;
}