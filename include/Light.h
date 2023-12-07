#pragma once

#include <DirectXMath.h>
#include <Windows.h>

#define MaxLights 16

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	FLOAT FalloffStart = 1.0f;								// point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };	// directional/spot light only
	FLOAT FalloffEnd = 10.0f;								// point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };		// point/spot light only
	FLOAT SpotPower = 64.0f;								// spot light only
};