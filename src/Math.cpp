#include "Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace scop
{

	Vec2::Vec2() : x(0.0f), y(0.0f) {}
	Vec2::Vec2(float xv, float yv) : x(xv), y(yv) {}
	Vec2 Vec2::operator+(const Vec2 &other) const { return Vec2(x + other.x, y + other.y); }
	Vec2 Vec2::operator-(const Vec2 &other) const { return Vec2(x - other.x, y - other.y); }
	Vec2 Vec2::operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }

	Vec3::Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
	Vec3::Vec3(float xv, float yv, float zv) : x(xv), y(yv), z(zv) {}
	Vec3 Vec3::operator+(const Vec3 &other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
	Vec3 Vec3::operator-(const Vec3 &other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
	Vec3 Vec3::operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
	Vec3 Vec3::operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
	Vec3 &Vec3::operator+=(const Vec3 &other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	Vec4::Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
	Vec4::Vec4(float xv, float yv, float zv, float wv) : x(xv), y(yv), z(zv), w(wv) {}

	Mat4::Mat4()
	{
		std::memset(m, 0, sizeof(m));
	}

	Mat4 Mat4::identity()
	{
		Mat4 result;
		result(0, 0) = 1.0f;
		result(1, 1) = 1.0f;
		result(2, 2) = 1.0f;
		result(3, 3) = 1.0f;
		return result;
	}

	Mat4 Mat4::translation(const Vec3 &translation)
	{
		Mat4 result = identity();
		result(0, 3) = translation.x;
		result(1, 3) = translation.y;
		result(2, 3) = translation.z;
		return result;
	}

	Mat4 Mat4::scale(const Vec3 &scale)
	{
		Mat4 result = identity();
		result(0, 0) = scale.x;
		result(1, 1) = scale.y;
		result(2, 2) = scale.z;
		return result;
	}

	Mat4 Mat4::rotationAxis(const Vec3 &axis, float angleRadians)
	{
		Vec3 n = normalize(axis);
		const float c = std::cos(angleRadians);
		const float s = std::sin(angleRadians);
		const float t = 1.0f - c;

		Mat4 result = identity();
		result(0, 0) = c + n.x * n.x * t;
		result(0, 1) = n.x * n.y * t - n.z * s;
		result(0, 2) = n.x * n.z * t + n.y * s;

		result(1, 0) = n.y * n.x * t + n.z * s;
		result(1, 1) = c + n.y * n.y * t;
		result(1, 2) = n.y * n.z * t - n.x * s;

		result(2, 0) = n.z * n.x * t - n.y * s;
		result(2, 1) = n.z * n.y * t + n.x * s;
		result(2, 2) = c + n.z * n.z * t;
		return result;
	}

	Mat4 Mat4::rotationY(float angleRadians)
	{
		return rotationAxis(Vec3(0.0f, 1.0f, 0.0f), angleRadians);
	}

	Mat4 Mat4::perspective(float fovRadians, float aspect, float nearPlane, float farPlane)
	{
		Mat4 result;
		const float tanHalfFov = std::tan(fovRadians / 2.0f);
		result(0, 0) = 1.0f / (aspect * tanHalfFov);
		result(1, 1) = 1.0f / tanHalfFov;
		result(2, 2) = farPlane / (nearPlane - farPlane);
		result(2, 3) = (farPlane * nearPlane) / (nearPlane - farPlane);
		result(3, 2) = -1.0f;
		return result;
	}

	Mat4 Mat4::lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)
	{
		const Vec3 f = normalize(center - eye);
		const Vec3 s = normalize(cross(f, up));
		const Vec3 u = cross(s, f);

		Mat4 result = identity();
		result(0, 0) = s.x;
		result(1, 0) = s.y;
		result(2, 0) = s.z;

		result(0, 1) = u.x;
		result(1, 1) = u.y;
		result(2, 1) = u.z;

		result(0, 2) = -f.x;
		result(1, 2) = -f.y;
		result(2, 2) = -f.z;

		result(0, 3) = -dot(s, eye);
		result(1, 3) = -dot(u, eye);
		result(2, 3) = dot(f, eye);
		return result;
	}

	float &Mat4::operator()(std::size_t row, std::size_t col)
	{
		return m[col * 4 + row];
	}

	float Mat4::operator()(std::size_t row, std::size_t col) const
	{
		return m[col * 4 + row];
	}

	Mat4 operator*(const Mat4 &lhs, const Mat4 &rhs)
	{
		Mat4 result;
		for (std::size_t row = 0; row < 4; ++row)
		{
			for (std::size_t col = 0; col < 4; ++col)
			{
				float sum = 0.0f;
				for (std::size_t i = 0; i < 4; ++i)
				{
					sum += lhs(row, i) * rhs(i, col);
				}
				result(row, col) = sum;
			}
		}
		return result;
	}

	float dot(const Vec2 &lhs, const Vec2 &rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y;
	}

	float dot(const Vec3 &lhs, const Vec3 &rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	}

	Vec3 cross(const Vec3 &lhs, const Vec3 &rhs)
	{
		return Vec3(
			lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.z * rhs.x - lhs.x * rhs.z,
			lhs.x * rhs.y - lhs.y * rhs.x);
	}

	float length(const Vec3 &v)
	{
		return std::sqrt(dot(v, v));
	}

	float length(const Vec2 &v)
	{
		return std::sqrt(dot(v, v));
	}

	Vec3 normalize(const Vec3 &v)
	{
		const float len = length(v);
		if (len <= 1e-8f)
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}
		return v / len;
	}

	Vec2 normalize(const Vec2 &v)
	{
		const float len = length(v);
		if (len <= 1e-8f)
		{
			return Vec2(0.0f, 0.0f);
		}
		return Vec2(v.x / len, v.y / len);
	}

	Vec3 minVec(const Vec3 &lhs, const Vec3 &rhs)
	{
		return Vec3(std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z));
	}

	Vec3 maxVec(const Vec3 &lhs, const Vec3 &rhs)
	{
		return Vec3(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z));
	}

} // namespace scop
