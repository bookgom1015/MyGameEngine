#pragma once

#ifndef HLSL
	#include <Windows.h>
	#include <DirectXMath.h>
#endif

#define MaxLights 16

namespace LightType {
	enum {
		E_None = 0,
		E_Directional,
		E_Point,
		E_Spot
	};
}

struct Light {
	DirectX::XMFLOAT3	LightColor;
	FLOAT				FalloffStart;	// point/spot light only

	DirectX::XMFLOAT3	Direction;		// directional/spot light only
	FLOAT				FalloffEnd;		// point/spot light only

	DirectX::XMFLOAT3	Position;		// point/spot light only
	FLOAT				SpotPower;		// spot light only

	UINT				Type;
	FLOAT				Intensity;
	UINT				ConstantPads[2];
};