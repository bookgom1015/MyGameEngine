#include "Common/Mesh/MeshImporter.h"
#include "Common/Debug/Logger.h"
#include "Common/Mesh/Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <DirectXPackedVector.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::string BaseDir = "./../../assets/meshes/";
}

BOOL MeshImporter::LoadObj(const std::string& file, Mesh& mesh, Material& mat) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	{
		std::stringstream sstream;
		sstream << BaseDir << file;
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, sstream.str().c_str(), BaseDir.c_str())) {
			std::wstringstream wsstream;
			wsstream << warn.c_str() << err.c_str();
			ReturnFalse(wsstream.str());
		}
	}

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex = {};

			vertex.Position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.Normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			const FLOAT texY = attrib.texcoords[2 * index.texcoord_index + 1];
			vertex.TexCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				texY,
			};
			

			if (mesh.UniqueVertices.count(vertex) == 0) {
				mesh.UniqueVertices[vertex] = static_cast<UINT>(mesh.Vertices.size());
				mesh.Vertices.push_back(vertex);
			}

			mesh.Indices.push_back(static_cast<UINT>(mesh.UniqueVertices[vertex]));
		}
	}
	
	for (const auto& material : materials) {
		mat.Name = material.name;
		mat.DiffuseMapFileName = material.diffuse_texname;
		mat.Albedo = XMFLOAT4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.f);
		mat.Roughness = material.roughness;
		mat.Specular = material.specular[0];
	}

	return TRUE;
}

BOOL MeshImporter::LoadFbx(const std::string& file) { return TRUE; }