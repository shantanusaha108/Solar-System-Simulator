/*
 * vector3d.h - double precision 3D vector used as the authoritative
 * physics state. Deliberately free of any raylib / cglm dependency so
 * that the physics engine can be compiled and tested headlessly.
 */
#ifndef VECTOR3D_H
#define VECTOR3D_H

#include <math.h>

typedef struct
{
    double x;
    double y;
    double z;
} Vec3;

/* Constructor. Named vec3_make (not vec3) so it cannot clash with cglm's
   `vec3` type in translation units that use both. */
Vec3   vec3_make(double x, double y, double z);
Vec3   vec3_zero(void);
Vec3   vec3_add(Vec3 a, Vec3 b);
Vec3   vec3_sub(Vec3 a, Vec3 b);
Vec3   vec3_scale(Vec3 a, double s);
Vec3   vec3_neg(Vec3 a);
double vec3_dot(Vec3 a, Vec3 b);
Vec3   vec3_cross(Vec3 a, Vec3 b);
double vec3_length_sq(Vec3 a);
double vec3_length(Vec3 a);
double vec3_distance(Vec3 a, Vec3 b);
/* Returns a zero vector when the input length is (numerically) zero. */
Vec3   vec3_normalize(Vec3 a);
/* True when every component is finite (no NaN / Inf). */
int    vec3_is_finite(Vec3 a);

#endif /* VECTOR3D_H */
