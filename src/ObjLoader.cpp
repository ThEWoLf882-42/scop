#include "ObjLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace scop
{
	namespace
	{

		struct IndexTriplet
		{
			int v;
			int vt;
			int vn;
		};

		struct Face
		{
			std::vector<IndexTriplet> vertices;
		};

		struct RawObj
		{
			std::vector<Vec3> positions;
			std::vector<Vec2> texcoords;
			std::vector<Vec3> normals;
			std::vector<Face> faces;
			Bounds bounds;
		};

		struct Triangle
		{
			int a;
			int b;
			int c;
		};

		float hash01(std::size_t seed)
		{
			seed = (seed ^ 61U) ^ (seed >> 16U);
			seed *= 9U;
			seed = seed ^ (seed >> 4U);
			seed *= 0x27d4eb2dU;
			seed = seed ^ (seed >> 15U);
			return static_cast<float>(seed & 0xFFFFU) / 65535.0f;
		}

		Vec3 chooseFaceColor(const Vec3 &normal, std::size_t faceIndex)
		{
			static const Vec3 palette[] = {
				Vec3(0.95f, 0.35f, 0.35f),
				Vec3(0.25f, 0.75f, 0.95f),
				Vec3(0.95f, 0.80f, 0.25f),
				Vec3(0.35f, 0.90f, 0.55f),
				Vec3(0.80f, 0.45f, 0.95f),
				Vec3(0.95f, 0.55f, 0.25f)};

			if (std::fabs(normal.z) < 0.4f)
			{
				const float gray = 0.35f + 0.45f * hash01(faceIndex + 17U);
				return Vec3(gray, gray, gray);
			}

			const Vec3 base = palette[faceIndex % (sizeof(palette) / sizeof(palette[0]))];
			const float shade = 0.85f + 0.15f * hash01(faceIndex + 73U);
			return base * shade;
		}

		int resolveIndex(int index, int count)
		{
			if (index > 0)
			{
				return index - 1;
			}
			if (index < 0)
			{
				return count + index;
			}
			throw std::runtime_error("OBJ index 0 is invalid");
		}

		IndexTriplet parseFaceToken(const std::string &token, int positionCount, int texcoordCount, int normalCount)
		{
			IndexTriplet out = {-1, -1, -1};

			std::size_t firstSlash = token.find('/');
			if (firstSlash == std::string::npos)
			{
				out.v = resolveIndex(std::stoi(token), positionCount);
				return out;
			}

			std::size_t secondSlash = token.find('/', firstSlash + 1U);
			const std::string vPart = token.substr(0U, firstSlash);
			const std::string vtPart = (secondSlash == std::string::npos)
										   ? token.substr(firstSlash + 1U)
										   : token.substr(firstSlash + 1U, secondSlash - firstSlash - 1U);
			const std::string vnPart = (secondSlash == std::string::npos)
										   ? std::string()
										   : token.substr(secondSlash + 1U);

			out.v = resolveIndex(std::stoi(vPart), positionCount);
			if (!vtPart.empty())
			{
				out.vt = resolveIndex(std::stoi(vtPart), texcoordCount);
			}
			if (!vnPart.empty())
			{
				out.vn = resolveIndex(std::stoi(vnPart), normalCount);
			}
			return out;
		}

		Vec3 computeFaceNormal(const Face &face, const std::vector<Vec3> &positions)
		{
			Vec3 normal(0.0f, 0.0f, 0.0f);
			const std::size_t count = face.vertices.size();
			for (std::size_t i = 0; i < count; ++i)
			{
				const Vec3 &current = positions[face.vertices[i].v];
				const Vec3 &next = positions[face.vertices[(i + 1U) % count].v];
				normal.x += (current.y - next.y) * (current.z + next.z);
				normal.y += (current.z - next.z) * (current.x + next.x);
				normal.z += (current.x - next.x) * (current.y + next.y);
			}
			normal = normalize(normal);
			if (length(normal) <= 1e-6f && count >= 3U)
			{
				const Vec3 a = positions[face.vertices[0].v];
				const Vec3 b = positions[face.vertices[1].v];
				const Vec3 c = positions[face.vertices[2].v];
				normal = normalize(cross(b - a, c - a));
			}
			if (length(normal) <= 1e-6f)
			{
				normal = Vec3(0.0f, 0.0f, 1.0f);
			}
			return normal;
		}

		Vec2 projectPoint(const Vec3 &point, const Vec3 &normal)
		{
			const float ax = std::fabs(normal.x);
			const float ay = std::fabs(normal.y);
			const float az = std::fabs(normal.z);

			if (ax >= ay && ax >= az)
			{
				return Vec2(point.y, point.z);
			}
			if (ay >= az)
			{
				return Vec2(point.x, point.z);
			}
			return Vec2(point.x, point.y);
		}

		float signedArea(const std::vector<Vec2> &polygon)
		{
			float area = 0.0f;
			for (std::size_t i = 0; i < polygon.size(); ++i)
			{
				const Vec2 &a = polygon[i];
				const Vec2 &b = polygon[(i + 1U) % polygon.size()];
				area += a.x * b.y - b.x * a.y;
			}
			return area * 0.5f;
		}

		float cross2D(const Vec2 &a, const Vec2 &b, const Vec2 &c)
		{
			return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
		}

		bool pointInTriangle(const Vec2 &p, const Vec2 &a, const Vec2 &b, const Vec2 &c)
		{
			const float c1 = cross2D(a, b, p);
			const float c2 = cross2D(b, c, p);
			const float c3 = cross2D(c, a, p);
			const bool hasNeg = (c1 < 0.0f) || (c2 < 0.0f) || (c3 < 0.0f);
			const bool hasPos = (c1 > 0.0f) || (c2 > 0.0f) || (c3 > 0.0f);
			return !(hasNeg && hasPos);
		}

		std::vector<Triangle> triangulateFace(const Face &face, const std::vector<Vec3> &positions)
		{
			std::vector<Triangle> out;
			if (face.vertices.size() < 3U)
			{
				return out;
			}
			if (face.vertices.size() == 3U)
			{
				out.push_back({0, 1, 2});
				return out;
			}

			const Vec3 faceNormal = computeFaceNormal(face, positions);
			std::vector<Vec2> projected;
			projected.reserve(face.vertices.size());
			for (const IndexTriplet &idx : face.vertices)
			{
				projected.push_back(projectPoint(positions[idx.v], faceNormal));
			}

			const float area = signedArea(projected);
			if (std::fabs(area) <= 1e-6f)
			{
				for (std::size_t i = 1; i + 1 < face.vertices.size(); ++i)
				{
					out.push_back({0, static_cast<int>(i), static_cast<int>(i + 1U)});
				}
				return out;
			}

			std::vector<int> remaining(face.vertices.size());
			for (std::size_t i = 0; i < face.vertices.size(); ++i)
			{
				remaining[i] = static_cast<int>(i);
			}

			const bool ccw = area > 0.0f;
			std::size_t guard = 0U;
			while (remaining.size() > 3U && guard < face.vertices.size() * face.vertices.size())
			{
				bool earFound = false;
				for (std::size_t i = 0; i < remaining.size(); ++i)
				{
					const int prev = remaining[(i + remaining.size() - 1U) % remaining.size()];
					const int curr = remaining[i];
					const int next = remaining[(i + 1U) % remaining.size()];

					const float corner = cross2D(projected[prev], projected[curr], projected[next]);
					if ((ccw && corner <= 1e-6f) || (!ccw && corner >= -1e-6f))
					{
						continue;
					}

					bool containsPoint = false;
					for (int candidate : remaining)
					{
						if (candidate == prev || candidate == curr || candidate == next)
						{
							continue;
						}
						if (pointInTriangle(projected[candidate], projected[prev], projected[curr], projected[next]))
						{
							containsPoint = true;
							break;
						}
					}
					if (containsPoint)
					{
						continue;
					}

					out.push_back({prev, curr, next});
					remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
					earFound = true;
					break;
				}

				if (!earFound)
				{
					out.clear();
					for (std::size_t i = 1; i + 1 < face.vertices.size(); ++i)
					{
						out.push_back({0, static_cast<int>(i), static_cast<int>(i + 1U)});
					}
					return out;
				}
				++guard;
			}

			if (remaining.size() == 3U)
			{
				out.push_back({remaining[0], remaining[1], remaining[2]});
			}
			return out;
		}

		float normalizedAxis(float value, float minValue, float maxValue)
		{
			const float extent = maxValue - minValue;
			if (extent <= 1e-6f)
			{
				return 0.5f;
			}
			return (value - minValue) / extent;
		}

		Vec2 generateBoxUV(const Vec3 &position, const Vec3 &normal, const Bounds &bounds)
		{
			const float ax = std::fabs(normal.x);
			const float ay = std::fabs(normal.y);
			const float az = std::fabs(normal.z);
			Vec2 uv;

			if (ax >= ay && ax >= az)
			{
				uv.x = normalizedAxis(position.z, bounds.min.z, bounds.max.z);
				uv.y = normalizedAxis(position.y, bounds.min.y, bounds.max.y);
			}
			else if (ay >= az)
			{
				uv.x = normalizedAxis(position.x, bounds.min.x, bounds.max.x);
				uv.y = normalizedAxis(position.z, bounds.min.z, bounds.max.z);
			}
			else
			{
				uv.x = normalizedAxis(position.x, bounds.min.x, bounds.max.x);
				uv.y = normalizedAxis(position.y, bounds.min.y, bounds.max.y);
			}
			uv.y = 1.0f - uv.y;
			return uv;
		}

		RawObj parseObj(const std::string &path)
		{
			std::ifstream file(path.c_str());
			if (!file)
			{
				throw std::runtime_error("Failed to open OBJ file: " + path);
			}

			RawObj raw;
			raw.bounds.min = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			raw.bounds.max = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

			std::string line;
			std::size_t lineNumber = 0U;
			while (std::getline(file, line))
			{
				++lineNumber;
				if (line.empty() || line[0] == '#')
				{
					continue;
				}

				std::istringstream stream(line);
				std::string type;
				stream >> type;
				if (type == "v")
				{
					float x = 0.0f;
					float y = 0.0f;
					float z = 0.0f;
					stream >> x >> y >> z;
					raw.positions.push_back(Vec3(x, y, z));
					raw.bounds.min = minVec(raw.bounds.min, raw.positions.back());
					raw.bounds.max = maxVec(raw.bounds.max, raw.positions.back());
				}
				else if (type == "vt")
				{
					float u = 0.0f;
					float v = 0.0f;
					stream >> u >> v;
					raw.texcoords.push_back(Vec2(u, 1.0f - v));
				}
				else if (type == "vn")
				{
					float x = 0.0f;
					float y = 0.0f;
					float z = 0.0f;
					stream >> x >> y >> z;
					raw.normals.push_back(normalize(Vec3(x, y, z)));
				}
				else if (type == "f")
				{
					Face face;
					std::string token;
					while (stream >> token)
					{
						face.vertices.push_back(parseFaceToken(
							token,
							static_cast<int>(raw.positions.size()),
							static_cast<int>(raw.texcoords.size()),
							static_cast<int>(raw.normals.size())));
					}
					if (face.vertices.size() >= 3U)
					{
						raw.faces.push_back(face);
					}
				}
			}

			if (raw.positions.empty() || raw.faces.empty())
			{
				throw std::runtime_error("OBJ file contains no renderable geometry: " + path);
			}
			return raw;
		}

	} // namespace

	MeshData ObjLoader::loadFromFile(const std::string &path)
	{
		const RawObj raw = parseObj(path);

		MeshData mesh;
		mesh.bounds = raw.bounds;
		mesh.hasSourceTexcoords = !raw.texcoords.empty();
		mesh.usedGeneratedTexcoords = false;

		for (std::size_t faceIndex = 0; faceIndex < raw.faces.size(); ++faceIndex)
		{
			const Face &face = raw.faces[faceIndex];
			const Vec3 faceNormal = computeFaceNormal(face, raw.positions);
			const Vec3 faceColor = chooseFaceColor(faceNormal, faceIndex);
			const std::vector<Triangle> triangles = triangulateFace(face, raw.positions);

			for (const Triangle &tri : triangles)
			{
				const IndexTriplet triplets[3] = {
					face.vertices[static_cast<std::size_t>(tri.a)],
					face.vertices[static_cast<std::size_t>(tri.b)],
					face.vertices[static_cast<std::size_t>(tri.c)]};

				const Vec3 triPositions[3] = {
					raw.positions[triplets[0].v],
					raw.positions[triplets[1].v],
					raw.positions[triplets[2].v]};
				Vec3 triNormal = normalize(cross(triPositions[1] - triPositions[0], triPositions[2] - triPositions[0]));
				if (length(triNormal) <= 1e-6f)
				{
					triNormal = faceNormal;
				}

				for (int corner = 0; corner < 3; ++corner)
				{
					Vertex vertex;
					vertex.position = triPositions[corner];
					vertex.color = faceColor;
					vertex.normal = triNormal;

					if (triplets[corner].vt >= 0)
					{
						vertex.uv = raw.texcoords[triplets[corner].vt];
					}
					else
					{
						vertex.uv = generateBoxUV(vertex.position, triNormal, raw.bounds);
						mesh.usedGeneratedTexcoords = true;
					}

					mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
					mesh.vertices.push_back(vertex);
				}
			}
		}

		if (mesh.vertices.empty() || mesh.indices.empty())
		{
			throw std::runtime_error("OBJ parsing produced an empty mesh: " + path);
		}
		return mesh;
	}

} // namespace scop
