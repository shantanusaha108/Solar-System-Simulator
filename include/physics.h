/*
 * physics.h - Newtonian gravity in normalised simulation units.
 *
 * UNITS
 *   length : 1.0 == 1 AU-equivalent
 *   mass   : 1.0 == default Star mass
 *   time   : chosen so that G == 1, hence a 1 AU circular orbit around a
 *            unit-mass star has speed 1 and period 2*pi.
 *
 * GRAVITY MULTIPLIER (experimental, non-physical)
 *   In real Newtonian mechanics mass alone sets gravitational influence:
 *       F = G*m1*m2 / r^2
 *   There is no ordinary physical concept of a "gravity slider" separate
 *   from mass and G, so this is explicitly a sandbox parameter, not physics,
 *   and the UI labels it that way. Mass and gravity are NOT decoupled:
 *   changing a body's mass always changes its gravitational influence,
 *   independently of this multiplier.
 *
 *   The multiplier is applied as a SYMMETRIC coupling on each pair:
 *       a_i = sum_j  G * g_i * g_j * m_j / r^3 * (p_j - p_i)
 *   i.e. the pair's whole interaction is scaled by g_i*g_j, the same factor
 *   for both bodies of the pair. With every multiplier at 1.0 the model is
 *   exactly Newtonian. Because the factor is identical for both members of
 *   the pair (only the multiplied mass differs, m_j vs m_i), Newton's third
 *   law and momentum conservation hold for ANY combination of multipliers,
 *   not only when they're equal - an earlier source-only formulation broke
 *   momentum conservation whenever g_i != g_j, which is why this form is
 *   used instead.
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include <stddef.h>

#include "body.h"
#include "vector3d.h"

/* Gravitational constant in simulation units. */
#define PHYS_G 1.0

/*
 * Plummer-style softening length. Purely a numerical guard against division
 * by zero when two bodies are at (nearly) the same point. It is far smaller
 * than any physical body radius, so collisions are always detected long
 * before softening becomes relevant - it is not used to hide collisions.
 */
#define PHYS_SOFTENING 1.0e-7

void   physics_compute_accelerations(Body *bodies, size_t count);
double physics_kinetic_energy(const Body *bodies, size_t count);
double physics_potential_energy(const Body *bodies, size_t count);
double physics_total_energy(const Body *bodies, size_t count);
Vec3   physics_total_momentum(const Body *bodies, size_t count);
double physics_total_mass(const Body *bodies, size_t count);
Vec3   physics_center_of_mass(const Body *bodies, size_t count);
Vec3   physics_center_of_mass_velocity(const Body *bodies, size_t count);
/* Subtracts the centre-of-mass velocity from every body. */
void   physics_remove_com_velocity(Body *bodies, size_t count);
/* Adds the same velocity to every body (does not change relative dynamics). */
void   physics_add_common_velocity(Body *bodies, size_t count, Vec3 v);
/* Circular orbit speed sqrt(G*M/r); returns 0 for invalid inputs. */
double physics_circular_orbit_speed(double centralMass, double radius);

#endif /* PHYSICS_H */
