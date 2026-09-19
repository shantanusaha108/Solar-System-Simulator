/*
 * simulation.h - owns the bodies, the configuration and the fixed timestep
 * loop. Knows nothing about raylib: it can be driven headlessly by the tests.
 */
#ifndef SIMULATION_H
#define SIMULATION_H

#include <stddef.h>

#include "body.h"
#include "collision.h"
#include "integrator.h"
#include "physics.h"

#define SIM_MAX_BODIES 16

enum { SIM_STAR = 0, SIM_EARTH = 1, SIM_MOON = 2 };

typedef struct
{
    /* --- masses (simulation units, Star = 1 by default) --- */
    double starMass;
    double earthMass;
    double moonMass;

    /* --- experimental, non-physical gravity multipliers --- */
    double starGravity;
    double earthGravity;
    double moonGravity;

    /* --- physical (collision) radii --- */
    double starRadius;
    double earthRadius;
    double moonRadius;

    /* --- initial orbital geometry (applied on reset) --- */
    double earthOrbitRadius; /* Earth-Star distance, AU-equivalent */
    double moonOrbitRadius;  /* Moon-Earth distance, AU-equivalent */

    /* Common translational velocity added to every body after the internal
       orbital velocities are built and the COM drift is removed. The orbital
       plane is X-Z, so translating along +Y turns Earth's path into a helix. */
    Vec3 translation;

    /* --- time --- */
    double dt;            /* fixed physics timestep                       */
    int    stepsPerFrame; /* simulation speed: physics steps per frame    */
    double trailSample;   /* simulation-time interval between trail points */

    /* --- trail history lengths (points) --- */
    size_t starTrailLength;
    size_t earthTrailLength;
    size_t moonTrailLength;

    CollisionMode  collisionMode;
    IntegratorType integrator;
} SimConfig;

typedef struct
{
    SimConfig config;

    Body   bodies[SIM_MAX_BODIES];
    size_t bodyCount;

    double time;
    int    paused;

    double nextTrailSampleTime;

    double initialEnergy;
    CollisionEvent lastCollision;
} Simulation;

SimConfig simulation_default_config(void);

/* First-time setup: allocates trails, then builds the initial state. */
void simulation_init(Simulation *sim);
void simulation_free(Simulation *sim);

/* Rebuilds the initial state from sim->config (positions, velocities,
   accelerations, masses, radii, trails, time). Deterministic.
   This is the ONE initialisation path: simulation_init, the factory reset
   below and the UI's "Apply Changes" all funnel through it, so orbital
   velocities are only ever derived in a single place. Callers that want new
   initial conditions edit sim->config first and then call this. */
void simulation_reset(Simulation *sim);

/* Factory reset: replaces the WHOLE configuration with
   simulation_default_config() and then rebuilds the initial state from it.
   Distinct from simulation_reset(), which keeps whatever configuration is
   currently in place - masses, gravity multipliers, speed and trail lengths
   included. */
void simulation_restore_defaults(Simulation *sim);

/* Advances the simulation by config.stepsPerFrame fixed steps (nothing while
   paused). frameTime is accepted for API symmetry and diagnostics; the
   physics timestep is deliberately independent of the frame rate. */
void simulation_update(Simulation *sim, double frameTime);

/* A single fixed physics step: integrate, resolve collisions, sample trails. */
void simulation_step(Simulation *sim);

void simulation_set_speed(Simulation *sim, int stepsPerFrame);
void simulation_toggle_pause(Simulation *sim);

/* Live parameter changes (no re-initialisation, no teleporting). */
void simulation_set_mass(Simulation *sim, size_t index, double mass);
void simulation_set_gravity_multiplier(Simulation *sim, size_t index, double g);
void simulation_set_trail_length(Simulation *sim, size_t index, size_t length);

Vec3   simulation_system_center(const Simulation *sim);
Vec3   simulation_system_velocity(const Simulation *sim);
double simulation_energy_drift(const Simulation *sim);
const Body *simulation_body(const Simulation *sim, size_t index);

#endif /* SIMULATION_H */
