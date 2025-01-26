#pragma once

#include "algorithm.hpp"
#include "fmt/printf.h"
#include "fmt/color.h"
#include <cstdio>
#include <math.h>
#include <assert.h>

namespace engine {

#define Cast(x) static_cast<T>(x)

	constexpr auto pi = 3.14159265358979323846;
	constexpr float float_max = std::numeric_limits<float>::max();
	constexpr float float_min = - float_max;

	constexpr inline float Lerp(float a, float b, float t) {
		return a * (1 - t) + b * t;
	}

	template<typename T>
	struct Vec3_T;

	template<typename T>
	struct Vec4_T;

	/*! @brief Two element vector
	*/
	template<typename T>
	struct Vec2_T {

		static constexpr inline size_t size = 2;

		static constexpr Vec2_T Right(T num = Cast(1)) {
			return Vec2_T(num, Cast(0));
		}

		static constexpr Vec2_T Left(T num = Cast(1)) {
			return Vec2_T(-num, Cast(0));
		}

		static constexpr Vec2_T Up(T num = Cast(1)) {
			return Vec2_T(Cast(0), num);
		}

		static constexpr Vec2_T Down(T num = Cast(1)) {
			return Vec2_T(Cast(0), -num);
		}

		T x, y;

		constexpr inline Vec2_T(T x = Cast(0), T y = Cast(0)) noexcept : x(x), y(y) {}

		template<typename U>
		constexpr inline Vec2_T(const Vec2_T<U>& other) noexcept : x(Cast(other.x)), y(Cast(other.y)) {}

		template<typename U>
		constexpr inline Vec2_T(const Vec3_T<U>& other) noexcept : x(Cast(other.x)), y(Cast(other.y)) {}

		template<typename U>
		constexpr inline Vec2_T(const Vec4_T<U>& other) noexcept : x(Cast(other.x)), y(Cast(other.y)) {}

		static constexpr inline T Dot(const Vec2_T& a, const Vec2_T& b) noexcept { return a.x * b.x + a.y * b.y; }

		constexpr inline T SqrMagnitude() const noexcept { return x * x + y * y; }

		constexpr inline T Magnitude() const noexcept { return sqrt(SqrMagnitude()); }

		constexpr inline Vec2_T Normalized() const noexcept {
			T mag = SqrMagnitude();
			if (mag <= Cast(0.00001)) {
				return Vec2(Cast(0), Cast(0));
			}
			mag = sqrt(mag);
			return Vec2_T(x / mag, y / mag);
		}

		constexpr inline Vec2_T Rotated(float rads) const {
			float c = cos(rads);
			float s = sin(rads);
			return {
				x * c - y * s,
				x * s - y * c,
			};
		}

		static constexpr Vec2_T Lerp(Vec2_T a, Vec2_T b, float t) {
			return a * (1 - t) + b * t;
		}

		constexpr inline Vec2_T operator*(T scalar) const noexcept { return Vec2_T(x * scalar, y * scalar); }
		constexpr inline Vec2_T& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; return *this; }
		constexpr inline Vec2_T operator/(T scalar) const noexcept { return Vec2_T(x / scalar, y / scalar); }
		constexpr inline Vec2_T operator-() const noexcept { return Vec2_T(-x, -y); }
		constexpr inline Vec2_T operator+(Vec2_T other) const noexcept { return Vec2_T(x + other.x, y + other.y); }
		constexpr inline Vec2_T operator-(Vec2_T other) const noexcept { return Vec2_T(x - other.x, y - other.y); }
		constexpr inline Vec2_T& operator+=(Vec2_T other) noexcept { x += other.x; y += other.y; return *this; }
		constexpr inline Vec2_T& operator-=(Vec2_T other) noexcept { x -= other.x; y -= other.y; return *this; }

		constexpr inline bool operator==(const Vec2_T& other) const noexcept = default;

		constexpr inline const T& operator[](size_t index) const {
			return index == 0 ? x : y;
		}
	};

	template<typename T>
	Vec2_T<T> Min(Vec2_T<T> a, Vec2_T<T> b) {
		if (a.SqrMagnitude() < b.SqrMagnitude()) {
			return a;
		}
		return b;
	}	

	typedef Vec2_T<float> Vec2;
	typedef Vec2_T<int> IntVec2;

	template<typename T>
	struct Mat3_T;

	template<typename T>
	struct Vec4_T;

	template<typename T>
	struct Vec3_T {

		static constexpr inline size_t size = 3;

		static constexpr const Vec3_T Right(T num = Cast(1)) {
			return Vec3_T(num, Cast(0), Cast(0));
		}

		static constexpr const Vec3_T Left(T num = Cast(1)) {
			return Vec3_T(-num, Cast(0), Cast(0));
		}

		static constexpr const Vec3_T Up(T num = Cast(1)) {
			return Vec3_T(Cast(0), num, Cast(0));
		}

		static constexpr const Vec3_T Down(T num = Cast(1)) {
			return Vec3_T(Cast(0), -num, Cast(0));
		}

		static constexpr const Vec3_T Forward(T num = Cast(1)) {
			return Vec3_T(Cast(0), Cast(0), num);
		}

		static constexpr const Vec3_T Backward(T num = Cast(1)) {
			return Vec3_T(Cast(0), Cast(0), -num);
		}

		T x, y, z;

		constexpr inline Vec3_T(T x = Cast(0), T y = Cast(0), T z = Cast(0)) noexcept : x(x), y(y), z(z) {}

		constexpr inline Vec3_T(const Vec2_T<T>& other) noexcept : x(Cast(other.x)), y(Cast(other.y)), z(Cast(0)) {}

		constexpr inline Vec3_T(const Vec4_T<T>& other) noexcept : x(Cast(other.x)), y(Cast(other.y)), z(Cast(other.z)) {}

		constexpr inline T SqrMagnitude() const noexcept { 
			return x * x + y * y + z * z; 
		}

		constexpr inline T Magnitude() const noexcept { return sqrt(SqrMagnitude()); }

		constexpr inline Vec3_T Normalized() const noexcept {
			T mag = SqrMagnitude();
			if (mag < Cast(0.00001f)) {
				return Vec3_T(Cast(0), Cast(0), Cast(0));
			}
			mag = sqrt(mag);
			return Vec3_T(x / mag, y / mag, z / mag);
		}

		constexpr inline Vec3_T operator*(T scalar) const noexcept { return Vec3_T(x * scalar, y * scalar, z * scalar); }
		constexpr inline Vec3_T operator/(T scalar) const noexcept { return Vec3_T(x / scalar, y / scalar, z / scalar); }
		constexpr inline Vec3_T operator-() const noexcept { return Vec3_T(-x, -y, -z); }
		constexpr inline Vec3_T operator+(const Vec3_T& other) const noexcept { return Vec3_T(x + other.x, y + other.y, z + other.z); }
		constexpr inline Vec3_T operator-(const Vec3_T& other) const noexcept { return Vec3_T(x - other.x, y - other.y, z - other.z); }
		constexpr inline Vec3_T& operator+=(const Vec3_T& other) noexcept { x += other.x; y += other.y; z += other.z; return *this; }
		constexpr inline Vec3_T& operator-=(const Vec3_T& other) noexcept { x -= other.x; y -= other.y; z -= other.z; return *this; }
		constexpr inline Vec3_T& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }

		constexpr inline Vec3_T operator*(const Mat3_T<T>& other) const {
			return Vec3_T<T>(
				x * other[0].x + y * other[0].y + z * other[0].z,
				x * other[1].x + y * other[1].y + z * other[1].z,
				x * other[2].x + y * other[2].y + z * other[2].z
			);
		}

		constexpr inline T& operator[](size_t index) const { assert(index < size); return ((T*)this)[index]; }

		constexpr inline bool operator==(const Vec3_T& other) const noexcept = default;
	};

	template<typename T>
	static constexpr inline Vec3_T<T> Cross(const Vec3_T<T>& a, const Vec3_T<T>& b) noexcept { 
		return Vec3_T(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); 
	}

	template<typename T>
	static constexpr inline T Dot(const Vec3_T<T>& a, const Vec3_T<T>& b) noexcept { 
		return a.x * b.x + a.y * b.y + a.z * b.z; 
	}

	template<typename T>
	static constexpr inline void Normalize(Vec3_T<T>& vec) noexcept {
		T mag = vec.SqrMagnitude();
		if (mag <= Cast(0.00001)) {
			vec.x = Cast(0); vec.y = Cast(0); vec.z = Cast(0);
			return;
		}
		vec.x /= mag; vec.y /= mag; vec.z /= mag;
	}

	typedef Vec3_T<float> Vec3;
	typedef Vec3_T<int> IntVec3;

	template<typename T>
	struct Vec4_T {

		static constexpr inline size_t size = 4;

		T x, y, z, w;

		constexpr inline Vec4_T(T x = Cast(0), T y = Cast(0), T z = Cast(0), T w = Cast(0)) noexcept : x(x), y(y), z(z), w(w) {}
		constexpr inline Vec4_T(const Vec3_T<T>& other, T w = Cast(0)) noexcept : x(other.x), y(other.y), z(other.z), w(w) {}

		static constexpr inline T Dot(const Vec4_T& a, const Vec4_T& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

		constexpr inline T SqrMagnitude() const noexcept { return x * x + y * y + z * z + w * w; }
		constexpr inline T Magnitude() const noexcept { return sqrt(SqrMagnitude()); }
		constexpr inline Vec4_T Normalized() const noexcept {
			T mag = SqrMagnitude();
			if (mag <= Cast(0.00001)) {
				return { Cast(0), Cast(0), Cast(0), Cast(0)};
			}
			mag = sqrtf(mag);
			return { x / mag, y / mag, z / mag, w / mag };
		}

		constexpr inline Vec4_T operator*(T scalar) const noexcept { return Vec4_T(x * scalar, y * scalar, z * scalar, w * scalar); }
		constexpr inline Vec4_T& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
		constexpr inline Vec4_T& operator+=(const Vec4_T& other) noexcept { x += other.x; y += other.y; z += other.z; w += other.w; return *this; }

		constexpr inline bool operator==(const Vec4_T& other) const noexcept = default;

		constexpr inline explicit operator Vec3_T<T>() const noexcept { return Vec3_T<T>(x, y, z); }
		constexpr inline explicit operator Vec2_T<T>() const noexcept { return Vec2_T<T>(x, y); }

		constexpr inline T& operator[](size_t index) const { assert(index < size); return ((T*)this)[index]; }
	};

	typedef Vec4_T<float> Vec4;
	typedef Vec4_T<int> IntVec4;

	template<typename T>
	struct Mat2_T {
		static inline constexpr size_t size = 2;
		Vec2_T<T> columns[2];
		constexpr inline Mat2_T() noexcept : columns() {}
		constexpr inline Mat2_T(T num) noexcept : columns { { num, Cast(0) }, { Cast(0), num } } {}
		constexpr inline Mat2_T(T num0, T num1, T num2, T num3) noexcept : columns { { num0, num1 }, { num2, num3 } } {}

		constexpr inline Vec2_T<T>& operator[](size_t index) { assert(index < size); return columns[index]; }
		constexpr inline const Vec2_T<T>& operator[](size_t index) const { assert(index < size); return columns[index]; }
	};

	template<typename T>
	constexpr inline T Determinant(const Mat2_T<T>& a) noexcept { return a[0][0] * a[1][1] - a[0][1] * a[1][0]; }

	typedef Mat2_T<float> Mat2;

	template<typename T>
	struct Mat4_T;

	template<typename T>
	struct Mat3_T {

		static constexpr inline size_t size = 3;

		Vec3_T<T> columns[3];

		constexpr inline Mat3_T(T num = Cast(0)) noexcept 
			: columns { { num, Cast(0), Cast(0) }, { Cast(0), num, Cast(0) }, { Cast(0), Cast(0), num} } {}

		constexpr inline Mat3_T(T n0, T n1, T n2, T n3, T n4, T n5, T n6, T n7, T n8) noexcept
			: columns { { n0, n1, n2 }, { n3, n4, n5 }, { n6, n7, n8 } } {}

		constexpr inline Mat3_T(const Vec3_T<T>& col1, const Vec3_T<T>& col2, const Vec3_T<T>& col3) 
			: columns { col1, col2, col3 } {}

		constexpr inline Mat3_T(const Mat4_T<T>& other) : columns { other[0], other[1], other[2] } {}

		static constexpr inline Vec3_T<T> Multiply(const Vec3_T<T>& a, const Mat3_T& b) noexcept {
			return Vec3_T<T>(
				a.x * b[0].x + a.y * b[0].y + a.z * b[0].z,
				a.x * b[1].x + a.y * b[1].y + a.z * b[1].z,
				a.x * b[2].x + a.y * b[2].y + a.z * b[2].z
			);
		}

		constexpr inline const Vec3_T<T>& operator[](size_t index) const { return columns[index]; }
		constexpr inline Vec3_T<T>& operator[](size_t index) { return columns[index]; }
	};

	template<typename T>
	static constexpr inline Mat3_T<T> Transpose(const Mat3_T<T>& a) noexcept {
		Mat3_T<T> result = a;
		Swap(result[0][2], result[2][0]);
		Swap(result[0][1], result[1][0]);
		Swap(result[1][2], result[2][1]);
		return result;
	}

	template<typename T>
	static constexpr inline Mat3_T<T> Inverse(const Mat3_T<T>& a) noexcept {
		Vec3_T<T> minors0 = Vec3_T<T>(
			Determinant(Mat2_T<T>(a[1][1], a[1][2], a[2][1], a[2][2])),
			Determinant(Mat2_T<T>(a[1][2], a[1][0], a[2][2], a[2][0])),
			Determinant(Mat2_T<T>(a[1][0], a[1][1], a[2][0], a[2][1]))
		);
		Vec3_T<T> minors1 = Vec3_T<T>(
			Determinant(Mat2_T<T>(a[2][1], a[2][2], a[0][1], a[0][2])),
			Determinant(Mat2_T<T>(a[2][2], a[2][0], a[0][2], a[0][0])),
			Determinant(Mat2_T<T>(a[2][0], a[2][1], a[0][0], a[0][1]))
		);
		Vec3_T<T> minors2 = Vec3_T<T>(
			Determinant(Mat2_T<T>(a[0][1], a[0][2], a[1][1], a[1][2])),
			Determinant(Mat2_T<T>(a[0][2], a[0][0], a[1][2], a[1][0])),
			Determinant(Mat2_T<T>(a[0][0], a[0][1], a[1][0], a[1][1]))
		);
		Mat3_T<T> result(minors0, minors1, minors2);
		return Transpose(result);
	}


	typedef Mat3_T<float> Mat3;

	template<typename T>
	struct Mat4_T {

		static constexpr inline size_t col_length = 4;

		Vec4_T<T> columns[4];

		constexpr inline Mat4_T() noexcept : columns() {}

		constexpr inline Mat4_T(T num) noexcept
			: columns{ { num, Cast(0), Cast(0), Cast(0) }, { Cast(0), num, Cast(0), Cast(0) }, 
				{ Cast(0), Cast(0), num, Cast(0) }, { Cast(0), Cast(0), Cast(0), num } } {}

		constexpr inline Mat4_T(T n0, T n1, T n2, T n3, T n4, T n5, T n6, T n7, T n8, T n9, T n10, T n11, T n12, T n13, T n14, T n15) noexcept
			: columns{ { n0, n1, n2, n3 }, { n4, n5, n6, n7 }, { n8, n9, n10, n11 }, { n12, n13, n14, n15 } } {}

		static constexpr inline Mat4_T Multiply(const Mat4_T a, const Mat4_T b) {

			const Vec4_T<T>& aCol0 = a[0];
			const Vec4_T<T>& aCol1 = a[1];
			const Vec4_T<T>& aCol2 = a[2];
			const Vec4_T<T>& aCol3 = a[3];

			const Vec4_T<T>& bCol0 = b[0];
			const Vec4_T<T>& bCol1 = b[1];
			const Vec4_T<T>& bCol2 = b[2];
			const Vec4_T<T>& bCol3 = b[3];

			Mat4_T<T> result;
			Vec4_T<T> col; 
			col =  aCol0 * bCol0.x;
			col += aCol1 * bCol0.y;
			col += aCol2 * bCol0.z;
			col += aCol3 * bCol0.w;
			result[0] = col;
			col =  aCol0 * bCol1.x;
			col += aCol1 * bCol1.y;
			col += aCol2 * bCol1.z;
			col += aCol3 * bCol1.w;
			result[1] = col;
			col =  aCol0 * bCol2.x;
			col += aCol1 * bCol2.y;
			col += aCol2 * bCol2.z;
			col += aCol3 * bCol2.w;
			result[2] = col;
			col =  aCol0 * bCol3.x;
			col += aCol1 * bCol3.y;
			col += aCol2 * bCol3.z;
			col += aCol3 * bCol3.w;
			result[3] = col;
			return result;
		}

		static constexpr inline Vec4_T<T> Multiply(const Mat4_T& a, const Vec4_T<T>& b) noexcept {
			return Vec4_T<T>(
				a[0][0] * b.x + a[1][0] * b.y + a[2][0] * b.z + a[3][0] * b.w,
				a[0][1] * b.x + a[1][1] * b.y + a[2][1] * b.z + a[3][1] * b.w,
				a[0][2] * b.x + a[1][2] * b.y + a[2][2] * b.z + a[3][2] * b.w,
				a[0][3] * b.x + a[1][3] * b.y + a[2][3] * b.z + a[3][3] * b.w
			);
		}

		static constexpr inline Vec4_T<T> Multiply(const Vec4_T<T>& a, const Mat4_T& b) noexcept {
			return Vec4_T<T>(
				a.x * b[0].x + a.y * b[0].y + a.z * b[0].z + a.w * b[0].w,
				a.x * b[1].x + a.y * b[1].y + a.z * b[1].z + a.w * b[1].w,
				a.x * b[2].x + a.y * b[2].y + a.z * b[2].z + a.w * b[2].w,
				a.x * b[3].x + a.y * b[3].y + a.z * b[3].z + a.w * b[3].w
			);
		}

		static constexpr inline T Determinant(const Mat4_T& a) noexcept {
			return
				a[0].x * a[1].y * a[2].z * a[3].w +
				a[0].x * a[1].z * a[2].w * a[3].y +
				a[0].x * a[1].w * a[2].y * a[3].z -
				a[0].x * a[1].w * a[2].z * a[3].y -
				a[0].x * a[1].z * a[2].y * a[3].w -
				a[0].x * a[1].y * a[2].w * a[3].z -
				a[0].y * a[1].x * a[2].z * a[3].w -
				a[0].z * a[1].x * a[2].w * a[3].y -
				a[0].w * a[1].x * a[2].y * a[3].z +
				a[0].w * a[1].x * a[2].z * a[3].y +
				a[0].z * a[1].x * a[2].y * a[3].w +
				a[0].y * a[1].x * a[2].w * a[3].z +
				a[0].y * a[1].z * a[2].x * a[3].w +
				a[0].z * a[1].w * a[2].x * a[3].y +
				a[0].w * a[1].y * a[2].x * a[3].z -
				a[0].w * a[1].z * a[2].x * a[3].y -
				a[0].z * a[1].y * a[2].x * a[3].w -
				a[0].y * a[1].w * a[2].x * a[3].z -
				a[0].y * a[1].z * a[2].w * a[3].x -
				a[0].z * a[1].w * a[2].y * a[3].x -
				a[0].w * a[1].y * a[2].z * a[3].x +
				a[0].w * a[1].z * a[2].y * a[3].x +
				a[0].z * a[1].y * a[2].w * a[3].x +
				a[0].y * a[1].w * a[2].z * a[3].x;
		}

		static constexpr inline Mat4_T LookAt(const Vec3_T<T>& eyePosition, const Vec3_T<T>& upDirection, const Vec3_T<T>& lookAtPosition) noexcept {
			Vec3_T<T> pos = eyePosition;
			pos.y = -pos.y;
			Vec3_T<T> front = (Vec3(lookAtPosition.x, -lookAtPosition.y, lookAtPosition.z) - pos).Normalized();
			Vec3_T<T> right = Cross(upDirection, front).Normalized();
			Vec3_T<T> up = Cross(front, right).Normalized();
			Mat4_T result{};
			result[0].x = right.x;
			result[1].x = right.y;
			result[2].x = right.z;
			result[0].y = up.x;
			result[1].y = up.y;
			result[2].y = up.z;
			result[0].z = front.x;
			result[1].z = front.y;
			result[2].z = front.z;
			pos = -pos;
			result[3] = {
				Dot(right, pos),
				Dot(up, pos),
				Dot(front, pos),
				Cast(1),
			};
			return result;
		}

		constexpr inline Vec3_T<T> LookAtFront() const {
			return Vec3_T<T>(-columns[0].z, columns[1].z, -columns[2].z);
		}

		static constexpr inline Mat4_T Projection(T radFovY, T aspectRatio, T zNear, T zFar) noexcept {
			T halfTan = tan(radFovY / 2);
			Mat4_T result(1);
			result[0][0] = 1 / (aspectRatio * halfTan);
			result[1][1] = 1 / halfTan;
			result[2][2] = (zFar - zNear) / (zFar + zNear);
			result[2][3] = 1;
			result[3][2] = (-2 * zFar * zNear) / (zFar + zNear);
			return result;
		}

		static constexpr inline Mat4_T Orthogonal(T leftPlane, T rightPlane, T bottomPlane, T topPlane, T nearPlane, T farPlane) noexcept {
			Mat4_T result(1);
			result[0][0] = Cast(2) / (rightPlane - leftPlane);
			result[1][1] = Cast(2) / (topPlane - bottomPlane);
			result[2][2] = Cast(2) / (nearPlane + farPlane);
			result[3][0] = - (rightPlane + leftPlane) / (rightPlane - leftPlane);
			result[3][1] = - (bottomPlane + topPlane) / (bottomPlane - topPlane);
			result[3][2] = nearPlane / (nearPlane + farPlane);
			result[3][3] = Cast(1);
			return result;
		}

		constexpr inline const Vec4_T<T>& operator[](size_t index) const { return columns[index]; }
		constexpr inline Vec4_T<T>& operator[](size_t index) { return columns[index]; }

		constexpr inline explicit operator Mat4_T() const noexcept {
			return Mat4_T((Vec3_T<T>)columns[0], (Vec3_T<T>)columns[1], (Vec3_T<T>)columns[2]);
		}

		friend constexpr inline Mat4_T<T> operator*(const Mat4_T<T>& a, const Mat4_T<T>& b) {
			return Multiply(a, b);
		}

		friend constexpr inline Vec4 operator*(const Mat4_T& a, const Vec4_T<T>& b) {
			return Multiply(a, b);
		}
	};

	template<typename T>
	constexpr inline Mat4_T<T> Transpose(const Mat4_T<T>& a) noexcept {
		Mat4_T result = a;
		Swap(result[0][1], result[1][0]);
		Swap(result[0][2], result[2][0]);
		Swap(result[0][3], result[3][0]);
		Swap(result[1][2], result[2][1]);
		Swap(result[3][1], result[1][3]);
		Swap(result[3][2], result[2][3]);
		return result;
	}

	template<typename T>
	constexpr inline Mat4_T<T> Inverse(const Mat4_T<T>& a) noexcept {

		Mat4_T<T> res;

		const float* pA = (float*)&a;
		float* pRes = (float*)&res;

		pRes[0] = 
				pA[5] * pA[10] * pA[15] -
				pA[5] * pA[11] * pA[14] -
				pA[9] * pA[6] * pA[15] +
				pA[9] * pA[7] * pA[14] +
				pA[13] * pA[6] * pA[11] -
				pA[13] * pA[7] * pA[10];

		pRes[1] = 
				-pA[1] * pA[10] * pA[15] +
				pA[1] * pA[11] * pA[14] +
				pA[9] * pA[2] * pA[15] -
				pA[9] *	pA[3] * pA[14] -
				pA[13] * pA[2] * pA[11] +
				pA[13] * pA[3] * pA[10];

		pRes[2] =
				pA[1] * pA[6] * pA[15] -
				pA[1] * pA[7] * pA[14] -
				pA[5] * pA[2] * pA[15] +
				pA[5] * pA[3] * pA[14] +
				pA[13] * pA[2] * pA[7] -
				pA[13] * pA[3] * pA[6];

		pRes[3] =
				-pA[1] * pA[6] * pA[11] +
				pA[1] * pA[7] * pA[10] +
				pA[5] * pA[2] * pA[11] -
				pA[5] * pA[3] * pA[10] -
				pA[9] * pA[2] * pA[7] +
				pA[9] * pA[3] * pA[6];

		pRes[4] =
				-pA[4] * pA[10] * pA[15] +
				pA[4] * pA[11] * pA[14] +
				pA[8] * pA[6] * pA[15] -
				pA[8] * pA[7] * pA[14] -
				pA[12] * pA[6] * pA[11] +
				pA[12] * pA[7] * pA[10];

		pRes[5] =
				pA[0] * pA[10] * pA[15] -
				pA[0] * pA[11] * pA[14] -
				pA[8] * pA[2] * pA[15] +
				pA[8] * pA[3] * pA[14] +
				pA[12] * pA[2] * pA[11] -
				pA[12] * pA[3] * pA[10];

		pRes[6] =
				-pA[0] * pA[6] * pA[15] +
				pA[0] * pA[7] * pA[14] +
				pA[4] * pA[2] * pA[15] -
				pA[4] * pA[3] * pA[14] -
				pA[12] * pA[2] * pA[7] +
				pA[12] * pA[3] * pA[6];

		pRes[7] = 
				pA[0] * pA[6] * pA[11] -
				pA[0] * pA[7] * pA[10] -
				pA[4] * pA[2] * pA[11] +
				pA[4] * pA[3] * pA[10] +
				pA[8] * pA[2] * pA[7] -
				pA[8] * pA[3] * pA[6];

		pRes[8] =
				pA[4] * pA[9] * pA[15] -
				pA[4] * pA[11] * pA[13] -
				pA[8] * pA[5] * pA[15] +
				pA[8] * pA[7] * pA[13] +
				pA[12] * pA[5] * pA[11] -
				pA[12] * pA[7] * pA[9];

		pRes[9] = 
				-pA[0] * pA[9] * pA[15] +
            	pA[0] * pA[11] * pA[13] +
				pA[8] * pA[1] * pA[15] -
				pA[8] * pA[3] * pA[13] -
				pA[12] * pA[1] * pA[11] +
				pA[12] * pA[3] * pA[9];

				
	    pRes[10] = 
				pA[0] * pA[5] * pA[15] -
				pA[0] * pA[7] * pA[13] -
				pA[4] * pA[1] * pA[15] +
				pA[4] * pA[3] * pA[13] +
				pA[12] * pA[1] * pA[7] -
				pA[12] * pA[3] * pA[5];

    	pRes[11] = 
				-pA[0] * pA[5] * pA[11] +
				pA[0] * pA[7] * pA[9] +
				pA[4] * pA[1] * pA[11] -
				pA[4] * pA[3] * pA[9] -
				pA[8] * pA[1] * pA[7] +
				pA[8] * pA[3] * pA[5]; 

    	pRes[12] = 
				-pA[4]  * pA[9] * pA[14] +
				pA[4]  * pA[10] * pA[13] +
				pA[8]  * pA[5] * pA[14] -
				pA[8]  * pA[6] * pA[13] -
				pA[12] * pA[5] * pA[10] +
				pA[12] * pA[6] * pA[9];


    	pRes[13] = 
				pA[0]  * pA[9] * pA[14] -
            	pA[0]  * pA[10] * pA[13] -
        		pA[8]  * pA[1] * pA[14] +
            	pA[8]  * pA[2] * pA[13] +
        		pA[12] * pA[1] * pA[10] -
        		pA[12] * pA[2] * pA[9];

    	pRes[14] = 
				-pA[0]  * pA[5] * pA[14] +
        		pA[0]  * pA[6] * pA[13] +
        		pA[4]  * pA[1] * pA[14] -
        		pA[4]  * pA[2] * pA[13] -
        		pA[12] * pA[1] * pA[6] +
        		pA[12] * pA[2] * pA[5];

    	pRes[15] = 
				pA[0] * pA[5] * pA[10] -
        		pA[0] * pA[6] * pA[9] -
        		pA[4] * pA[1] * pA[10] +
        		pA[4] * pA[2] * pA[9] +
        		pA[8] * pA[1] * pA[6] -
        		pA[8] * pA[2] * pA[5];

		float det = pA[0] * pRes[0] + pA[1] * pRes[4] + pA[2] * pRes[8] + pA[3] * pRes[12];

		if (det == 0.0f) {
			fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, 
				"attempting to invert non-invertible 4x4 matrix (in function engine::Invert)!");
			return Mat4_T<T>(1);
		}

		det = 1.0f / det;

		for (size_t i = 0; i < 16; i++) {
			pRes[i] *= det;
		}

		return res;
	}


	typedef Mat4_T<float> Mat4;

	template<typename T>
	struct Quaternion_T {

		static constexpr inline const Quaternion_T& Identity() noexcept { 
			static constexpr Quaternion_T identity = Quaternion_T(Cast(0), Cast(0), Cast(0), Cast(1));
			return identity;
		}

		T x, y, z, w;

		constexpr inline Quaternion_T() noexcept : x(Cast(0)), y(Cast(0)), z(Cast(0)), w(Cast(0)) {}
		constexpr inline Quaternion_T(T x, T y, T z, T w) noexcept : x(x), y(y), z(z), w(w) {}
		constexpr inline Quaternion_T(Vec4_T<T> other) noexcept : x(other.x), y(other.y), z(other.z), w(other.w) {}

		static constexpr inline T Dot(const Quaternion_T& a, const Quaternion_T& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

		static constexpr inline T AngleBetween(const Quaternion_T& a, const Quaternion_T& b) noexcept {
			return acos(fmin(fabs(Quaternion_T::Dot(a, b)), Cast(1))) * Cast(2);
		}

		static constexpr inline Quaternion_T Slerp(const Quaternion_T& from, const Quaternion_T& to, T t) noexcept {
			return Quaternion_T(from * (Cast(1) - t) + to * t).Normalized();
		}

		static constexpr inline Quaternion_T RotateTowards(const Quaternion_T& from, const Quaternion_T& to, T maxRadians) noexcept {
			T angle = AngleBetween(from, to);
			if (abs(angle) < Cast(0.00001)) {
				return to;
			}
			return Slerp(from, to, Clamp(maxRadians / angle, Cast(0), Cast(1)));
		}

		static constexpr inline Quaternion_T AxisRotation(const Vec3_T<T>& axis, T radians) noexcept {
			radians /= 2;
			Vec3_T<T> norm = axis.Normalized();
			T sine = sin(radians);
			return Quaternion_T(norm.x * sine, norm.y * sine, norm.z * sine, cos(radians)).Normalized();
		}

		static constexpr inline Quaternion_T Multiply(const Quaternion_T& a, const Quaternion_T& b) noexcept {
			return Quaternion_T(
				a.x * b.w + a.w * b.x - a.y * b.z + a.z * b.y,
				a.y * b.w - a.z * b.x + a.w * b.y + a.x * b.z,
				a.z * b.w + a.y * b.x - a.x * b.y + a.w * b.x,
				a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
			).Normalized();
		}

		static constexpr inline Quaternion_T RotationBetween(const Vec3_T<T>& a, const Vec3_T<T>& b) noexcept {
			T aLen = a.SqrMagnitude(), bLen = b.SqrMagnitude();
			if (aLen < Cast(0.00001) || bLen < Cast(0.00001)) {
				return { Cast(0), Cast(0), Cast(0), Cast(0) };
			}
			Vec3_T<T> abCross = Cross(a, b);
			return Quaternion_T(abCross.x, abCross.y, abCross.z, sqrt(aLen * bLen + Vec3_T<T>::Dot(a, b))).Normalized();
		}

		constexpr inline T SqrMagnitude() const noexcept { return x * x + y * y + z * z + w * w; }

		constexpr inline T Magnitude() const noexcept { return sqrt(SqrMagnitude()); }

		constexpr inline Quaternion_T Normalized() const noexcept {
			T mag = SqrMagnitude();
			if (fabs(mag) < Cast(0.00001)) {
				return { Cast(0), Cast(0), Cast(0), Cast(0) };
			}
			mag = sqrt(mag);
			return { x / mag, y / mag, z / mag, w / mag };
		}

		constexpr inline Mat3_T<T> AsMat3() const noexcept {
			T num1 = x * x;
			T num2 = y * y;
			T num3 = z * z;
			return Mat3_T<T>(
				Cast(1) - Cast(2) * (num2 + num3), Cast(2) * (x * y + z * w), Cast(2) * (x * z - y * w),
				Cast(2) * (x * y - z * w), Cast(1) - Cast(2) * (num1 + num3), Cast(2) * (y * z + x * w),
				Cast(2) * (x * z + y * w), Cast(2) * (y * z - x * w), Cast(1) - Cast(2) * (num1 + num2)
			);
		}

		constexpr inline Mat4_T<T> AsMat4() const noexcept {
			T num1 = x * x;
			T num2 = y * y;
			T num3 = z * z;
			return Mat4_T<T>(
				Cast(1) - Cast(2) * (num2 + num3), Cast(2) * (x * y + z * w), Cast(2) * (x * z - y * w), Cast(0),
				Cast(2) * (x * y - z * w), Cast(1) - Cast(2) * (num1 + num3), Cast(2) * (y * z + x * w), Cast(0),
				Cast(2) * (x * z + y * w), Cast(2) * (y * z - x * w), Cast(1) - Cast(2) * (num1 + num2), Cast(0),
				Cast(0), Cast(0), Cast(0), Cast(1)
			);
		}

		Quaternion_T operator+(const Quaternion_T& other) const noexcept { return Quaternion_T(x + other.x, y + other.y, z + other.z, w + other.w); }
		Quaternion_T operator*(T scalar) const noexcept { return Quaternion_T(x * scalar, y * scalar, z * scalar, w * scalar); }
		constexpr inline bool operator==(const Quaternion_T& other) const noexcept { return x == other.x && y == other.y && z == other.z && w == other.w; }
		
		constexpr inline explicit operator Vec4_T<T>() const noexcept {
			return Vec4_T<T>(x, y, z, w);
		}
	};

	typedef Quaternion_T<float> Quaternion;
}
