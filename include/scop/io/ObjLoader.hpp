#pragma once
#include <string>
#include <vector>
#include "scop/vk/Buffer.hpp"

namespace scop::io
{

	struct MeshData
	{
		std::vector<scop::vk::Vertex> vertices;
		std::vector<uint32_t> indices;

		// resolved full/relative path to diffuse texture from MTL (map_Kd)
		std::string diffusePath;
	};

	MeshData loadObj(const std::string &objPath, bool triangulate);

} // namespace scop::io
