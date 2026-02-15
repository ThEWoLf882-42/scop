#pragma once

#include <string>
#include <vector>

#include "scop/vk/Vertex.hpp"

namespace scop::io
{

	struct MeshData
	{
		std::vector<scop::vk::Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	MeshData loadObj(const std::string &path, bool normalizeToUnit = true);

}
