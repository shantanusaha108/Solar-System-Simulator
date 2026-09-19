#include "vector3d.h"

Vec3 vec3_make(double x, double y, double z)
{
    Vec3 v = { x, y, z };
    return v;
}

Vec3 vec3_zero(void)
{
    return vec3_make(0.0, 0.0, 0.0);
}

Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 vec3_scale(Vec3 a, double s)
{
    return vec3_make(a.x * s, a.y * s, a.z * s);
}

Vec3 vec3_neg(Vec3 a)
{
    return vec3_make(-a.x, -a.y, -a.z);
}

double vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return vec3_make(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

double vec3_length_sq(Vec3 a)
{
    return vec3_dot(a, a);
}

double vec3_length(Vec3 a)
{
    return sqrt(vec3_length_sq(a));
}

double vec3_distance(Vec3 a, Vec3 b)
{
    return vec3_length(vec3_sub(a, b));
}

Vec3 vec3_normalize(Vec3 a)
{
    double len = vec3_length(a);
    if (!(len > 0.0) || !isfinite(len)) return vec3_zero();
    return vec3_scale(a, 1.0 / len);
}

int vec3_is_finite(Vec3 a)
{
    return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
}
