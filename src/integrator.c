#include <math.h>

#include "integrator.h"
#include "physics.h"

void integrator_velocity_verlet(Body *bodies, size_t count, double dt)
{
    if (!bodies || count == 0 || !isfinite(dt) || dt == 0.0) return;

    /* Half kick + drift. */
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        bodies[i].velocity = vec3_add(bodies[i].velocity,
                                      vec3_scale(bodies[i].acceleration, 0.5 * dt));
        bodies[i].position = vec3_add(bodies[i].position,
                                      vec3_scale(bodies[i].velocity, dt));
    }

    physics_compute_accelerations(bodies, count);

    /* Second half kick with the new accelerations. */
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        bodies[i].velocity = vec3_add(bodies[i].velocity,
                                      vec3_scale(bodies[i].acceleration, 0.5 * dt));
    }
}

void integrator_symplectic_euler(Body *bodies, size_t count, double dt)
{
    if (!bodies || count == 0 || !isfinite(dt) || dt == 0.0) return;

    physics_compute_accelerations(bodies, count);
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        bodies[i].velocity = vec3_add(bodies[i].velocity,
                                      vec3_scale(bodies[i].acceleration, dt));
        bodies[i].position = vec3_add(bodies[i].position,
                                      vec3_scale(bodies[i].velocity, dt));
    }
}

void integrator_step(Body *bodies, size_t count, double dt, IntegratorType type)
{
    switch (type)
    {
        case INTEGRATOR_SYMPLECTIC_EULER:
            integrator_symplectic_euler(bodies, count, dt);
            break;
        case INTEGRATOR_VELOCITY_VERLET:
        default:
            integrator_velocity_verlet(bodies, count, dt);
            break;
    }
}
