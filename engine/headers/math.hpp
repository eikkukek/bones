#pragma once

#include "algorithm.hpp"
#include <cstdio>
#include <math.h>

namespace engine {

#define Cast(x) static_cast<T>(x)

	constexpr auto pi = 3.14159265358979323846;

	template<typename T>
	struct Vec4_T;

	template<typename T>
	struct Vec2_T {

		static constexpr inline size_t size = 2;

		T x, y;

		constexpr inline Vec2_T(T x = Cast(0), T y = Cast(0)) noexcept : x(x), y(y) {}

		constexpr inline T SqrMagnitude() const noexcept { return x * x + y * y; }
		constexpr inline float Magnitude() const noexcept { return sqrt(SqrMagnitude()); }
		constexpr inline Vec2_T Normalized() const noexcept { 
			T mag = SqrMagnitude();
			if (mag <= Cast(0.00001)) {
				return Vec2(Cast(0), Cast(0));
			}
			mag = sqrt(mag);
			return Vec2_T(x / mag, y / mag);
		}

		constexpr inline Vec2_T operator*(float scalar) const noexcept { return Vec2_T(x * scalar, y * scalar); }
		constexpr inline Vec2_T operator/(float scalar) const noexcept { return Vec2_T(x * scalar, y / scalar); }
		constexpr inline Vec2_T operator-() const noexcept { return Vec2_T(-x, -y); }
		constexpr inline Vec2_T operator+(Vec2_T other) const noexcept { return Vec2_T(x + other.x, y + other.y); }
		constexpr inline Vec2_T operator-(Vec2_T other) const noexcept { return Vec2_T(x - other.x, y - other.y); }
		constexpr inline Vec2_T& operator+=(Vec2_T other) noexcept { x += other.x; y += other.y; return *this; }
	};

	typedef Vec2_T<float> Vec2;
	typedef Vec2_T<double> DoubleVec2;
	typedef Vec2_T<int> IntVec2;

	template<typename T>
	struct Vec3_T {

		static constexpr inline size_t size = 3;

		T x, y, z;

		constexpr inline Vec3_T(T x = Cast(0), T y = Cast(0), float z = Cast(0)) noexcept : x(x), y(y), z(z) {}
		constexpr inline Vec3_T(const Vec2_T<T>& other) noexcept : x(other.x), y(other.y), z(Cast(0)) {}

		static constexpr inline float Dot(const Vec3_T& a, const Vec3_T& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
		static constexpr inline Vec3_T Cross(const Vec3_T& a, const Vec3_T& b) noexcept { return Vec3_T(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
		static constexpr inline void Normalize(Vec3_T& vec) noexcept {
			float mag = vec.SqrMagnitude();
			if (mag <= Cast(0.00001)) {
				vec.x = Cast(0); vec.y = Cast(0); vec.z = Cast(0);
				return;
			}
			vec.x /= mag; vec.y /= mag; vec.z /= mag;
		}

		constexpr inline float SqrMagnitude() const noexcept { return x * x + y * y + z * z; }
		constexpr inline float Magnitude() const noexcept { return sqrt(SqrMagnitude()); }
		constexpr inline Vec3_T Normalized() const noexcept {
			float mag = SqrMagnitude();
			if (mag < Cast(0.00001f)) {
				return Vec3_T(Cast(0), Cast(0), Cast(0));
			}
			mag = sqrt(mag);
			return Vec3_T(x / mag, y / mag, z / mag);
		}

		constexpr inline Vec3_T operator*(float scalar) const noexcept { return Vec3_T(x * scalar, y * scalar, z * scalar); }
		constexpr inline Vec3_T operator/(float scalar) const noexcept { return Vec3_T(x / scalar, y / scalar, z / scalar); }
		constexpr inline Vec3_T operator-() const noexcept { return Vec3_T(-x, -y, -z); }
		constexpr inline Vec3_T operator+(const Vec3_T& other) const noexcept { return Vec3_T(x + other.x, y + other.y, z + other.z); }
		constexpr inline Vec3_T operator-(const Vec3_T& other) const noexcept { return Vec3_T(x - other.x, y - other.y, z - other.z); }
		constexpr inline Vec3_T& operator+=(const Vec3_T& other) noexcept { x += other.x; y += other.y; z += other.z; return *this; }
		constexpr inline Vec3_T& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }

		constexpr inline bool operator==(const Vec3_T& other) const noexcept { return x == other.x && y == other.y && z == other.z; }

		constexpr inline T& operator[](size_t index) { if (index >= size) { printf("index out of bounds (Vec3_T operator[])"); return T(); } return (T*)&x; }

		constexpr explicit operator Vec4_T<T>() const noexcept;
		constexpr inline explicit operator Vec2_T<T>() const noexcept { return Vec2_T<T>(x, y); }
	};

	typedef Vec3_T<float> Vec3;
	typedef Vec3_T<double> DoubleVec3;
	typedef Vec3_T<int> IntVec3;

	template<typename T>
	struct Vec4_T {

		static constexpr inline size_t size = 4;

		T x, y, z, w;

		constexpr inline Vec4_T(float x = Cast(0), float y = Cast(0), float z = Cast(0), float w = Cast(0)) noexcept : x(x), y(y), z(z), w(w) {}
		constexpr inline Vec4_T(const Vec3_T<T>& other, float w = Cast(0)) noexcept : x(other.x), y(other.y), z(other.z), w(w) {}

		static constexpr inline T Dot(const Vec4_T& a, const Vec4_T& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

		constexpr inline float SqrMagnitude() const noexcept { return x * x + y * y + z * z + w * w; }
		constexpr inline float Magnitude() const noexcept { return sqrt(SqrMagnitude()); }
		constexpr inline Vec4_T Normalized() const noexcept {
			float mag = SqrMagnitude();
			if (mag <= Cast(0.00001)) {
				return { Cast(0), Cast(0), Cast(0), Cast(0)};
			}
			mag = sqrtf(mag);
			return { x / mag, y / mag, z / mag, w / mag };
		}

		constexpr inline Vec4_T operator*(T scalar) const noexcept { return Vec4_T(x * scalar, y * scalar, z * scalar, w * scalar); }
		constexpr inline Vec4_T& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }

		constexpr inline explicit operator Vec3_T<T>() const noexcept { return Vec3_T<T>(x, y, z); }
		constexpr inline explicit operator Vec2_T<T>() const noexcept { return Vec2_T<T>(x, y); }
	};

	typedef Vec4_T<float> Vec4;
	typedef Vec4_T<double> DoubleVec4;
	typedef Vec4_T<int> IntVec4;

	template<typename T>
	struct Mat2_T {
		static inline constexpr size_t size = 2;
		Vec2_T<T> columns[2];
		constexpr inline Mat2_T() noexcept : columns() {}
		constexpr inline Mat2_T(T num) noexcept : columns { { num, Cast(0) }, { Cast(0), num } } {}
		constexpr inline Mat2_T(T num0, T num1, T num2, T num3) noexcept : columns { { num0, num1 }, { num2, num3 } } {}
		static constexpr inline T Determinant(const Mat2_T& a) noexcept { return a[0][0] * a[1][1] - a[0][1] * a[1][0]; }
		constexpr inline Vec2_T<T>& operator[](size_t index) { if (index >= size) { printf("index out of bounds (Mat2_T opertator[])"); return Vec2_T<T>(); }; return columns[index]; }
	};

	typedef Mat2_T<float> Mat2;
	typedef Mat2_T<double> DoubleMat2;

	template<typename T>
	struct Mat3_T {

		static constexpr inline size_t size = 3;

		Vec3_T<T> columns[3];

		constexpr inline Mat3_T() noexcept : columns{ {  } } {}
		constexpr inline Mat3_T(T n0, T n1, T n2, T n3, T n4, T n5, T n6, T n7, T n8) noexcept
			: columns{ { n0, n1, n2 }, { n3, n4, n5 }, { n6, n7, n8 } } {}

		static constexpr inline Mat3_T& Transpose(Mat3_T& a) noexcept {
			Swap(a[0][2], a[2][0]);
			Swap(a[0][1], a[1][0]);
			Swap(a[1][2], a[2][1]);
			return a;
		}
		static constexpr inline Mat3_T& Inverse(Mat3_T& a) noexcept {
			Vec3_T<T> minors0 = Vec3_T<T>(
				Mat2_T<T>::Determinant(Mat2_T<T>(a[1][1], a[1][2], a[2][1], a[2][2])),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[1][2], a[1][0], a[2][2], a[2][0])),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[1][0], a[1][1], a[2][0], a[2][1]))
			);
			Vec3_T minors1 = Vec3_T<T>(
				Mat2_T<T>::Determinant(Mat2_T<T>(a[2].y, a[2].z, a[0].y, a[0].z)),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[2].z, a[2].x, a[0].z, a[0].x)),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[2].x, a[2].y, a[0].x, a[0].y))
			);
			Vec3_T minors2 = Vec3_T(
				Mat2_T<T>::Determinant(Mat2_T<T>(a[0].y, a[0].z, a[1].y, a[1].z)),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[0].z, a[0].x, a[1].z, a[1].x)),
				Mat2_T<T>::Determinant(Mat2_T<T>(a[0].x, a[0].y, a[1].x, a[1].y))
			);
			a = { minors0, minors1, minors2 };
			return Transpose(a);
		}
	};

	typedef Mat3_T<float> Mat3;
	typedef Mat3_T<double> DoubleMat3;

	template<typename T>
	struct Mat4_T {

		static constexpr inline size_t size = 4;

		Vec4_T<T> columns[4];

		constexpr inline Mat4_T() noexcept : columns() {}
		constexpr inline Mat4_T(T num) noexcept
			: columns{ { num, Cast(0), Cast(0), Cast(0) }, { Cast(0), num, Cast(0), Cast(0) }, { Cast(0), Cast(0), num, Cast(0) }, { Cast(0), Cast(0), Cast(0), num } } {}
		constexpr inline Mat4_T(T n0, T n1, T n2, T n3, T n4, T n5, T n6, T n7, T n8, T n9, T n10, T n11, T n12, T n13, T n14, T n15) noexcept
			: columns{ { n0, n1, n2, n3 }, { n4, n5, n6, n7 }, { n8, n9, n10, n11 }, { n12, n13, n14, n15 } } {}

		static constexpr inline Mat4_T Multiply(const Mat4_T& a, const Mat4_T& b) noexcept {
			Mat4_T result{};
			float (*aF)[4] = (float(*)[4])&a;
			float (*bF)[4] = (float(*)[4])&b;
			float (*resF)[4] = (float(*)[4])&result;
			for (int i = 0; i < 4; i++) {
				float num = 0.0f;
				for (int j = 0; j < 4; j++) {
					for (int k = 0; k < 4; k++) {
						num += aF[k][j] * bF[i][k];
					}
					resF[i][j] += num;
				}
			}
			return result;
		}

		static constexpr inline Vec4_T<T> Multiply(const Mat4_T& a, const Vec4_T<T>& b) noexcept {
			return Vec4_T<T>(
				a[0].x * b.x + a[1].x * b.y + a[2][0] * b.z + a[3][0] * b.w,
				a[0].y * b.x + a[1].y * b.y + a[2][1] * b.z + a[3][1] * b.w,
				a[0].z * b.x + a[1].z * b.y + a[2][2] * b.z + a[3][2] * b.w,
				a[0].w * b.x + a[1].w * b.y + a[2][3] * b.z + a[3][3] * b.w
			);
		}

		static constexpr inline Vec4_T<T> Multiply(const Vec4_T<T>& a, const Mat4_T& b) noexcept {
			return Vec4_T<T>(
				a.x * b[0].x + a.y * b[0].y + a.z * b[0].z + a.w * b[0].w,
				a.x * b[1].x + a.y * b[1].y + a.z * b[1].z + a.w * b[1].w,
				a.x * b[2].x + a.y * b[2].y + a.z * b[2].z + a.w * b[2].w,
				a.x * b[3].x + a.y * b[3].y + a.z * b[3].z + a.w * b[3].w);
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
			Vec3_T<T> front = (lookAtPosition - pos).Normalized();
			Vec3_T<T> right = Vec3_T<T>::Cross(upDirection.Normalized(), front).Normalized();
			Vec3_T<T> up = Vec3_T<T>::Cross(front, right).Normalized();
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
				Vec3_T<T>::Dot(right, pos),
				Vec3_T<T>::Dot(up, pos),
				Vec3_T<T>::Dot(front, pos),
				1.0f
			};
			return result;
		}

		static constexpr inline Mat4_T Projection(T radFovY, T aspectRatio, T zNear, T zFar) noexcept {
			T halfTan = tan(radFovY / 2);
			Mat4_T result{};
			result[0].x = 1 / (aspectRatio * halfTan);
			result[1].y = 1 / halfTan;
			result[2].z = (zFar + zNear) / (zFar - zNear);
			result[2].w = 1;
			result[3].z = (2 * zFar * zNear) / (zFar - zNear);
			return result;
		}

		static constexpr inline Mat4_T Orthogonal(T leftPlane, T rightPlane, T bottomPlane, T topPlane, T nearPlane, T farPlane) noexcept {
			Mat4_T result{};
			result[0].x = Cast(2) / (rightPlane - leftPlane);
			result[1].y = Cast(2) / (topPlane - bottomPlane);
			result[2].z = Cast(2) / (nearPlane + farPlane);
			result[3].x = -(rightPlane + leftPlane) / (rightPlane - leftPlane);
			result[3].y = -(bottomPlane + topPlane) / (bottomPlane - topPlane);
			result[3].z = nearPlane / (nearPlane + farPlane);
			return result;
		}

		static constexpr inline Mat4_T Transpose(const Mat4_T& a) noexcept {
			Mat4_T result = a;
			Swap(result[0].y, result[1].x);
			Swap(result[0].z, result[2].x);
			Swap(result[0].w, result[3].x);
			Swap(result[1].z, result[3].y);
			Swap(result[3].y, result[1].w);
			Swap(result[3].z, result[2].w);
			return result;
		}

		constexpr inline Vec4_T<T>& operator[](size_t index) { if (index >= size) { printf("index out of bounds (Mat4_T operator[])"); return Vec4_T<T>(); } return columns[index]; }

		constexpr inline explicit operator Mat4_T() const noexcept {
			return Mat4_T((Vec3_T<T>)columns[0], (Vec3_T<T>)columns[1], (Vec3_T<T>)columns[2]);
		}
	};

	typedef Mat4_T<float> Mat4;
	typedef Mat4_T<double> DoubleMat4;

	template<typename T>
	struct Quaternion_T {

		T x, y, z, w;

		constexpr inline Quaternion_T() noexcept : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
		constexpr inline Quaternion_T(float x, float y, float z, float w) noexcept : x(x), y(y), z(z), w(w) {}
		constexpr inline Quaternion_T(Vec4_T<T> other) noexcept : x(other.x), y(other.y), z(other.z), w(other.w) {}

		static constexpr inline T Dot(const Quaternion_T& a, const Quaternion_T& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
		static constexpr inline T AngleBetween(const Quaternion_T& a, const Quaternion_T& b) noexcept {
			return acos(fmin(fabs(Quaternion_T::Dot(a, b)), Cast(1))) * Cast(2);
		}
		static constexpr inline Quaternion_T Slerp(const Quaternion_T& from, const Quaternion_T& to, float t) noexcept {
			return Quaternion_T(from * (Cast(1) - t) + to * t).Normalized();
		}
		static constexpr inline Quaternion_T RotateTowards(const Quaternion_T& from, const Quaternion_T& to, float maxRadians) noexcept {
			T angle = AngleBetween(from, to);
			if (abs(angle) < Cast(0.00001)) {
				return to;
			}
			return Slerp(from, to, Clamp(maxRadians / angle, 0.0f, 1.0f));
		}
		static constexpr inline Quaternion_T AxisRotation(const Vec3_T<T>& axis, float radians) noexcept {
			radians /= 2;
			Vec3_T<T> norm = axis.Normalized();
			float sine = sin(radians);
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
			float aLen = a.SqrMagnitude(), bLen = b.SqrMagnitude();
			if (aLen < 0.00001f || bLen < 0.00001f) {
				return { 0, 0, 0, 0 };
			}
			Vec3_T<T> abCross = Vec3_T<T>::Cross(a, b);
			return Quaternion_T(abCross.x, abCross.y, abCross.z, sqrt(aLen * bLen + Vec3_T<T>::Dot(a, b))).Normalized();
		}

		constexpr inline float SqrMagnitude() const noexcept { return x * x + y * y + z * z + w * w; }
		constexpr inline float Magnitude() const noexcept { return sqrt(SqrMagnitude()); }
		constexpr inline Quaternion_T Normalized() const noexcept {
			float mag = SqrMagnitude();
			if (fabs(mag) < 0.00001f) {
				return { 0.0f, 0.0f, 0.0f, 0.0f };
			}
			mag = sqrt(mag);
			return { x / mag, y / mag, z / mag, w / mag };
		}
		constexpr inline Mat4_T<T> AsMat4() const noexcept {
			float num1 = x * x;
			float num2 = y * y;
			float num3 = z * z;
			return Mat4_T<T>(
				Cast(1) - Cast(2) * (num2 + num3), Cast(2) * (x * y + z * w), Cast(2) * (x * z - y * w), Cast(0),
				Cast(2) * (x * y - z * w), Cast(1) - Cast(2) * (num1 + num3), Cast(2) * (y * z + x * w), Cast(0),
				Cast(2) * (x * z + y * w), Cast(2) * (y * z - x * w), Cast(1) - Cast(2) * (num1 + num2), Cast(0),
				Cast(0), Cast(0), Cast(0), Cast(1)
			);
		}

		static constexpr inline Quaternion_T Identity() noexcept { return Quaternion_T(); }

		Quaternion_T operator+(const Quaternion_T& other) const noexcept { return Quaternion_T(x + other.x, y + other.y, z + other.z, w + other.w); }
		Quaternion_T operator*(float scalar) const noexcept { return Quaternion_T(x * scalar, y * scalar, z * scalar, w * scalar); }
		constexpr inline bool operator==(const Quaternion_T& other) const noexcept { return x == other.x && y == other.y && z == other.z && w == other.w; }
		
		constexpr inline explicit operator Vec4_T<T>() const noexcept {
			return Vec4_T<T>(x, y, z, w);
		}
	};

	typedef Quaternion_T<float> Quaternion;
	typedef Quaternion_T<double> DoubleQuaternion;
}
