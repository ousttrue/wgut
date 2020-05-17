#pragma once
#include <array>
#include <limits>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

///
/// float algebra
///
/// TODO: use ROW vector & ROW major matrix
///
/// [COL vector] => x
/// R|T   X   X'
/// -+- * Y = Y'
/// 0|1   Z   Z'
/// #multiply order
/// root * parent * local * col vector => vector
///
/// [ROW vector]
///       R|0
/// XYZ * -+- = X'Y'Z'
///       T|1
/// #multiply order
/// row vector * local * parent * root => vector
///
/// [ROW major]
/// matrix { m11, m12, m13, ... }
///
/// [COL major] => x
/// matrix { m11, m21, m31, ... }
///
/// tradeoff
/// => get translation from matrix
/// => matrix apply vector function
/// => matrix conversion function(ex. quaterion to matrix, decompose... etc)
///
/// upload [ROW vector][ROW major] matrix to d3d constant buffer
/// mul(matrix, float4) [ROW vector usage]
/// OK
///
/// if
/// mul(float4, world) [COL vector usage]
/// world is inversed
///
namespace falg
{
using float2 = std::array<float, 2>;
using float3 = std::array<float, 3>;
using float4 = std::array<float, 4>;
using float16 = std::array<float, 16>;

const float PI = (float)M_PI;
const float TO_RADIANS = PI / 180.0f;

template <typename T, typename S>
inline const T &size_cast(const S &s)
{
    static_assert(sizeof(S) == sizeof(T), "must same size");
    return *((T *)&s);
}

template <typename T, typename S>
inline T &size_cast(S &s)
{
    static_assert(sizeof(S) == sizeof(T), "must same size");
    return *((T *)&s);
}

struct range
{
    const void *begin;
    const void *end;

    size_t size() const
    {
        return (uint8_t *)end - (uint8_t *)begin;
    }
};

template <typename T, typename S>
inline const T &vector_cast(const std::vector<S> &s)
{
    range r{s.data(), s.data() + s.size()};
    if (r.size() != sizeof(T))
    {
        throw;
    }
    return *(const T *)r.begin;
}

struct xyz
{
    float x;
    float y;
    float z;
};
struct xyzw
{
    float x;
    float y;
    float z;
    float w;
};
struct row_matrix
{
    float _11;
    float _12;
    float _13;
    float _14;
    float _21;
    float _22;
    float _23;
    float _24;
    float _31;
    float _32;
    float _33;
    float _34;
    float _41;
    float _42;
    float _43;
    float _44;
};

template <size_t MOD, size_t N>
struct mul4
{
    // SFINAE for MOD not 0
};
template <size_t N>
struct mul4<0, N>
{
    static const size_t value = N;
};
template <typename T>
struct float_array
{
    static const size_t length = mul4<sizeof(T) % 4, sizeof(T) / 4>::value;
    using type = std::array<float, length>;
    template <typename S>
    static const type &cast(const S &s)
    {
        return size_cast<type>(s);
    }
};

template <typename T>
inline bool Nearly(const T &lhs, const T &rhs)
{
    using ARRAY = float_array<T>;
    auto &l = ARRAY::cast(lhs);
    auto &r = ARRAY::cast(rhs);

    const auto EPSILON = 1e-5f;
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        if (abs(l[i] - r[i]) > EPSILON)
        {
            return false;
        }
    }
    return true;
}

template <typename T>
inline T MulScalar(const T &value, float f)
{
    using ARRAY = float_array<T>;
    auto v = ARRAY::cast(value);
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        v[i] *= f;
    }
    return size_cast<T>(v);
}

template <typename T>
inline T Add(const T &lhs, const T &rhs)
{
    using ARRAY = float_array<T>;
    auto &l = ARRAY::cast(lhs);
    auto &r = ARRAY::cast(rhs);

    ARRAY::type value;
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        value[i] = l[i] + r[i];
    }
    return size_cast<T>(value);
}

template <typename T>
inline T Sub(const T &lhs, const T &rhs)
{
    using ARRAY = float_array<T>;
    auto &l = ARRAY::cast(lhs);
    auto &r = ARRAY::cast(rhs);

    ARRAY::type value;
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        value[i] = l[i] - r[i];
    }
    return size_cast<T>(value);
}

template <typename T>
inline T EachMul(const T &lhs, const T &rhs)
{
    using ARRAY = float_array<T>;
    auto &l = ARRAY::cast(lhs);
    auto &r = ARRAY::cast(rhs);

    ARRAY::type value;
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        value[i] = l[i] * r[i];
    }
    return size_cast<T>(value);
}

template <typename T>
inline float Dot(const T &lhs, const T &rhs)
{
    using ARRAY = float_array<T>;
    auto &l = ARRAY::cast(lhs);
    auto &r = ARRAY::cast(rhs);

    float value = 0;
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        value += l[i] * r[i];
    }
    return value;
}

template <typename T>
inline float Length(const T &src)
{
    return std::sqrt(Dot(src, src));
}

template <typename T>
inline T Normalize(const T &src)
{
    using ARRAY = float_array<T>;
    auto value = ARRAY::cast(src);
    auto factor = 1.0f / Length(src);
    for (size_t i = 0; i < ARRAY::length; ++i)
    {
        value[i] *= factor;
    }
    return size_cast<T>(value);
}

template <typename T>
inline T Cross(const T &lhs, const T &rhs)
{
    auto &l = size_cast<xyz>(lhs);
    auto &r = size_cast<xyz>(rhs);

    xyz value{
        l.y * r.z - l.z * r.y,
        l.z * r.x - l.x * r.z,
        l.x * r.y - l.y * r.x,
    };
    return size_cast<T>(value);
}

inline float Dot4(const float *row, const float *col, int step = 1)
{
    auto i = 0;
    auto a = row[0] * col[i];
    i += step;
    auto b = row[1] * col[i];
    i += step;
    auto c = row[2] * col[i];
    i += step;
    auto d = row[3] * col[i];
    auto value = a + b + c + d;
    return value;
}

template <typename M, typename T>
float3 RowMatrixApplyPosition(const M &_m, const T &_t)
{
    auto m = size_cast<row_matrix>(_m);
    auto &t = size_cast<xyz>(_t);
    std::array<float, 4> v = {t.x, t.y, t.z, 1};
    return {
        Dot4(v.data(), &m._11, 4),
        Dot4(v.data(), &m._12, 4),
        Dot4(v.data(), &m._13, 4)
    };
}

//

inline std::array<float, 16> RowMatrixMul(const std::array<float, 16> &l, const std::array<float, 16> &r)
{
    auto _11 = Dot4(&l[0], &r[0], 4);
    auto _12 = Dot4(&l[0], &r[1], 4);
    auto _13 = Dot4(&l[0], &r[2], 4);
    auto _14 = Dot4(&l[0], &r[3], 4);
    auto _21 = Dot4(&l[4], &r[0], 4);
    auto _22 = Dot4(&l[4], &r[1], 4);
    auto _23 = Dot4(&l[4], &r[2], 4);
    auto _24 = Dot4(&l[4], &r[3], 4);
    auto _31 = Dot4(&l[8], &r[0], 4);
    auto _32 = Dot4(&l[8], &r[1], 4);
    auto _33 = Dot4(&l[8], &r[2], 4);
    auto _34 = Dot4(&l[8], &r[3], 4);
    auto _41 = Dot4(&l[12], &r[0], 4);
    auto _42 = Dot4(&l[12], &r[1], 4);
    auto _43 = Dot4(&l[12], &r[2], 4);
    auto _44 = Dot4(&l[12], &r[3], 4);

    return std::array<float, 16>{
        _11,
        _12,
        _13,
        _14,
        _21,
        _22,
        _23,
        _24,
        _31,
        _32,
        _33,
        _34,
        _41,
        _42,
        _43,
        _44,
    };
}
} // namespace falg

namespace std
{
inline falg::float16 operator*(const falg::float16 &lhs, const falg::float16 &rhs)
{
    return falg::RowMatrixMul(lhs, rhs);
}
} // namespace std

namespace falg
{

inline void Transpose(std::array<float, 16> &m)
{
    std::swap(m[1], m[4]);
    std::swap(m[2], m[8]);
    std::swap(m[3], m[12]);
    std::swap(m[6], m[9]);
    std::swap(m[7], m[13]);
    std::swap(m[11], m[14]);
}

inline std::array<float, 16> IdentityMatrix()
{
    return std::array<float, 16>{
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
    };
}

inline std::array<float, 16> YawMatrix(float yawRadians)
{
    auto ys = (float)sin(yawRadians);
    auto yc = (float)cos(yawRadians);
    std::array<float, 16> yaw = {
        yc,
        0,
        ys,
        0,

        0,
        1,
        0,
        0,

        -ys,
        0,
        yc,
        0,

        0,
        0,
        0,
        1,
    };
    return yaw;
}

inline std::array<float, 16> PitchMatrix(float pitchRadians)
{
    auto ps = (float)sin(pitchRadians);
    auto pc = (float)cos(pitchRadians);
    std::array<float, 16> pitch = {
        1,
        0,
        0,
        0,

        0,
        pc,
        ps,
        0,

        0,
        -ps,
        pc,
        0,

        0,
        0,
        0,
        1,
    };
    return pitch;
}

inline std::array<float, 16> TranslationMatrix(float x, float y, float z)
{
    std::array<float, 16> t = {
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        x,
        y,
        z,
        1,
    };
    return t;
}

inline double Cot(double value)
{
    return 1.0f / tan(value);
}

inline void PerspectiveRHGL(float projection[16], float fovYRadians, float aspectRatio, float zNear, float zFar)
{
    const float f = static_cast<float>(Cot(fovYRadians / 2.0));
    projection[0] = f / aspectRatio;
    projection[1] = 0.0f;
    projection[2] = 0.0f;
    projection[3] = 0.0f;

    projection[4] = 0.0f;
    projection[5] = f;
    projection[6] = 0.0f;
    projection[7] = 0.0f;

    projection[8] = 0.0f;
    projection[9] = 0.0f;
    projection[10] = (zNear + zFar) / (zNear - zFar);
    projection[11] = -1;

    projection[12] = 0.0f;
    projection[13] = 0.0f;
    projection[14] = (2 * zFar * zNear) / (zNear - zFar);
    projection[15] = 0.0f;
}

inline void PerspectiveRHDX(float projection[16], float fovYRadians, float aspectRatio, float zNear, float zFar)
{
    auto yScale = (float)Cot(fovYRadians / 2);
    auto xScale = yScale / aspectRatio;
    projection[0] = xScale;
    projection[1] = 0.0f;
    projection[2] = 0.0f;
    projection[3] = 0.0f;

    projection[4] = 0.0f;
    projection[5] = yScale;
    projection[6] = 0.0f;
    projection[7] = 0.0f;

    projection[8] = 0.0f;
    projection[9] = 0.0f;
    projection[10] = zFar / (zNear - zFar);
    projection[11] = -1;

    projection[12] = 0.0f;
    projection[13] = 0.0f;
    projection[14] = (zFar * zNear) / (zNear - zFar);
    projection[15] = 0.0f;
}

inline float4 QuaternionAxisAngle(const float3 &axis, float angle)
{
    auto angle_half = angle / 2;
    auto sin = std::sin(angle_half);
    auto cos = std::cos(angle_half);
    return {axis[0] * sin, axis[1] * sin, axis[2] * sin, cos};
}

// inline float4 QuaternionMul(const float4 &lhs, const float4 &rhs)
// {
//     return {lhs[0] * rhs[3] + lhs[3] * rhs[0] + lhs[1] * rhs[2] - lhs[2] * rhs[1],
//             lhs[1] * rhs[3] + lhs[3] * rhs[1] + lhs[2] * rhs[0] - lhs[0] * rhs[2],
//             lhs[2] * rhs[3] + lhs[3] * rhs[2] + lhs[0] * rhs[1] - lhs[1] * rhs[0],
//             lhs[3] * rhs[3] - lhs[0] * rhs[0] - lhs[1] * rhs[1] - lhs[2] * rhs[2]};
// }
template <typename T>
inline float4 QuaternionMul(const T &l, T r)
{
    if (Dot(l, r) < 0)
    {
        r = {-r[0], -r[1], -r[2], -r[3]};
    }
    const auto &lhs = size_cast<xyzw>(l);
    const auto &rhs = size_cast<xyzw>(r);
    return {rhs.x * lhs.w + rhs.w * lhs.x + rhs.y * lhs.z - rhs.z * lhs.y,
            rhs.y * lhs.w + rhs.w * lhs.y + rhs.z * lhs.x - rhs.x * lhs.z,
            rhs.z * lhs.w + rhs.w * lhs.z + rhs.x * lhs.y - rhs.y * lhs.x,
            rhs.w * lhs.w - rhs.x * lhs.x - rhs.y * lhs.y - rhs.z * lhs.z};
}
// inline float4 operator*(const float4 &lhs, const float4 &rhs)
// {
//     float ax = lhs[0];
//     float ay = lhs[1];
//     float az = lhs[2];
//     float aw = lhs[3];
//     float bx = rhs[0];
//     float by = rhs[1];
//     float bz = rhs[2];
//     float bw = rhs[3];
//     return {
//         ax * bw + aw * bx + ay * bz - az * by,
//         ay * bw + aw * by + az * bx - ax * bz,
//         az * bw + aw * bz + ax * by - ay * bx,
//         aw * bw - ax * bx - ay * by - az * bz,
//     };
// }

inline float4 QuaternionConjugate(const float4 &v)
{
    return {-v[0], -v[1], -v[2], v[3]};
}

inline float3 QuaternionXDir(const float4 &v)
{
    auto x = v[0];
    auto y = v[1];
    auto z = v[2];
    auto w = v[3];
    return {w * w + x * x - y * y - z * z, (x * y + z * w) * 2, (z * x - y * w) * 2};
}

inline float3 QuaternionYDir(const float4 &v)
{
    auto x = v[0];
    auto y = v[1];
    auto z = v[2];
    auto w = v[3];
    return {(x * y - z * w) * 2, w * w - x * x + y * y - z * z, (y * z + x * w) * 2};
}

inline float3 QuaternionZDir(const float4 &v)
{
    auto x = v[0];
    auto y = v[1];
    auto z = v[2];
    auto w = v[3];
    return {(z * x + y * w) * 2, (y * z - x * w) * 2, w * w - x * x - y * y + z * z};
}

inline std::array<float, 16> QuaternionMatrix(const float4 &q)
{
    auto x = QuaternionXDir(q);
    auto y = QuaternionYDir(q);
    auto z = QuaternionZDir(q);
    return {
        x[0], x[1], x[2], 0,
        y[0], y[1], y[2], 0,
        z[0], z[1], z[2], 0,
        0, 0, 0, 1};
}

inline std::array<float, 16> ScaleMatrix(const float3 &s)
{
    return {
        s[0], 0, 0, 0,
        0, s[1], 0, 0,
        0, 0, s[2], 0,
        0, 0, 0, 1};
}

inline std::array<float, 16> ScaleMatrix(float x, float y, float z)
{
    return ScaleMatrix({x, y, z});
}

inline float3 QuaternionRotateFloat3(const float4 &q, const float3 &v)
{
    auto x = QuaternionXDir(q);
    auto y = QuaternionYDir(q);
    auto z = QuaternionZDir(q);
    return Add(MulScalar(x, v[0]), Add(MulScalar(y, v[1]), MulScalar(z, v[2])));
}

struct Transform
{
    float3 translation{0, 0, 0};
    float4 rotation{0, 0, 0, 1};

    Transform operator*(const Transform &rhs) const
    {
        return {
            Add(QuaternionRotateFloat3(rhs.rotation, translation), rhs.translation),
            QuaternionMul(rotation, rhs.rotation)};
    }

    std::array<float, 16> RowMatrix() const
    {
        auto r = QuaternionMatrix(rotation);
        r[12] = translation[0];
        r[13] = translation[1];
        r[14] = translation[2];
        return r;
    }

    Transform Inverse() const
    {
        auto inv_r = QuaternionConjugate(rotation);
        auto inv_t = QuaternionRotateFloat3(inv_r, MulScalar(translation, -1));
        return {inv_t, inv_r};
    }

    float3 ApplyPosition(const float3 &v) const
    {
        return Add(QuaternionRotateFloat3(rotation, v), translation);
    }

    float3 ApplyDirection(const float3 &v) const
    {
        return QuaternionRotateFloat3(rotation, v);
    }
};

struct TRS
{
    union {
        Transform transform;
        struct
        {
            float3 translation;
            float4 rotation;
        };
    };
    float3 scale;

    TRS()
        : translation({0, 0, 0}), rotation({0, 0, 0, 1}), scale({1, 1, 1})
    {
    }

    TRS(const float3 &t, const float4 &r, const float3 &s)
        : translation(t), rotation(r), scale(s)
    {
    }

    std::array<float, 16> RowMatrix() const
    {
        return ScaleMatrix(scale) * transform.RowMatrix();
    }
};
static_assert(sizeof(TRS) == 40, "TRS");

template <typename T>
TRS RowMatrixDecompose(const T &_m)
{
    auto &m = size_cast<row_matrix>(_m);
    auto s1 = Length(float3{m._11, m._12, m._13});
    auto s2 = Length(float3{m._21, m._22, m._23});
    auto s3 = Length(float3{m._31, m._32, m._33});
    TRS trs({m._41, m._42, m._43}, RowMatrixToQuaternion(_m), {s1, s2, s3});
    return trs;
}

struct Ray
{
    float3 origin;
    float3 direction;

    Ray(const float3 &_origin, const float3 &_direction)
        : origin(_origin), direction(_direction)
    {
    }

    template <typename T>
    Ray(const T &t)
    {
        static_assert(sizeof(T) == sizeof(Ray), "Ray::Ray()");
        auto &ray = size_cast<Ray>(t);
        origin = ray.origin;
        direction = ray.direction;
    }

    Ray ToLocal(const falg::Transform &t) const
    {
        auto toLocal = t.Inverse();
        return {
            toLocal.ApplyPosition(origin),
            falg::QuaternionRotateFloat3(toLocal.rotation, direction)};
    }

    float3 SetT(float t) const
    {
        return Add(origin, MulScalar(direction, t));
    }

    Ray Transform(const falg::Transform &t) const
    {
        return {
            t.ApplyPosition(origin),
            t.ApplyDirection(direction),
        };
    }
};

struct Plane
{
    float3 normal;
    float3 pointOnPlane;

    Plane(const float3 &n, const float3 &point_on_plane)
        : normal(n), pointOnPlane(point_on_plane)
    {
    }
};

inline float operator>>(const Ray &ray, const Plane &plane)
{
    auto NV = Dot(plane.normal, ray.direction);
    if (NV == 0)
    {
        // not intersect
        return -std::numeric_limits<float>::infinity();
    }

    auto Q = Sub(plane.pointOnPlane, ray.origin);
    auto NQ = Dot(plane.normal, Q);
    return NQ / NV;
}

struct Triangle
{
    float3 v0;
    float3 v1;
    float3 v2;
};

// float intersect_ray_triangle(const ray &ray, const minalg::float3 &v0, const minalg::float3 &v1, const minalg::float3 &v2)
inline float operator>>(const Ray &ray, const Triangle &triangle)
{
    auto e1 = Sub(triangle.v1, triangle.v0);
    auto e2 = Sub(triangle.v2, triangle.v0);
    auto h = Cross(ray.direction, e2);
    auto a = Dot(e1, h);
    if (std::abs(a) == 0)
        return std::numeric_limits<float>::infinity();

    float f = 1 / a;
    auto s = Sub(ray.origin, triangle.v0);
    auto u = f * Dot(s, h);
    if (u < 0 || u > 1)
        return std::numeric_limits<float>::infinity();

    auto q = Cross(s, e1);
    auto v = f * Dot(ray.direction, q);
    if (v < 0 || u + v > 1)
        return std::numeric_limits<float>::infinity();

    auto t = f * Dot(e2, q);
    if (t < 0)
        return std::numeric_limits<float>::infinity();

    return t;
}

struct Matrix2x3
{
    float3 x;
    float3 y;

    float3 Apply(const float2 &value) const
    {
        return {
            x[0] * value[0] + y[0] * value[1],
            x[1] * value[0] + y[1] * value[1],
            x[2] * value[0] + y[2] * value[1],
        };
    }
};

struct m16
{
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
};

inline std::array<float, 3> MatrixToTranslation(const m16 &m)
{
    return {m._41, m._42, m._43};
}

template <typename T>
std::array<float, 3> MatrixToTranslation(const T &m)
{
    return MatrixToTranslation(size_cast<m16>(m));
}

inline std::array<float, 4> ColMatrixToQuaternion(const m16 &m)
{
    float x, y, z, w;
    float trace = m._11 + m._22 + m._33; // I removed + 1.0f; see discussion with Ethan
    if (trace > 0)
    { // I changed M_EPSILON to 0
        float s = 0.5f / sqrtf(trace + 1.0f);
        w = 0.25f / s;
        x = (m._32 - m._23) * s;
        y = (m._13 - m._31) * s;
        z = (m._21 - m._12) * s;
    }
    else
    {
        if (m._11 > m._22 && m._22 > m._33)
        {
            float s = 2.0f * sqrtf(1.0f + m._11 - m._22 - m._33);
            w = (m._32 - m._23) / s;
            x = 0.25f * s;
            y = (m._12 + m._21) / s;
            z = (m._13 + m._31) / s;
        }
        else if (m._22 > m._33)
        {
            float s = 2.0f * sqrtf(1.0f + m._22 - m._11 - m._33);
            w = (m._13 - m._31) / s;
            x = (m._12 + m._21) / s;
            y = 0.25f * s;
            z = (m._23 + m._32) / s;
        }
        else
        {
            float s = 2.0f * sqrtf(1.0f + m._33 - m._11 - m._22);
            w = (m._21 - m._12) / s;
            x = (m._13 + m._31) / s;
            y = (m._23 + m._32) / s;
            z = 0.25f * s;
        }
    }
    return {x, y, z, w};
}

template <typename T>
std::array<float, 4> ColMatrixToQuaternion(const T &m)
{
    return ColMatrixToQuaternion(size_cast<m16>(m));
}

template <typename T>
std::array<float, 4> RowMatrixToQuaternion(const T &m)
{
    return QuaternionConjugate(ColMatrixToQuaternion(size_cast<m16>(m)));
}

} // namespace falg

/////////////////////////////////////////////////////////////////////////////
// std
/////////////////////////////////////////////////////////////////////////////
namespace std
{

// for std::array
inline falg::float3 operator-(const falg::float3 &lhs)
{
    return {-lhs[0], -lhs[1], -lhs[2]};
}
inline falg::float3 operator+(const falg::float3 &lhs, const falg::float3 &rhs)
{
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}
inline falg::float3 &operator+=(falg::float3 &lhs, const falg::float3 &rhs)
{
    lhs[0] += rhs[0];
    lhs[1] += rhs[1];
    lhs[2] += rhs[2];
    return lhs;
}
inline falg::float3 operator-(const falg::float3 &lhs, const falg::float3 &rhs)
{
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}
inline falg::float3 operator*(const falg::float3 &lhs, float scalar)
{
    return {lhs[0] * scalar, lhs[1] * scalar, lhs[2] * scalar};
}

} // namespace std
