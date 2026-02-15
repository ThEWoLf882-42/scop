#pragma once
#include <cmath>

namespace scop::math
{

	struct Vec3
	{
		float x, y, z;
	};

	static inline Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	static inline float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	static inline Vec3 cross(const Vec3 &a, const Vec3 &b)
	{
		return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}
	static inline float len(const Vec3 &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
	static inline Vec3 normalize(const Vec3 &v)
	{
		float l = len(v);
		if (l <= 0.0f)
			return {0.f, 0.f, 1.f};
		return {v.x / l, v.y / l, v.z / l};
	}

	struct Mat4
	{
		// Column-major
		float m[16];

		static Mat4 identity()
		{
			Mat4 r{};
			r.m[0] = 1.f;
			r.m[5] = 1.f;
			r.m[10] = 1.f;
			r.m[15] = 1.f;
			return r;
		}

		static Mat4 translation(float x, float y, float z)
		{
			Mat4 r = identity();
			r.m[12] = x;
			r.m[13] = y;
			r.m[14] = z;
			return r;
		}

		static Mat4 scale(float s)
		{
			Mat4 r{};
			r.m[0] = s;
			r.m[5] = s;
			r.m[10] = s;
			r.m[15] = 1.f;
			return r;
		}

		static Mat4 scale(float x, float y, float z)
		{
			Mat4 r{};
			r.m[0] = x;
			r.m[5] = y;
			r.m[10] = z;
			r.m[15] = 1.f;
			return r;
		}

		static Mat4 rotationX(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad), s = std::sin(rad);
			r.m[5] = c;
			r.m[6] = s;
			r.m[9] = -s;
			r.m[10] = c;
			return r;
		}

		static Mat4 rotationY(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad), s = std::sin(rad);
			r.m[0] = c;
			r.m[2] = -s;
			r.m[8] = s;
			r.m[10] = c;
			return r;
		}

		static Mat4 rotationZ(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad), s = std::sin(rad);
			r.m[0] = c;
			r.m[1] = s;
			r.m[4] = -s;
			r.m[5] = c;
			return r;
		}

		// Vulkan depth: 0..1, flipY=true typical
		static Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar, bool flipY = true)
		{
			Mat4 r{};
			const float f = 1.0f / std::tan(fovYRad * 0.5f);

			r.m[0] = f / aspect;
			r.m[5] = flipY ? -f : f;

			r.m[10] = zFar / (zNear - zFar);
			r.m[11] = -1.0f;

			r.m[14] = (zFar * zNear) / (zNear - zFar);
			return r;
		}

		// Right-handed lookAt
		static Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)
		{
			const Vec3 f = normalize(sub(center, eye));
			const Vec3 s = normalize(cross(f, up));
			const Vec3 u = cross(s, f);

			Mat4 r = identity();

			// col0 (s)
			r.m[0] = s.x;
			r.m[1] = s.y;
			r.m[2] = s.z;
			// col1 (u)
			r.m[4] = u.x;
			r.m[5] = u.y;
			r.m[6] = u.z;
			// col2 (-f)
			r.m[8] = -f.x;
			r.m[9] = -f.y;
			r.m[10] = -f.z;

			// col3 (translation)
			r.m[12] = -dot(s, eye);
			r.m[13] = -dot(u, eye);
			r.m[14] = dot(f, eye);

			return r;
		}

		static Mat4 mul(const Mat4 &a, const Mat4 &b)
		{
			Mat4 r{};
			for (int col = 0; col < 4; ++col)
			{
				for (int row = 0; row < 4; ++row)
				{
					r.m[col * 4 + row] =
						a.m[0 * 4 + row] * b.m[col * 4 + 0] +
						a.m[1 * 4 + row] * b.m[col * 4 + 1] +
						a.m[2 * 4 + row] * b.m[col * 4 + 2] +
						a.m[3 * 4 + row] * b.m[col * 4 + 3];
				}
			}
			return r;
		}
	};

}
