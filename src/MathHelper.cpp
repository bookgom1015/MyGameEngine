#include "MathHelper.h"

#include <sstream>

using namespace DirectX;

FLOAT MathHelper::AngleFromXY(FLOAT x, FLOAT y) {
	FLOAT theta = 0.0f;

	// Quadrant I or IV
	if (x >= 0.0f) {
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if (theta < 0.0f)
			theta += 2.0f * Pi; // in [0, 2*pi).
	}
	// Quadrant II or III
	else {
		theta = atanf(y / x) + Pi; // in [0, 2*pi).
	}

	return theta;
}

XMVECTOR MathHelper::SphericalToCartesian(FLOAT radius, FLOAT theta, FLOAT phi) {
	return XMVectorSet(
		radius * sinf(phi) * cosf(theta),
		radius * cosf(phi),
		radius * sinf(phi) * sinf(theta),
		1.0f);
}

XMMATRIX MathHelper::InverseTranspose(CXMMATRIX M) {
	// Inverse-transpose is just applied to normals.  So zero out 
	// translation row so that it doesn't get into our inverse-transpose
	// calculation--we don't want the inverse-transpose of the translation.
	XMMATRIX A = M;
	A.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	XMVECTOR det = XMMatrixDeterminant(A);

	return XMMatrixTranspose(XMMatrixInverse(&det, A));
}

XMFLOAT4X4 MathHelper::Identity4x4() {
	static XMFLOAT4X4 I(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	return I;
}

XMVECTOR MathHelper::RandUnitVec3() {
	XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while (true) {
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if (XMVector3Greater(XMVector3LengthSq(v), One))
			continue;

		return XMVector3Normalize(v);
	}
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n) {
	XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while (true) {
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if (XMVector3Greater(XMVector3LengthSq(v), One))
			continue;

		// Ignore points in the bottom hemisphere.
		if (XMVector3Less(XMVector3Dot(n, v), Zero))
			continue;

		return XMVector3Normalize(v);
	}
}

std::string MathHelper::to_string(DirectX::XMFLOAT4 vec) {
	std::stringstream sstream;

	sstream << "[ " << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << " ]";

	return sstream.str();
}

std::string MathHelper::to_string(DirectX::XMFLOAT4X4 mat) {
	std::stringstream sstream;

	sstream << "| " << mat.m[0][0] << ' ' << mat.m[0][1] << ' ' << mat.m[0][2] << ' ' << mat.m[0][3] << " |" << std::endl;
	sstream << "| " << mat.m[1][0] << ' ' << mat.m[1][1] << ' ' << mat.m[1][2] << ' ' << mat.m[1][3] << " |" << std::endl;
	sstream << "| " << mat.m[2][0] << ' ' << mat.m[2][1] << ' ' << mat.m[2][2] << ' ' << mat.m[2][3] << " |" << std::endl;
	sstream << "| " << mat.m[3][0] << ' ' << mat.m[3][1] << ' ' << mat.m[3][2] << ' ' << mat.m[3][3] << " |";

	return sstream.str();
}