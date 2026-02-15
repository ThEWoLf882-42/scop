#include "scop/io/ObjLoader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{

	struct Vec3
	{
		float x, y, z;
	};

	static Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	static Vec3 cross(const Vec3 &a, const Vec3 &b)
	{
		return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}
	static float len(const Vec3 &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
	static Vec3 norm(const Vec3 &v)
	{
		float l = len(v);
		if (l <= 0.0f)
			return {0.f, 0.f, 1.f};
		return {v.x / l, v.y / l, v.z / l};
	}

	static int fixIndex(int idx, int size)
	{
		if (idx > 0)
			return idx - 1;
		if (idx < 0)
			return size + idx;
		return -1;
	}

	struct Ref
	{
		int v = -1;
		int vt = -1;
		int vn = -1;
	};

	static Ref parseRef(const std::string &tok, int vCount, int vtCount, int vnCount)
	{
		Ref r{};
		int a = 0, b = 0, c = 0;
		bool hasA = false, hasB = false, hasC = false;

		size_t s1 = tok.find('/');
		if (s1 == std::string::npos)
		{
			a = std::stoi(tok);
			hasA = true;
		}
		else
		{
			std::string p0 = tok.substr(0, s1);
			if (!p0.empty())
			{
				a = std::stoi(p0);
				hasA = true;
			}

			size_t s2 = tok.find('/', s1 + 1);
			if (s2 == std::string::npos)
			{
				std::string p1 = tok.substr(s1 + 1);
				if (!p1.empty())
				{
					b = std::stoi(p1);
					hasB = true;
				}
			}
			else
			{
				std::string p1 = tok.substr(s1 + 1, s2 - (s1 + 1));
				std::string p2 = tok.substr(s2 + 1);
				if (!p1.empty())
				{
					b = std::stoi(p1);
					hasB = true;
				}
				if (!p2.empty())
				{
					c = std::stoi(p2);
					hasC = true;
				}
			}
		}

		if (hasA)
			r.v = fixIndex(a, vCount);
		if (hasB)
			r.vt = fixIndex(b, vtCount);
		if (hasC)
			r.vn = fixIndex(c, vnCount);

		return r;
	}

	struct Key
	{
		int v, vt, vn;
		bool operator==(const Key &o) const { return v == o.v && vt == o.vt && vn == o.vn; }
	};

	struct KeyHash
	{
		std::size_t operator()(const Key &k) const noexcept
		{
			std::size_t h1 = std::hash<int>{}(k.v);
			std::size_t h2 = std::hash<int>{}(k.vt);
			std::size_t h3 = std::hash<int>{}(k.vn);
			std::size_t h = h1;
			h ^= (h2 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
			h ^= (h3 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
			return h;
		}
	};

}

namespace scop::io
{

	MeshData loadObj(const std::string &path, bool normalizeToUnit)
	{
		std::ifstream f(path.c_str());
		if (!f.is_open())
			throw std::runtime_error("OBJ: failed to open: " + path);

		std::vector<Vec3> positions;
		std::vector<Vec3> normals;

		MeshData out;

		std::unordered_map<Key, uint32_t, KeyHash> dedup;

		std::string line;
		positions.reserve(4096);
		normals.reserve(4096);
		out.vertices.reserve(4096);
		out.indices.reserve(8192);

		while (std::getline(f, line))
		{
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream iss(line);
			std::string tag;
			iss >> tag;

			if (tag == "v")
			{
				Vec3 p{};
				iss >> p.x >> p.y >> p.z;
				positions.push_back(p);
			}
			else if (tag == "vn")
			{
				Vec3 n{};
				iss >> n.x >> n.y >> n.z;
				normals.push_back(norm(n));
			}
			else if (tag == "f")
			{
				std::vector<std::string> toks;
				std::string t;
				while (iss >> t)
					toks.push_back(t);
				if (toks.size() < 3)
					continue;

				std::vector<Ref> refs;
				refs.reserve(toks.size());
				for (size_t i = 0; i < toks.size(); ++i)
				{
					refs.push_back(parseRef(
						toks[i],
						static_cast<int>(positions.size()),
						0,
						static_cast<int>(normals.size())));
				}

				Vec3 faceN{0.f, 0.f, 1.f};
				bool anyVN = false;
				for (const auto &r : refs)
					if (r.vn >= 0)
					{
						anyVN = true;
						break;
					}
				if (!anyVN)
				{
					const Vec3 p0 = positions[static_cast<size_t>(refs[0].v)];
					const Vec3 p1 = positions[static_cast<size_t>(refs[1].v)];
					const Vec3 p2 = positions[static_cast<size_t>(refs[2].v)];
					faceN = norm(cross(sub(p1, p0), sub(p2, p0)));
				}

				auto emitIndex = [&](const Ref &r) -> uint32_t
				{
					const Vec3 p = positions[static_cast<size_t>(r.v)];
					Vec3 n = faceN;
					if (r.vn >= 0)
						n = normals[static_cast<size_t>(r.vn)];

					if (r.vn >= 0)
					{
						Key k{r.v, r.vt, r.vn};
						auto it = dedup.find(k);
						if (it != dedup.end())
							return it->second;

						scop::vk::Vertex v{};
						v.pos[0] = p.x;
						v.pos[1] = p.y;
						v.pos[2] = p.z;
						v.normal[0] = n.x;
						v.normal[1] = n.y;
						v.normal[2] = n.z;

						uint32_t newIndex = static_cast<uint32_t>(out.vertices.size());
						out.vertices.push_back(v);
						dedup.emplace(k, newIndex);
						return newIndex;
					}
					else
					{
						scop::vk::Vertex v{};
						v.pos[0] = p.x;
						v.pos[1] = p.y;
						v.pos[2] = p.z;
						v.normal[0] = n.x;
						v.normal[1] = n.y;
						v.normal[2] = n.z;

						uint32_t newIndex = static_cast<uint32_t>(out.vertices.size());
						out.vertices.push_back(v);
						return newIndex;
					}
				};

				const uint32_t i0 = emitIndex(refs[0]);
				for (size_t i = 1; i + 1 < refs.size(); ++i)
				{
					uint32_t i1 = emitIndex(refs[i]);
					uint32_t i2 = emitIndex(refs[i + 1]);
					out.indices.push_back(i0);
					out.indices.push_back(i1);
					out.indices.push_back(i2);
				}
			}
		}

		if (normalizeToUnit && !out.vertices.empty())
		{
			float minX = std::numeric_limits<float>::max();
			float minY = std::numeric_limits<float>::max();
			float minZ = std::numeric_limits<float>::max();
			float maxX = -std::numeric_limits<float>::max();
			float maxY = -std::numeric_limits<float>::max();
			float maxZ = -std::numeric_limits<float>::max();

			for (const auto &v : out.vertices)
			{
				minX = std::min(minX, v.pos[0]);
				maxX = std::max(maxX, v.pos[0]);
				minY = std::min(minY, v.pos[1]);
				maxY = std::max(maxY, v.pos[1]);
				minZ = std::min(minZ, v.pos[2]);
				maxZ = std::max(maxZ, v.pos[2]);
			}

			const float cx = (minX + maxX) * 0.5f;
			const float cy = (minY + maxY) * 0.5f;
			const float cz = (minZ + maxZ) * 0.5f;

			const float sx = (maxX - minX);
			const float sy = (maxY - minY);
			const float sz = (maxZ - minZ);
			const float s = std::max(sx, std::max(sy, sz));
			const float inv = (s > 0.f) ? (1.0f / s) : 1.0f;

			for (auto &v : out.vertices)
			{
				v.pos[0] = (v.pos[0] - cx) * inv;
				v.pos[1] = (v.pos[1] - cy) * inv;
				v.pos[2] = (v.pos[2] - cz) * inv;
			}
		}

		return out;
	}

}
