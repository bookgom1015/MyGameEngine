#pragma once

#include <cmath>
#include <string>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <Windows.h>

namespace UnitVector {
	const DirectX::XMVECTOR RightVector		= DirectX::XMVectorSet( 1.0f,  0.0f,  0.0f, 0.0f);
	const DirectX::XMVECTOR UpVector		= DirectX::XMVectorSet( 0.0f,  1.0f,  0.0f, 0.0f);
	const DirectX::XMVECTOR ForwardVector	= DirectX::XMVectorSet( 0.0f,  0.0f,  1.0f, 0.0f);
	const DirectX::XMVECTOR LeftVector		= DirectX::XMVectorSet(-1.0f,  0.0f,  0.0f, 0.0f);
	const DirectX::XMVECTOR DownVector		= DirectX::XMVectorSet( 0.0f, -1.0f,  0.0f, 0.0f);
	const DirectX::XMVECTOR BackwardVector	= DirectX::XMVectorSet( 0.0f,  0.0f, -1.0f, 0.0f);
}

namespace MathHelper {
	const FLOAT Infinity = FLT_MAX;
	const FLOAT Pi = 3.1415926535f;
	const FLOAT Epsilon = 0.0000000001f;
	const FLOAT RadToDeg = 180.0f / 3.1415926535f;
	const FLOAT DegToRad = 3.1415926535f / 180.0f;

	__forceinline FLOAT Sin(FLOAT t);
	__forceinline FLOAT ASin(FLOAT t);
	__forceinline FLOAT Cos(FLOAT t);
	__forceinline FLOAT ACos(FLOAT t);
	__forceinline FLOAT Tan(FLOAT t);
	__forceinline FLOAT ATan2(FLOAT x, FLOAT y);

	__forceinline constexpr FLOAT DegreesToRadians(FLOAT degrees);
	__forceinline constexpr FLOAT RadiansToDegrees(FLOAT radians);

	// Returns random FLOAT in [0, 1).
	__forceinline FLOAT RandF();
	// Returns random FLOAT in [a, b).
	__forceinline FLOAT RandF(FLOAT a, FLOAT b);
	__forceinline int Rand(INT a, INT b);

	template<typename T>
	T Min(const T& a, const T& b);
	template<typename T>
	T Max(const T& a, const T& b);
	template<typename T>
	T Lerp(const T& a, const T& b, FLOAT t);
	template<typename T>
	T Clamp(const T& x, const T& low, const T& high);

	__forceinline FLOAT Abs(FLOAT param);

	__forceinline constexpr BOOL IsZero(FLOAT value);
	__forceinline constexpr BOOL IsNotZero(FLOAT value);

	__forceinline BOOL IsEqual(FLOAT a, FLOAT b);
	__forceinline BOOL IsNotEqual(FLOAT a, FLOAT b);
	__forceinline BOOL IsEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs);
	__forceinline BOOL IsNotEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs);
	__forceinline BOOL IsEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs);
	__forceinline BOOL IsNotEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs);
	__forceinline BOOL IsEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs);
	__forceinline BOOL IsNotEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs);

	__forceinline DirectX::PackedVector::XMFLOAT3PK PackXMFLOAT3(const DirectX::XMFLOAT3& v);

	__forceinline DirectX::XMVECTOR CalcUpVector(const DirectX::XMFLOAT3& dir);
	__forceinline DirectX::XMVECTOR CalcUpVector(const DirectX::XMVECTOR& dir);
	__forceinline void CalcUpVector(DirectX::XMFLOAT3& dst, const DirectX::XMFLOAT3& dir);
	__forceinline void CalcUpVector(DirectX::XMFLOAT3& dst, const DirectX::XMVECTOR& dir);

	// Returns the polar angle of the point (x,y) in [0, 2*PI).
	FLOAT AngleFromXY(FLOAT x, FLOAT y);

	DirectX::XMVECTOR SphericalToCartesian(FLOAT radius, FLOAT theta, FLOAT phi);
	DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M);
	DirectX::XMFLOAT4X4 Identity4x4();
	DirectX::XMVECTOR RandUnitVec3();
	DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

	std::string to_string(DirectX::XMFLOAT2 vec);
	std::string to_string(DirectX::XMFLOAT3 vec);
	std::string to_string(DirectX::XMFLOAT4 vec);
	std::string to_string(DirectX::XMFLOAT4X4 mat);
};

#include "MathHelper.inl"