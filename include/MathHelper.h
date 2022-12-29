#pragma once

#include <cmath>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <string>

namespace UnitVectors {
	const DirectX::XMVECTOR RightVector = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const DirectX::XMVECTOR UpVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const DirectX::XMVECTOR ForwardVector = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
}

namespace MathHelper {
	const float Infinity = FLT_MAX;
	const float Pi = 3.1415926535f;
	const float Epsilon = 0.0000000001f;
	const float RadToDeg = 180.0f / 3.1415926535f;
	const float DegToRad = 3.1415926535f / 180.0f;

	__forceinline float Sin(float t);
	__forceinline float ASin(float t);
	__forceinline float Cos(float t);
	__forceinline float ACos(float t);
	__forceinline float Tan(float t);
	__forceinline float ATan2(float x, float y);

	__forceinline constexpr float DegreesToRadians(float degrees);
	__forceinline constexpr float RadiansToDegrees(float radians);

	// Returns random float in [0, 1).
	__forceinline float RandF();
	// Returns random float in [a, b).
	__forceinline float RandF(float a, float b);
	__forceinline int Rand(int a, int b);

	template<typename T>
	T Min(const T& a, const T& b);
	template<typename T>
	T Max(const T& a, const T& b);
	template<typename T>
	T Lerp(const T& a, const T& b, float t);
	template<typename T>
	T Clamp(const T& x, const T& low, const T& high);

	__forceinline float Abs(float param);

	__forceinline constexpr bool IsZero(float value);
	__forceinline constexpr bool IsNotZero(float value);

	__forceinline bool IsEqual(float a, float b);
	__forceinline bool IsNotEqual(float a, float b);
	__forceinline bool IsEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs);
	__forceinline bool IsNotEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs);
	__forceinline bool IsEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs);
	__forceinline bool IsNotEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs);
	__forceinline bool IsEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs);
	__forceinline bool IsNotEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs);

	// Returns the polar angle of the point (x,y) in [0, 2*PI).
	float AngleFromXY(float x, float y);

	DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi);
	DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M);
	DirectX::XMFLOAT4X4 Identity4x4();
	DirectX::XMVECTOR RandUnitVec3();
	DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

	std::string to_string(DirectX::XMFLOAT4 vec);
	std::string to_string(DirectX::XMFLOAT4X4 mat);
};

#include "MathHelper.inl"