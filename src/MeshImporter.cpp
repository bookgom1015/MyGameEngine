#include "MeshImporter.h"
#include "Logger.h"
#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace DirectX;

namespace {
	std::string BaseDir = "./../../assets/meshes/";
}

bool MeshImporter::LoadObj(const std::string& file, Mesh& mesh, Material& mat) {
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

			int first = 3 * index.vertex_index + 0;
			int second = 3 * index.vertex_index + 1;
			int third = 3 * index.vertex_index + 2;

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

			float texY = attrib.texcoords[2 * index.texcoord_index + 1];
			vertex.TexCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				texY,
			};
			

			if (mesh.UniqueVertices.count(vertex) == 0) {
				mesh.UniqueVertices[vertex] = static_cast<std::uint32_t>(mesh.Vertices.size());
				mesh.Vertices.push_back(vertex);
			}

			mesh.Indices.push_back(static_cast<std::uint32_t>(mesh.UniqueVertices[vertex]));
		}
	}
	
	for (const auto& material : materials) {
		mat.Name = material.name;
		mat.DiffuseMapFileName = material.diffuse_texname;
		mat.DiffuseAlbedo = XMFLOAT4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.0f);
	}

	return true;
}

bool MeshImporter::LoadFbx(const std::string& file) {


	return true;
}