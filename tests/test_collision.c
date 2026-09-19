#include "collision.h"
#include "simulation.h"
#include "test_util.h"

int main(void)
{
    printf("test_collision\n");

    Body a, b;
    body_init(&a, "A", 4.0, 1.0, vec3_make(0.0, 0.0, 0.0), vec3_make(1.0, 0.0, 0.0), 8);
    body_init(&b, "B", 1.0, 0.5, vec3_make(1.4, 0.0, 0.0), vec3_make(-2.0, 0.0, 0.0), 8);

    CHECK(collision_detect(&a, &b) == 1, "overlap detected when d <= r1+r2");

    Body far1, far2;
    body_init(&far1, "A", 1.0, 0.5, vec3_zero(), vec3_zero(), 8);
    body_init(&far2, "B", 1.0, 0.5, vec3_make(2.0, 0.0, 0.0), vec3_zero(), 8);
    CHECK(collision_detect(&far1, &far2) == 0, "no collision when d > r1+r2");
    body_free(&far1); body_free(&far2);

    Vec3   p0    = vec3_add(vec3_scale(a.velocity, a.mass), vec3_scale(b.velocity, b.mass));
    double mass0 = a.mass + b.mass;

    CollisionEvent ev = {0};
    collision_merge(&a, &b, &ev, 0, 1);

    CHECK(ev.occurred == 1, "merge reports an event");
    CHECK(a.active == 1 && b.active == 0, "heavier body survives the merge");
    CHECK_NEAR(a.mass, mass0, 1e-12, "mass is conserved");

    Vec3 p1 = vec3_scale(a.velocity, a.mass);
    CHECK_NEAR(vec3_distance(p1, p0), 0.0, 1e-12, "linear momentum is conserved");
    CHECK_NEAR(a.velocity.x, (4.0 * 1.0 + 1.0 * -2.0) / 5.0, 1e-12,
               "merged velocity is the mass-weighted mean");
    CHECK_NEAR(a.position.x, (4.0 * 0.0 + 1.0 * 1.4) / 5.0, 1e-12,
               "merged position is mass weighted");
    CHECK_NEAR(a.radius, cbrt(1.0 + 0.125), 1e-12, "radius conserves volume");

    body_free(&a); body_free(&b);

    /* Extreme configuration: a tiny Earth orbit forces a real collision and
       the simulation must survive it. */
    Simulation sim;
    simulation_init(&sim);
    /* Earth starts inside the Star's radius. The timestep is reduced to match
       the very high orbital speed at that radius: a fixed step that lets a
       body move further than its own radius can step straight over a contact
       (see README, "Known limitations"). */
    sim.config.earthOrbitRadius = 0.004;
    sim.config.dt               = 1.0e-5;
    simulation_reset(&sim);

    double mass0sim = physics_total_mass(sim.bodies, sim.bodyCount);
    Vec3   psim0    = physics_total_momentum(sim.bodies, sim.bodyCount);

    for (int i = 0; i < 20000; ++i) simulation_step(&sim);

    CHECK(sim.lastCollision.occurred == 1, "extreme configuration produces a collision");
    CHECK(sim.bodies[SIM_EARTH].active == 0 || sim.bodies[SIM_STAR].active == 1,
          "collision handled without removing the Star");
    CHECK_NEAR(physics_total_mass(sim.bodies, sim.bodyCount), mass0sim, 1e-12,
               "mass conserved across the merge");
    CHECK_NEAR(vec3_distance(physics_total_momentum(sim.bodies, sim.bodyCount), psim0),
               0.0, 1e-9, "momentum conserved across the merge");
    for (size_t i = 0; i < sim.bodyCount; ++i)
        CHECK(vec3_is_finite(sim.bodies[i].position), "post-collision state is finite");

    simulation_free(&sim);
    TEST_REPORT("test_collision");
}
