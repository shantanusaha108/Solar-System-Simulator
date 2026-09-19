#include "physics.h"
#include "test_util.h"

static void make_pair(Body *bodies, double m1, double m2, double separation)
{
    body_init(&bodies[0], "A", m1, 0.0, vec3_zero(), vec3_zero(), 8);
    body_init(&bodies[1], "B", m2, 0.0, vec3_make(separation, 0.0, 0.0), vec3_zero(), 8);
}

int main(void)
{
    printf("test_gravity\n");

    Body bodies[3];
    make_pair(bodies, 2.0, 3.0, 4.0);
    physics_compute_accelerations(bodies, 2);

    /* F = G*m1*m2/r^2 = 1*2*3/16 = 0.375 */
    double f1 = bodies[0].mass * vec3_length(bodies[0].acceleration);
    double f2 = bodies[1].mass * vec3_length(bodies[1].acceleration);
    CHECK_NEAR(f1, 0.375, 1e-9, "force magnitude matches G*m1*m2/r^2");
    CHECK_NEAR(f1, f2, 1e-12, "equal and opposite forces (Newton's third law)");

    Vec3 net = vec3_add(vec3_scale(bodies[0].acceleration, bodies[0].mass),
                        vec3_scale(bodies[1].acceleration, bodies[1].mass));
    CHECK_NEAR(vec3_length(net), 0.0, 1e-12, "net internal force is zero");

    CHECK(bodies[0].acceleration.x > 0.0 && bodies[1].acceleration.x < 0.0,
          "bodies accelerate toward each other");
    CHECK_NEAR(bodies[0].acceleration.x, PHYS_G * 3.0 / 16.0, 1e-9,
               "acceleration equals G*m_other/r^2");
    CHECK_NEAR(bodies[0].acceleration.y, 0.0, 1e-15, "no off-axis acceleration");

    double aNear = bodies[0].acceleration.x;
    body_free(&bodies[0]); body_free(&bodies[1]);

    /* Inverse square: doubling the distance quarters the acceleration. */
    make_pair(bodies, 2.0, 3.0, 8.0);
    physics_compute_accelerations(bodies, 2);
    CHECK_NEAR(bodies[0].acceleration.x, aNear / 4.0, 1e-12,
               "inverse-square falloff over doubled distance");
    body_free(&bodies[0]); body_free(&bodies[1]);

    /* Zero-distance protection: coincident bodies must not produce NaN/Inf. */
    make_pair(bodies, 1.0, 1.0, 0.0);
    physics_compute_accelerations(bodies, 2);
    CHECK(vec3_is_finite(bodies[0].acceleration) &&
          vec3_is_finite(bodies[1].acceleration),
          "coincident bodies yield finite accelerations");
    body_free(&bodies[0]); body_free(&bodies[1]);

    /* Superposition across three bodies. */
    body_init(&bodies[0], "A", 1.0, 0.0, vec3_make(-1.0, 0.0, 0.0), vec3_zero(), 8);
    body_init(&bodies[1], "B", 1.0, 0.0, vec3_make( 0.0, 0.0, 0.0), vec3_zero(), 8);
    body_init(&bodies[2], "C", 1.0, 0.0, vec3_make( 1.0, 0.0, 0.0), vec3_zero(), 8);
    physics_compute_accelerations(bodies, 3);
    CHECK_NEAR(vec3_length(bodies[1].acceleration), 0.0, 1e-12,
               "symmetric neighbours cancel (superposition)");
    CHECK_NEAR(bodies[0].acceleration.x, PHYS_G * (1.0 / 1.0 + 1.0 / 4.0), 1e-9,
               "accelerations from both neighbours sum");

    /* Gravity multiplier scales only the pull exerted by the source body. */
    bodies[2].gravityMultiplier = 2.0;
    physics_compute_accelerations(bodies, 3);
    CHECK_NEAR(bodies[0].acceleration.x, PHYS_G * (1.0 + 2.0 * 0.25), 1e-9,
               "gravity multiplier scales the source body's pull");

    for (int i = 0; i < 3; ++i) body_free(&bodies[i]);

    TEST_REPORT("test_gravity");
}
