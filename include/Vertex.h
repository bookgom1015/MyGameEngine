#pragma once

#include <array>
#include <vulkan/vulkan.h>

#include "MathHelper.h"

struct Vertex {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();

	bool operator==(const Vertex& other) const;
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			size_t pos = static_cast<size_t>(vertex.Position.x + vertex.Position.y + vertex.Position.z);
			size_t normal = static_cast<size_t>(vertex.Normal.x + vertex.Normal.y + vertex.Normal.z);
			size_t texc = static_cast<size_t>(vertex.TexCoord.x + vertex.TexCoord.y);
			return (pos ^ normal ^ texc);
		}
	};
}