#include "Renderer.h"

void Renderer::SetCamera(Camera* cam) {
	mCamera = cam;
}

void Renderer::EnableShadow(bool state) {
	bShadowEnabled = state;
}