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
	DirectX::XMFLOAT4X4 Mat0;
	DirectX::XMFLOAT4X4 Mat1;
	DirectX::XMFLOAT4X4 Mat2;
	DirectX::XMFLOAT4X4 Mat3;
	DirectX::XMFLOAT4X4 Mat4;
	DirectX::XMFLOAT4X4 Mat5;

	DirectX::XMFLOAT3	LightColor;
	FLOAT				Intensity;

	DirectX::XMFLOAT3	Direction;			// directional/spot light only
	FLOAT				ConstantPad0;

	DirectX::XMFLOAT3	Position;			// point/spot light only
	FLOAT				LightRadius;

	UINT				Type;
	FLOAT				InnerConeAngle;		// spot light only (degrees)
	FLOAT				OuterConeAngle;		// spot light only (degrees)
	FLOAT				AttenuationRadius;	// point/spot light only
};