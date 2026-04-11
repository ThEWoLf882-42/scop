#pragma once

#include <string>

#include "Mesh.hpp"

namespace scop
{

	class ObjLoader
	{
	public:
		static MeshData loadFromFile(const std::string &path);
	};

} // namespace scop
