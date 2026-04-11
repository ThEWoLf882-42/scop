#pragma once

#include <cstddef>
#include <cstdint>

namespace scop
{

	struct Vec2
	{
		float x;
		float y;

		Vec2();
		Vec2(float xv, float yv);

		Vec2 operator+(const Vec2 &other) const;
		Vec2 operator-(const Vec2 &other) const;
		Vec2 operator*(float scalar) const;
	};

	struct Vec3
	{
		float x;
		float y;
		float z;

		Vec3();
		Vec3(float xv, float yv, float zv);

		Vec3 operator+(const Vec3 &other) const;
		Vec3 operator-(const Vec3 &other) const;
		Vec3 operator*(float scalar) const;
		Vec3 operator/(float scalar) const;
		Vec3 &operator+=(const Vec3 &other);
	};

	struct alignas(16) Vec4
	{
		float x;
		float y;
		float z;
		float w;

		Vec4();
		Vec4(float xv, float yv, float zv, float wv);
	};

	struct alignas(16) Mat4
	{
		float m[16];

		Mat4();

		static Mat4 identity();
		static Mat4 translation(const Vec3 &translation);
		static Mat4 scale(const Vec3 &scale);
		static Mat4 rotationAxis(const Vec3 &axis, float angleRadians);
		static Mat4 rotationY(float angleRadians);
		static Mat4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane);
		static Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up);

		float &operator()(std::size_t row, std::size_t col);
		float operator()(std::size_t row, std::size_t col) const;
	};

	Mat4 operator*(const Mat4 &lhs, const Mat4 &rhs);

	float dot(const Vec2 &lhs, const Vec2 &rhs);
	float dot(const Vec3 &lhs, const Vec3 &rhs);
	Vec3 cross(const Vec3 &lhs, const Vec3 &rhs);
	float length(const Vec3 &v);
	float length(const Vec2 &v);
	Vec3 normalize(const Vec3 &v);
	Vec2 normalize(const Vec2 &v);
	Vec3 minVec(const Vec3 &lhs, const Vec3 &rhs);
	Vec3 maxVec(const Vec3 &lhs, const Vec3 &rhs);

} // namespace scop
