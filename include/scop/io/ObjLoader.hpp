#pragma once
#include <string>
#include <vector>
#include "scop/vk/Buffer.hpp"

namespace scop::io
{

	struct Material
	{
		std::string name;

		float Kd[3] = {1.f, 1.f, 1.f}; // diffuse
		float Ks[3] = {0.f, 0.f, 0.f}; // specular
		float Ns = 32.f;			   // shininess
		float d = 1.f;				   // alpha

		// resolved texture path (optional)
		std::string mapKd;
	};

	struct MeshData
	{
		std::vector<scop::vk::Vertex> vertices;
		std::vector<uint32_t> indices;

		Material material;
	};

	MeshData loadObj(const std::string &objPath, bool triangulate);

} // namespace scop::io
