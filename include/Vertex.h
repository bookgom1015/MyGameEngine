#pragma once

#ifndef HLSL
	#include <array>
	#include <vector>
	#include <d3d12.h>

	#ifdef _Vulkan
		#include <vulkan/vulkan.h>
	#endif
	
	#include "MathHelper.h"
	#include "HashUtil.h"
#endif

struct Vertex {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;

#ifndef HLSL
public:
#ifdef _Vulkan
	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
#endif

	static D3D12_INPUT_LAYOUT_DESC InputLayoutDesc();

	BOOL operator==(const Vertex& other) const;

private:
	static const std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	static const D3D12_INPUT_LAYOUT_DESC mInputLayoutDesc;
#endif
};

#ifndef HLSL
	namespace std {
		template<> struct hash<Vertex> {
			size_t operator()(const Vertex& vert) const {
				size_t pos = hu::hash_combine(0, static_cast<size_t>(vert.Position.x));
				pos = hu::hash_combine(pos, static_cast<size_t>(vert.Position.y));
				pos = hu::hash_combine(pos, static_cast<size_t>(vert.Position.z));
	
				size_t normal = hu::hash_combine(0, static_cast<size_t>(vert.Normal.x));
				normal = hu::hash_combine(normal, static_cast<size_t>(vert.Normal.y));
				normal = hu::hash_combine(normal, static_cast<size_t>(vert.Normal.z));
	
				size_t texc = hu::hash_combine(0, static_cast<size_t>(vert.TexCoord.x));
				texc = hu::hash_combine(texc, static_cast<size_t>(vert.TexCoord.y));
	
				return hu::hash_combine(hu::hash_combine(pos, normal), texc);
			}
		};
	}
#else 
	#ifndef VERTEX_IN
	#define VERTEX_IN					\
		struct VertexIn {				\
			float3 PosL		: POSITION;	\
			float3 NormalL	: NORMAL;	\
			float2 TexC		: TEXCOORD;	\
		};
	#endif 
#endif