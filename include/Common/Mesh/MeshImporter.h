#pragma once

#include "Mesh.h"

class MeshImporter {
public:
	static BOOL LoadObj(const std::string& file, Mesh& mesh, Material& mat);
	static BOOL LoadFbx(const std::string& file);
};