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
	DirectX::XMFLOAT4X4	ViewProj;
	DirectX::XMFLOAT4X4 ShadowTransform0;
	DirectX::XMFLOAT4X4 ShadowTransform1;
	DirectX::XMFLOAT4X4 ShadowTransform2;
	DirectX::XMFLOAT4X4 ShadowTransform3;
	DirectX::XMFLOAT4X4 ShadowTransform4;
	DirectX::XMFLOAT4X4 ShadowTransform5;

	DirectX::XMFLOAT3	LightColor;
	FLOAT				FalloffStart;	// point/spot light only

	DirectX::XMFLOAT3	Direction;		// directional/spot light only
	FLOAT				FalloffEnd;		// point/spot light only

	DirectX::XMFLOAT3	Position;		// point/spot light only
	FLOAT				Intensity;

	UINT				Type;
	FLOAT				SpotPower;		// spot light only
	FLOAT				ConstantPad0;
	FLOAT				ConstantPad1;
};