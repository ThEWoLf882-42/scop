#pragma once

#include <cstdint>
#include <vector>

#include "Math.hpp"

namespace scop
{

	struct Vertex
	{
		Vec3 position;
		Vec3 color;
		Vec2 uv;
		Vec3 normal;
	};

	struct Bounds
	{
		Vec3 min;
		Vec3 max;
	};

	struct MeshData
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		Bounds bounds;
		bool hasSourceTexcoords;
		bool usedGeneratedTexcoords;
	};

} // namespace scop
