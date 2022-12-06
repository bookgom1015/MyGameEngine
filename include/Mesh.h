#pragma once

#include <vector>
#include <unordered_map>

#include "Vertex.h"

struct Mesh {
	std::unordered_map<Vertex, std::uint32_t>	UniqueVertices;
	std::vector<Vertex>							Vertices;
	std::vector<std::uint32_t>					Indices;
};

struct Material {
	std::string Name;
	std::string DiffuseMapFileName;
	std::string NormalMapFileName;
	std::string AlphaMapFileName;

	DirectX::XMFLOAT4X4 MatTransform	= MathHelper::Identity4x4();
	DirectX::XMFLOAT4 DiffuseAlbedo		= { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0			= { 0.5f, 0.5f, 0.5f };
	float Roughness						= 0.5f;
};