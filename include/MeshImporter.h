#pragma once

#include "Mesh.h"

class MeshImporter {
public:
	static bool LoadObj(const std::string& file, Mesh& mesh, Material& mat);
	static bool LoadFbx(const std::string& file);
};