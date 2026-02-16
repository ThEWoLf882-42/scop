#include "scop/io/ObjLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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

	static inline float absf(float x) { return x < 0.f ? -x : x; }

	// Planar projection using the 2 largest bbox axes (fallback).
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
		for (int i = 0; i < 3; ++i)
			if (ext[i] < 1e-6f)
				ext[i] = 1.0f;

		int ax[3] = {0, 1, 2};
		std::sort(ax, ax + 3, [&](int a, int b)
				  { return ext[a] > ext[b]; });
		const int A = ax[0];
		const int B = ax[1];

		for (auto &v : verts)
		{
			const float u = (v.pos[A] - mn[A]) / ext[A];
			const float vv = (v.pos[B] - mn[B]) / ext[B];

			v.uv[0] = clamp01(u);
			v.uv[1] = clamp01(vv); // flip V (if upside down, use clamp01(vv))
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
			return true;
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

	// Cube-style per-face projection: choose plane based on dominant normal axis.
	static void setAutoUVCube(scop::vk::Vertex &v,
							  const V3 &P,
							  const V3 &faceN,
							  const float mn[3],
							  const float mx[3])
	{
		float ext[3] = {mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]};
		for (int i = 0; i < 3; ++i)
			if (ext[i] < 1e-6f)
				ext[i] = 1.0f;

		const float ax = absf(faceN.x), ay = absf(faceN.y), az = absf(faceN.z);

		float u = 0.f, vv = 0.f;

		if (az >= ax && az >= ay) // Z -> XY
		{
			u = (P.x - mn[0]) / ext[0];
			vv = (P.y - mn[1]) / ext[1];
		}
		else if (ax >= ay && ax >= az) // X -> ZY
		{
			u = (P.z - mn[2]) / ext[2];
			vv = (P.y - mn[1]) / ext[1];
		}
		else // Y -> XZ
		{
			u = (P.x - mn[0]) / ext[0];
			vv = (P.z - mn[2]) / ext[2];
		}

		v.uv[0] = clamp01(u);
		v.uv[1] = clamp01(vv); // if upside down: clamp01(vv)
	}

	static int fixIndex(int idx, int n)
	{
		if (idx > 0)
			return idx - 1;
		if (idx < 0)
			return n + idx;
		return -1;
	}

	static void parseFaceToken(const std::string &tok, int &vi, int &ti, int &ni)
	{
		vi = ti = ni = 0;
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
			std::string s1 = tok.substr(a + 1);
			if (!s1.empty())
				ti = std::stoi(s1);
			return;
		}
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

	// Choose last token that doesn't start with '-'
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
		bool usedUV = false;
		bool generatedUV = false;

		std::ifstream f(objPath);
		if (!f)
			throw std::runtime_error("OBJ open failed: " + objPath);

		const std::string objDir = dirOf(objPath);

		std::vector<V3> positions;
		std::vector<V3> normals;
		std::vector<V2> uvs;

		// BBox for AutoUV
		float bboxMn[3] = {
			std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity()};
		float bboxMx[3] = {
			-std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity()};

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

				bboxMn[0] = std::min(bboxMn[0], v.x);
				bboxMx[0] = std::max(bboxMx[0], v.x);
				bboxMn[1] = std::min(bboxMn[1], v.y);
				bboxMx[1] = std::max(bboxMx[1], v.y);
				bboxMn[2] = std::min(bboxMn[2], v.z);
				bboxMx[2] = std::max(bboxMx[2], v.z);

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

					// If no vn OR no vt -> do not cache across faces (UV depends on face projection)
					if (n < 0 || t < 0)
					{
						scop::vk::Vertex v{};
						const V3 P = positions.at((size_t)p);
						v.pos[0] = P.x;
						v.pos[1] = P.y;
						v.pos[2] = P.z;

						V3 N = hasFaceNrm ? faceNrm : V3{0.f, 1.f, 0.f};
						if (n >= 0)
							N = normals.at((size_t)n);

						v.nrm[0] = N.x;
						v.nrm[1] = N.y;
						v.nrm[2] = N.z;

						if (t >= 0)
						{
							usedUV = true;
							const V2 T = uvs.at((size_t)t);
							v.uv[0] = T.u;
							v.uv[1] = T.v;
						}
						else
						{
							// AutoUV per-face
							generatedUV = true;
							const V3 projN = hasFaceNrm ? faceNrm : N;
							setAutoUVCube(v, P, projN, bboxMn, bboxMx);
						}

						out.vertices.push_back(v);
						return (uint32_t)(out.vertices.size() - 1);
					}

					// cache key for smooth data (v/vt/vn)
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

					usedUV = true;
					const V2 T = uvs.at((size_t)t);
					v.uv[0] = T.u;
					v.uv[1] = T.v;

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

		// MTL parsing (always use it if present)
		if (!mtllib.empty())
		{
			const std::string mtlPath = looksAbsolutePath(mtllib) ? mtllib : (objDir + mtllib);
			out.material = parseMtlFile(mtlPath, firstUseMtl);
		}

		// If OBJ had no UVs or faces didn't reference UVs, we already generated per-face UVs.
		// If for some reason we didn't generate (no faces), planar fallback keeps things safe.
		if (!sawVT || !usedUV)
		{
			if (generatedUV)
				std::cout << "[OBJ] UV missing -> AutoUV cube-per-face enabled\n";
			else
			{
				std::cout << "[OBJ] UV missing -> AutoUV planar fallback enabled\n";
				applyAutoUV(out.vertices);
			}
		}

		return out;
	}

} // namespace scop::io
