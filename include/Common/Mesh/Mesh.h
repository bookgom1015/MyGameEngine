#pragma once

#include <vector>
#include <unordered_map>

#include "Common/Helper/MathHelper.h"
#include "Vertex.h"

struct Mesh {
	std::unordered_map<Vertex, UINT>	UniqueVertices;
	std::vector<Vertex>					Vertices;
	std::vector<UINT>					Indices;
};

struct Material {
	std::string Name;
	std::string DiffuseMapFileName;
	std::string NormalMapFileName;
	std::string AlphaMapFileName;

	DirectX::XMFLOAT4X4 MatTransform	= MathHelper::Identity4x4();
	DirectX::XMFLOAT4 Albedo			= { 1.f, 1.f, 1.f, 1.f };
	FLOAT Specular						= 0.5f;
	FLOAT Roughness						= 0.5f;
};