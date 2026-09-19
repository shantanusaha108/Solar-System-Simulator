/*
 * Phase 1 verification: the default system stays bound, conserves momentum
 * and energy to within numerical error, and genuinely translates through
 * space (which is what turns Earth's orbit into a stretched helix).
 */
#include "simulation.h"
#include "test_util.h"

static double specific_orbital_energy(const Body *central, const Body *sat)
{
    Vec3   rel = vec3_sub(sat->position, central->position);
    Vec3   vel = vec3_sub(sat->velocity, central->velocity);
    double r   = vec3_length(rel);
    return 0.5 * vec3_length_sq(vel) - PHYS_G * (central->mass + sat->mass) / r;
}

int main(void)
{
    printf("test_orbit\n");

    Simulation sim;
    simulation_init(&sim);

    const Body *star  = simulation_body(&sim, SIM_STAR);
    const Body *earth = simulation_body(&sim, SIM_EARTH);
    const Body *moon  = simulation_body(&sim, SIM_MOON);

    /* --- initial conditions --- */
    double r0 = vec3_distance(earth->position, star->position);
    CHECK_NEAR(r0, sim.config.earthOrbitRadius, 1e-9,
               "Earth starts at the configured orbit radius");

    Vec3 radial = vec3_normalize(vec3_sub(earth->position, star->position));
    Vec3 relVel = vec3_sub(earth->velocity, star->velocity);
    CHECK_NEAR(vec3_dot(radial, vec3_normalize(relVel)), 0.0, 1e-9,
               "Earth's initial velocity is tangential");

    CHECK_NEAR(vec3_length(relVel),
               physics_circular_orbit_speed(star->mass + earth->mass, r0), 1e-9,
               "Earth's initial speed is the circular orbit speed");

    Vec3 moonRel = vec3_sub(moon->velocity, earth->velocity);
    CHECK_NEAR(vec3_length(moonRel),
               physics_circular_orbit_speed(earth->mass + moon->mass,
                                            sim.config.moonOrbitRadius), 1e-9,
               "Moon's initial relative speed is its circular orbit speed");

    /* --- system translation --- */
    Vec3 vsys0 = simulation_system_velocity(&sim);
    CHECK_NEAR(vec3_distance(vsys0, sim.config.translation), 0.0, 1e-12,
               "centre-of-mass velocity equals the requested translation");

    Vec3   com0 = simulation_system_center(&sim);
    double e0   = physics_total_energy(sim.bodies, sim.bodyCount);
    Vec3   p0   = physics_total_momentum(sim.bodies, sim.bodyCount);

    CHECK(specific_orbital_energy(star, earth) < 0.0, "Earth starts bound");
    CHECK(specific_orbital_energy(earth, moon) < 0.0, "Moon starts bound");

    /* --- integrate ~5 Earth orbits (period = 2*pi) --- */
    const double target = 10.0 * 3.14159265358979323846;
    long steps = 0;
    while (sim.time < target)
    {
        simulation_step(&sim);
        steps++;
    }
    printf("  info : integrated %ld steps to t=%.3f\n", steps, sim.time);

    CHECK(vec3_is_finite(earth->position) && vec3_is_finite(moon->position),
          "state remains finite after 5 orbits");

    CHECK(specific_orbital_energy(star, earth) < 0.0, "Earth still bound after 5 orbits");
    CHECK(specific_orbital_energy(earth, moon) < 0.0, "Moon still bound after 5 orbits");

    double r = vec3_distance(earth->position, star->position);
    CHECK(fabs(r - r0) / r0 < 0.01, "Earth's orbit radius stays near-circular (<1%)");

    double rm = vec3_distance(moon->position, earth->position);
    CHECK(fabs(rm - sim.config.moonOrbitRadius) / sim.config.moonOrbitRadius < 0.05,
          "Moon stays near its orbit radius (<5%)");

    /* --- conservation --- */
    Vec3   p  = physics_total_momentum(sim.bodies, sim.bodyCount);
    double dp = vec3_distance(p, p0) / vec3_length(p0);
    CHECK(dp < 1e-10, "linear momentum conserved");

    double drift = fabs(simulation_energy_drift(&sim));
    printf("  info : relative energy drift = %.3e\n", drift);
    CHECK(drift < 1e-4, "total energy conserved within numerical error");
    CHECK_NEAR(physics_total_energy(sim.bodies, sim.bodyCount), e0,
               fabs(e0) * 1e-4, "total energy close to its initial value");

    /* --- the helix: orbit plus translation --- */
    Vec3   com = simulation_system_center(&sim);
    double travelled = vec3_distance(com, com0);
    CHECK(travelled > 1.0, "the whole system has translated through space");
    CHECK_NEAR(travelled, vec3_length(sim.config.translation) * sim.time,
               travelled * 1e-6, "system travels at the translation velocity");

    /* Earth's path must oscillate transversely while drifting along X. */
    size_t n = trail_count(&sim.bodies[SIM_EARTH].trail);
    CHECK(n > 100, "Earth trail recorded a long history");
    double minZ = 1e300, maxZ = -1e300, minX = 1e300, maxX = -1e300;
    for (size_t i = 0; i < n; ++i)
    {
        Vec3 pnt = trail_get(&sim.bodies[SIM_EARTH].trail, i);
        if (pnt.z < minZ) minZ = pnt.z;
        if (pnt.z > maxZ) maxZ = pnt.z;
        if (pnt.x < minX) minX = pnt.x;
        if (pnt.x > maxX) maxX = pnt.x;
    }
    CHECK(maxZ - minZ > 1.5, "Earth trail oscillates across the orbit (spring coils)");
    CHECK(maxX - minX > 4.0, "Earth trail is stretched along the direction of travel");

    /* --- reset determinism --- */
    simulation_reset(&sim);
    CHECK_NEAR(sim.time, 0.0, 1e-15, "reset restores simulation time");
    CHECK_NEAR(vec3_distance(sim.bodies[SIM_EARTH].position, com0), 0.0, 1.0,
               "reset restores the initial configuration");
    CHECK_NEAR(physics_total_energy(sim.bodies, sim.bodyCount), e0,
               fabs(e0) * 1e-12, "reset is deterministic");

    /* --- escape emerges from physics when the Star is weakened --- */
    simulation_set_mass(&sim, SIM_STAR, 0.02);
    for (int i = 0; i < 40000; ++i) simulation_step(&sim);
    CHECK(specific_orbital_energy(&sim.bodies[SIM_STAR], &sim.bodies[SIM_EARTH]) > 0.0,
          "Earth becomes unbound when the Star mass is reduced");

    simulation_free(&sim);
    TEST_REPORT("test_orbit");
}
