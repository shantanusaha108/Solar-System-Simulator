#include "test_util.h"
#include "vector3d.h"

int main(void)
{
    printf("test_vectors\n");

    Vec3 a = vec3_make(1.0, 2.0, 3.0);
    Vec3 b = vec3_make(-4.0, 0.5, 2.0);

    Vec3 sum = vec3_add(a, b);
    CHECK(sum.x == -3.0 && sum.y == 2.5 && sum.z == 5.0, "addition");

    Vec3 diff = vec3_sub(a, b);
    CHECK(diff.x == 5.0 && diff.y == 1.5 && diff.z == 1.0, "subtraction");

    Vec3 scaled = vec3_scale(a, 2.5);
    CHECK(scaled.x == 2.5 && scaled.y == 5.0 && scaled.z == 7.5, "scalar multiplication");

    CHECK_NEAR(vec3_length(vec3_make(3.0, 4.0, 0.0)), 5.0, 1e-12, "magnitude");
    CHECK_NEAR(vec3_length_sq(a), 14.0, 1e-12, "squared magnitude");
    CHECK_NEAR(vec3_distance(vec3_make(1, 0, 0), vec3_make(4, 4, 0)), 5.0, 1e-12, "distance");

    Vec3 n = vec3_normalize(vec3_make(0.0, 0.0, 7.0));
    CHECK_NEAR(vec3_length(n), 1.0, 1e-12, "normalization gives unit length");
    CHECK(n.z == 1.0, "normalization keeps direction");

    Vec3 nz = vec3_normalize(vec3_zero());
    CHECK(nz.x == 0.0 && nz.y == 0.0 && nz.z == 0.0, "normalizing zero is safe");

    CHECK_NEAR(vec3_dot(a, b), -4.0 + 1.0 + 6.0, 1e-12, "dot product");

    Vec3 c = vec3_cross(vec3_make(1, 0, 0), vec3_make(0, 1, 0));
    CHECK(c.x == 0.0 && c.y == 0.0 && c.z == 1.0, "cross product (x cross y = z)");
    CHECK_NEAR(vec3_dot(vec3_cross(a, b), a), 0.0, 1e-12, "cross product is orthogonal");

    CHECK(vec3_is_finite(a) == 1, "finite check accepts finite vectors");
    CHECK(vec3_is_finite(vec3_make(0.0, NAN, 0.0)) == 0, "finite check rejects NaN");

    TEST_REPORT("test_vectors");
}
