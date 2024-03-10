#pragma once

#ifndef HLSL
	#include <Windows.h>
	#include <DirectXMath.h>
#endif

#define MaxLights 8

namespace LightType {
	enum {
		E_None			= 0,
		E_Directional	= 1,
		E_Point			= 2,
		E_Spot			= 3
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
	FLOAT				ConstantPads0;
	FLOAT				ConstantPads1;
};