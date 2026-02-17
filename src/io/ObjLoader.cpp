#include "scop/io/ObjLoader.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>

namespace scop::io
{
	static inline float clamp01(float x)
	{
		if (x < 0.f)
			return 0.f;
		if (x > 1.f)
			return 1.f;
		return x;
	}

	// Box mapping based on dominant normal axis.
	// This fixes "texture only visible on some angles" when OBJ has no UVs (vt=0),
	// because planar mapping collapses on thin axes for extruded shapes.
	static void applyAutoUV(std::vector<scop::vk::Vertex> &verts)
	{
		if (verts.empty())
			return;

		float mn[3] = {
			std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity()};
		float mx[3] = {
			-std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity()};

		for (const auto &v : verts)
		{
			for (int i = 0; i < 3; ++i)
			{
				mn[i] = std::min(mn[i], v.pos[i]);
				mx[i] = std::max(mx[i], v.pos[i]);
			}
		}

		float ext[3] = {mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]};
		// avoid divide by zero
		for (int i = 0; i < 3; ++i)
			if (ext[i] < 1e-6f)
				ext[i] = 1.0f;

		for (auto &v : verts)
		{
			const float nx = std::fabs(v.nrm[0]);
			const float ny = std::fabs(v.nrm[1]);
			const float nz = std::fabs(v.nrm[2]);

			float u = 0.f;
			float vv = 0.f;

			// choose projection plane by dominant normal axis
			if (nx >= ny && nx >= nz)
			{
				// ±X face -> project on (Z,Y)
				u = (v.pos[2] - mn[2]) / ext[2];
				vv = (v.pos[1] - mn[1]) / ext[1];
			}
			else if (ny >= nx && ny >= nz)
			{
				// ±Y face -> project on (X,Z)
				u = (v.pos[0] - mn[0]) / ext[0];
				vv = (v.pos[2] - mn[2]) / ext[2];
			}
			else
			{
				// ±Z face -> project on (X,Y)
				u = (v.pos[0] - mn[0]) / ext[0];
				vv = (v.pos[1] - mn[1]) / ext[1];
			}

			v.uv[0] = clamp01(u);
			v.uv[1] = clamp01(1.0f - vv); // flip V (if upside down, use clamp01(vv))
		}
	}

	static std::string dirOf(const std::string &p)
	{
		const size_t s = p.find_last_of("/\\");
		if (s == std::string::npos)
			return "";
		return p.substr(0, s + 1);
	}

	static std::string trimLeft(std::string s)
	{
		size_t i = 0;
		while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
			++i;
		return s.substr(i);
	}

	static bool startsWith(const std::string &s, const char *pfx)
	{
		size_t i = 0;
		while (pfx[i])
		{
			if (i >= s.size() || s[i] != pfx[i])
				return false;
			++i;
		}
		return true;
	}

	static bool looksAbsolutePath(const std::string &p)
	{
		if (p.empty())
			return false;
		if (p[0] == '/')
			return true;
		if (p.size() > 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':')
			return true; // windows drive
		return false;
	}

	struct V3
	{
		float x, y, z;
	};
	struct V2
	{
		float u, v;
	};

	static int fixIndex(int idx, int n)
	{
		// OBJ indices: 1..n, or negative = relative to end
		if (idx > 0)
			return idx - 1;
		if (idx < 0)
			return n + idx;
		return -1;
	}

	static void parseFaceToken(const std::string &tok, int &vi, int &ti, int &ni)
	{
		vi = ti = ni = 0;
		// v/vt/vn or v//vn or v/vt or v
		const size_t a = tok.find('/');
		if (a == std::string::npos)
		{
			vi = std::stoi(tok);
			return;
		}
		const size_t b = tok.find('/', a + 1);
		vi = std::stoi(tok.substr(0, a));
		if (b == std::string::npos)
		{
			// v/vt
			std::string s1 = tok.substr(a + 1);
			if (!s1.empty())
				ti = std::stoi(s1);
			return;
		}
		// v/vt/vn OR v//vn
		std::string s1 = tok.substr(a + 1, b - (a + 1));
		std::string s2 = tok.substr(b + 1);
		if (!s1.empty())
			ti = std::stoi(s1);
		if (!s2.empty())
			ni = std::stoi(s2);
	}

	static V3 faceNormal(const V3 &a, const V3 &b, const V3 &c)
	{
		const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
		V3 n{};
		n.x = uy * vz - uz * vy;
		n.y = uz * vx - ux * vz;
		n.z = ux * vy - uy * vx;
		const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if (len > 0.000001f)
		{
			n.x /= len;
			n.y /= len;
			n.z /= len;
		}
		return n;
	}

	// Tries to extract filename from map_Kd line that may contain options.
	// For our needs: choose last token that doesn't start with '-'.
	static std::string parseMapLine(const std::string &rest)
	{
		std::istringstream ss(rest);
		std::string tok;
		std::string lastPath;
		while (ss >> tok)
		{
			if (!tok.empty() && tok[0] == '-')
				continue;
			lastPath = tok;
		}
		return lastPath;
	}

	static Material parseMtlFile(const std::string &mtlPath, const std::string &wantName)
	{
		Material out{};
		std::ifstream f(mtlPath);
		if (!f)
			return out;

		const std::string mtlDir = dirOf(mtlPath);

		std::string line;
		std::string currentName;
		Material current{};

		auto commitIfWanted = [&](bool force)
		{
			if (currentName.empty())
				return;
			if (force)
			{
				if (out.name.empty())
					out = current;
				return;
			}
			if (!wantName.empty() && currentName == wantName)
			{
				out = current;
			}
			else if (wantName.empty() && out.name.empty())
			{
				// if no requested material, take the first
				out = current;
			}
		};

		while (std::getline(f, line))
		{
			line = trimLeft(line);
			if (line.empty() || line[0] == '#')
				continue;

			if (startsWith(line, "newmtl "))
			{
				commitIfWanted(false);

				currentName = trimLeft(line.substr(7));
				current = Material{};
				current.name = currentName;
				continue;
			}

			if (currentName.empty())
				continue;

			if (startsWith(line, "Kd "))
			{
				std::istringstream ss(line.substr(3));
				ss >> current.Kd[0] >> current.Kd[1] >> current.Kd[2];
				continue;
			}
			if (startsWith(line, "Ks "))
			{
				std::istringstream ss(line.substr(3));
				ss >> current.Ks[0] >> current.Ks[1] >> current.Ks[2];
				continue;
			}
			if (startsWith(line, "Ns "))
			{
				std::istringstream ss(line.substr(3));
				ss >> current.Ns;
				continue;
			}
			if (startsWith(line, "d "))
			{
				std::istringstream ss(line.substr(2));
				ss >> current.d;
				continue;
			}
			if (startsWith(line, "Tr "))
			{
				float tr = 0.f;
				std::istringstream ss(line.substr(3));
				ss >> tr;
				current.d = 1.f - tr;
				continue;
			}
			if (startsWith(line, "map_Kd "))
			{
				std::string rest = trimLeft(line.substr(7));
				std::string tex = parseMapLine(rest);
				if (!tex.empty())
				{
					if (looksAbsolutePath(tex))
						current.mapKd = tex;
					else
						current.mapKd = mtlDir + tex;
				}
				continue;
			}
		}

		commitIfWanted(true);
		return out;
	}

	MeshData loadObj(const std::string &objPath, bool triangulate)
	{
		bool sawVT = false;

		std::ifstream f(objPath);
		if (!f)
			throw std::runtime_error("OBJ open failed: " + objPath);

		const std::string objDir = dirOf(objPath);

		std::vector<V3> positions;
		std::vector<V3> normals;
		std::vector<V2> uvs;

		std::string mtllib;
		std::string firstUseMtl;

		std::unordered_map<std::string, uint32_t> cache;
		MeshData out;

		std::string line;
		while (std::getline(f, line))
		{
			line = trimLeft(line);
			if (line.empty() || line[0] == '#')
				continue;

			if (startsWith(line, "mtllib "))
			{
				if (mtllib.empty())
					mtllib = trimLeft(line.substr(7));
				continue;
			}
			if (startsWith(line, "usemtl "))
			{
				if (firstUseMtl.empty())
					firstUseMtl = trimLeft(line.substr(7));
				continue;
			}
			if (startsWith(line, "v "))
			{
				std::istringstream ss(line.substr(2));
				V3 v{};
				ss >> v.x >> v.y >> v.z;
				positions.push_back(v);
				continue;
			}
			if (startsWith(line, "vn "))
			{
				std::istringstream ss(line.substr(3));
				V3 n{};
				ss >> n.x >> n.y >> n.z;
				normals.push_back(n);
				continue;
			}
			if (startsWith(line, "vt "))
			{
				std::istringstream ss(line.substr(3));
				V2 t{};
				ss >> t.u >> t.v;
				uvs.push_back(t);
				sawVT = true;
				continue;
			}
			if (startsWith(line, "f "))
			{
				std::istringstream ss(line.substr(2));
				std::vector<std::string> face;
				std::string tok;
				while (ss >> tok)
					face.push_back(tok);
				if (face.size() < 3)
					continue;

				auto getPosFromTok = [&](const std::string &ftok) -> V3
				{
					int vi = 0, ti = 0, ni = 0;
					parseFaceToken(ftok, vi, ti, ni);
					const int p = fixIndex(vi, (int)positions.size());
					return positions.at((size_t)p);
				};

				auto emitVertex = [&](const std::string &ftok, const V3 &faceNrm, bool hasFaceNrm) -> uint32_t
				{
					int vi = 0, ti = 0, ni = 0;
					parseFaceToken(ftok, vi, ti, ni);

					const int p = fixIndex(vi, (int)positions.size());
					const int t = (ti != 0) ? fixIndex(ti, (int)uvs.size()) : -1;
					const int n = (ni != 0) ? fixIndex(ni, (int)normals.size()) : -1;

					// If no vn in token -> do not cache across faces (flat normal)
					if (n < 0)
					{
						scop::vk::Vertex v{};
						const V3 P = positions.at((size_t)p);
						v.pos[0] = P.x;
						v.pos[1] = P.y;
						v.pos[2] = P.z;

						const V3 N = hasFaceNrm ? faceNrm : V3{0.f, 1.f, 0.f};
						v.nrm[0] = N.x;
						v.nrm[1] = N.y;
						v.nrm[2] = N.z;

						if (t >= 0)
						{
							const V2 T = uvs.at((size_t)t);
							v.uv[0] = T.u;
							v.uv[1] = T.v;
						}
						else
						{
							v.uv[0] = 0.f;
							v.uv[1] = 0.f;
						}

						out.vertices.push_back(v);
						return (uint32_t)(out.vertices.size() - 1);
					}

					// cache key for smooth data
					std::string key = std::to_string(vi) + "/" + std::to_string(ti) + "/" + std::to_string(ni);
					auto it = cache.find(key);
					if (it != cache.end())
						return it->second;

					scop::vk::Vertex v{};
					const V3 P = positions.at((size_t)p);
					v.pos[0] = P.x;
					v.pos[1] = P.y;
					v.pos[2] = P.z;

					const V3 N = normals.at((size_t)n);
					v.nrm[0] = N.x;
					v.nrm[1] = N.y;
					v.nrm[2] = N.z;

					if (t >= 0)
					{
						const V2 T = uvs.at((size_t)t);
						v.uv[0] = T.u;
						v.uv[1] = T.v;
					}
					else
					{
						v.uv[0] = 0.f;
						v.uv[1] = 0.f;
					}

					out.vertices.push_back(v);
					const uint32_t idx = (uint32_t)(out.vertices.size() - 1);
					cache[key] = idx;
					return idx;
				};

				if (triangulate && face.size() > 3)
				{
					for (size_t k = 1; k + 1 < face.size(); ++k)
					{
						const V3 A = getPosFromTok(face[0]);
						const V3 B = getPosFromTok(face[k]);
						const V3 C = getPosFromTok(face[k + 1]);
						const V3 FN = faceNormal(A, B, C);

						out.indices.push_back(emitVertex(face[0], FN, true));
						out.indices.push_back(emitVertex(face[k], FN, true));
						out.indices.push_back(emitVertex(face[k + 1], FN, true));
					}
				}
				else
				{
					const V3 A = getPosFromTok(face[0]);
					const V3 B = getPosFromTok(face[1]);
					const V3 C = getPosFromTok(face[2]);
					const V3 FN = faceNormal(A, B, C);

					out.indices.push_back(emitVertex(face[0], FN, true));
					out.indices.push_back(emitVertex(face[1], FN, true));
					out.indices.push_back(emitVertex(face[2], FN, true));
				}
			}
		}

		// If OBJ has no UVs, generate them
		if (!sawVT)
			applyAutoUV(out.vertices);

		// MTL parsing (always use it if present)
		if (!mtllib.empty())
		{
			const std::string mtlPath = looksAbsolutePath(mtllib) ? mtllib : (objDir + mtllib);
			out.material = parseMtlFile(mtlPath, firstUseMtl);
		}

		return out;
	}

} // namespace scop::io
