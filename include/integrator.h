/*
 * integrator.h - time integration. All stepping logic lives here so that the
 * scheme can be swapped without touching physics.c or the renderer.
 */
#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include <stddef.h>

#include "body.h"

typedef enum
{
    INTEGRATOR_VELOCITY_VERLET = 0,
    INTEGRATOR_SYMPLECTIC_EULER = 1
} IntegratorType;

/*
 * Velocity Verlet (symplectic, second order - the default):
 *     v(t+dt/2) = v(t) + a(t)*dt/2
 *     x(t+dt)   = x(t) + v(t+dt/2)*dt
 *     a(t+dt)   = A(x(t+dt))
 *     v(t+dt)   = v(t+dt/2) + a(t+dt)*dt/2
 *
 * Accelerations must be valid on entry; the function leaves a(t+dt) stored in
 * the bodies, so the caller only needs to seed them once (simulation_reset).
 */
void integrator_velocity_verlet(Body *bodies, size_t count, double dt);
void integrator_symplectic_euler(Body *bodies, size_t count, double dt);
void integrator_step(Body *bodies, size_t count, double dt, IntegratorType type);

#endif /* INTEGRATOR_H */
