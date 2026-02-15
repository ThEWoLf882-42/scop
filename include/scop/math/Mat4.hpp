#pragma once
#include <cmath>

namespace scop::math
{

	struct Mat4
	{
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

		static Mat4 rotationX(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad);
			float s = std::sin(rad);

			r.m[5] = c;
			r.m[6] = s;
			r.m[9] = -s;
			r.m[10] = c;
			return r;
		}

		static Mat4 rotationY(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad);
			float s = std::sin(rad);

			// col0
			r.m[0] = c;
			r.m[2] = -s;
			// col2
			r.m[8] = s;
			r.m[10] = c;
			return r;
		}

		static Mat4 rotationZ(float rad)
		{
			Mat4 r = identity();
			float c = std::cos(rad);
			float s = std::sin(rad);

			r.m[0] = c;
			r.m[1] = s;
			r.m[4] = -s;
			r.m[5] = c;
			return r;
		}

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
