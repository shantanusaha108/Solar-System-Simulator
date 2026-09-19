#include <math.h>

#include "physics.h"

void physics_compute_accelerations(Body *bodies, size_t count)
{
    if (!bodies) return;

    for (size_t i = 0; i < count; ++i) bodies[i].acceleration = vec3_zero();

    /* O(N^2): every unordered pair is visited once (3 pairs for 3 bodies). */
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;

        for (size_t j = i + 1; j < count; ++j)
        {
            if (!bodies[j].active) continue;

            Vec3   d   = vec3_sub(bodies[j].position, bodies[i].position);
            double r2  = vec3_length_sq(d) + PHYS_SOFTENING * PHYS_SOFTENING;
            if (!isfinite(r2) || r2 <= 0.0) continue;

            double invR3 = 1.0 / (r2 * sqrt(r2));
            if (!isfinite(invR3)) continue;

            /* Symmetric coupling: the pair's whole interaction is scaled by
               g_i*g_j (same factor for both bodies), so momentum is
               conserved for any combination of multipliers - see physics.h. */
            double coupling = bodies[i].gravityMultiplier * bodies[j].gravityMultiplier;
            double ki = PHYS_G * coupling * bodies[j].mass * invR3;
            double kj = PHYS_G * coupling * bodies[i].mass * invR3;

            bodies[i].acceleration = vec3_add(bodies[i].acceleration, vec3_scale(d,  ki));
            bodies[j].acceleration = vec3_add(bodies[j].acceleration, vec3_scale(d, -kj));
        }
    }

    for (size_t i = 0; i < count; ++i)
    {
        if (!vec3_is_finite(bodies[i].acceleration))
            bodies[i].acceleration = vec3_zero();
    }
}

double physics_kinetic_energy(const Body *bodies, size_t count)
{
    double k = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        k += 0.5 * bodies[i].mass * vec3_length_sq(bodies[i].velocity);
    }
    return k;
}

double physics_potential_energy(const Body *bodies, size_t count)
{
    double u = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        for (size_t j = i + 1; j < count; ++j)
        {
            if (!bodies[j].active) continue;
            double r = vec3_distance(bodies[i].position, bodies[j].position);
            if (r < PHYS_SOFTENING) r = PHYS_SOFTENING;
            u -= PHYS_G * bodies[i].mass * bodies[j].mass / r;
        }
    }
    return u;
}

double physics_total_energy(const Body *bodies, size_t count)
{
    return physics_kinetic_energy(bodies, count) +
           physics_potential_energy(bodies, count);
}

Vec3 physics_total_momentum(const Body *bodies, size_t count)
{
    Vec3 p = vec3_zero();
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        p = vec3_add(p, vec3_scale(bodies[i].velocity, bodies[i].mass));
    }
    return p;
}

double physics_total_mass(const Body *bodies, size_t count)
{
    double m = 0.0;
    for (size_t i = 0; i < count; ++i)
        if (bodies[i].active) m += bodies[i].mass;
    return m;
}

Vec3 physics_center_of_mass(const Body *bodies, size_t count)
{
    double m = physics_total_mass(bodies, count);
    if (!(m > 0.0)) return vec3_zero();

    Vec3 c = vec3_zero();
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        c = vec3_add(c, vec3_scale(bodies[i].position, bodies[i].mass));
    }
    return vec3_scale(c, 1.0 / m);
}

Vec3 physics_center_of_mass_velocity(const Body *bodies, size_t count)
{
    double m = physics_total_mass(bodies, count);
    if (!(m > 0.0)) return vec3_zero();
    return vec3_scale(physics_total_momentum(bodies, count), 1.0 / m);
}

void physics_remove_com_velocity(Body *bodies, size_t count)
{
    Vec3 vcm = physics_center_of_mass_velocity(bodies, count);
    physics_add_common_velocity(bodies, count, vec3_neg(vcm));
}

void physics_add_common_velocity(Body *bodies, size_t count, Vec3 v)
{
    if (!bodies || !vec3_is_finite(v)) return;
    for (size_t i = 0; i < count; ++i)
        bodies[i].velocity = vec3_add(bodies[i].velocity, v);
}

double physics_circular_orbit_speed(double centralMass, double radius)
{
    if (!(centralMass > 0.0) || !(radius > 0.0)) return 0.0;
    double v = sqrt(PHYS_G * centralMass / radius);
    return isfinite(v) ? v : 0.0;
}
