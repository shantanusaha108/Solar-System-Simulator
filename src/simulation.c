#include <math.h>
#include <string.h>

#include "simulation.h"

#define MIN_MASS   1.0e-12
#define MAX_MASS   1.0e6
#define MIN_SPEED  1
#define MAX_SPEED  64

static double clamp_mass(double m)
{
    if (!isfinite(m) || m < MIN_MASS) return MIN_MASS; /* negative mass unsupported */
    if (m > MAX_MASS) return MAX_MASS;
    return m;
}

static double clamp_gravity(double g)
{
    if (!isfinite(g)) return 1.0;
    if (g < 0.0)   return 0.0;
    if (g > 100.0) return 100.0;
    return g;
}

SimConfig simulation_default_config(void)
{
    SimConfig c;
    memset(&c, 0, sizeof(c));

    /* Normalised Sun-Earth-Moon system (Star mass == 1). */
    c.starMass  = 1.0;
    c.earthMass = 3.0e-6;
    c.moonMass  = 3.69e-8;

    c.starGravity  = 1.0;
    c.earthGravity = 1.0;
    c.moonGravity  = 1.0;

    /* Physical radii used ONLY for collisions, in AU-equivalent units and
       close to the real ratios. They must stay far smaller than the Moon's
       orbit radius, otherwise the default system would merge on frame one.
       The enlarged spheres you see on screen are a renderer setting. */
    c.starRadius  = 4.65e-3; /* ~696,000 km  */
    c.earthRadius = 4.26e-5; /* ~6,371 km    */
    c.moonRadius  = 1.16e-5; /* ~1,737 km    */

    c.earthOrbitRadius = 1.0;
    c.moonOrbitRadius  = 0.00257;

    /* Earth's orbital speed at 1 AU is 1.0 in these units, so a translation
       of 0.25 advances the system ~1.57 AU per orbit: a clearly stretched,
       but still overlapping, spring. The small +Y component keeps the helix
       off the orbital plane so it reads as 3D. */
    c.translation = vec3_make(0.25, 0.06, 0.0);

    c.dt            = 0.001;
    c.stepsPerFrame = 8;
    c.trailSample   = 0.008;

    c.earthTrailLength = 6000; /* dominant trail: ~7 visible oscillations */
    c.moonTrailLength  = 2400; /* ~40% of Earth                          */
    c.starTrailLength  = 450;  /* ~7% of Earth: short fading tick mark    */

    c.collisionMode = COLLISION_MODE_MERGE;
    c.integrator    = INTEGRATOR_VELOCITY_VERLET;

    return c;
}

void simulation_init(Simulation *sim)
{
    if (!sim) return;

    memset(sim, 0, sizeof(*sim));
    sim->config    = simulation_default_config();
    sim->bodyCount = 3;

    body_init(&sim->bodies[SIM_STAR],  "Star",  sim->config.starMass,
              sim->config.starRadius,  vec3_zero(), vec3_zero(),
              sim->config.starTrailLength);
    body_init(&sim->bodies[SIM_EARTH], "Earth", sim->config.earthMass,
              sim->config.earthRadius, vec3_zero(), vec3_zero(),
              sim->config.earthTrailLength);
    body_init(&sim->bodies[SIM_MOON],  "Moon",  sim->config.moonMass,
              sim->config.moonRadius,  vec3_zero(), vec3_zero(),
              sim->config.moonTrailLength);

    simulation_reset(sim);
}

void simulation_free(Simulation *sim)
{
    if (!sim) return;
    for (size_t i = 0; i < sim->bodyCount; ++i) body_free(&sim->bodies[i]);
    sim->bodyCount = 0;
}

void simulation_restore_defaults(Simulation *sim)
{
    if (!sim) return;

    /* Everything the user can reach - masses, gravity multipliers, orbital
       geometry, translation, timestep, speed, trail lengths, collision mode
       and integrator - comes back from the single defaults source, and the
       shared reset path then rebuilds the state to match. Trail buffers are
       resized by simulation_reset via trail_set_max_length, so the default
       lengths take effect immediately rather than at the next sample. */
    sim->config = simulation_default_config();
    sim->paused = 0; /* run state is part of the factory state; a plain
                        simulation_reset (Apply) deliberately keeps it */
    simulation_reset(sim);
}

void simulation_reset(Simulation *sim)
{
    if (!sim) return;

    SimConfig *c = &sim->config;

    c->starMass  = clamp_mass(c->starMass);
    c->earthMass = clamp_mass(c->earthMass);
    c->moonMass  = clamp_mass(c->moonMass);

    if (!(c->earthOrbitRadius > 0.0) || !isfinite(c->earthOrbitRadius))
        c->earthOrbitRadius = 1.0;
    if (!(c->moonOrbitRadius > 0.0) || !isfinite(c->moonOrbitRadius))
        c->moonOrbitRadius = 0.00257;

    Body *star  = &sim->bodies[SIM_STAR];
    Body *earth = &sim->bodies[SIM_EARTH];
    Body *moon  = &sim->bodies[SIM_MOON];

    star->mass   = c->starMass;
    earth->mass  = c->earthMass;
    moon->mass   = c->moonMass;

    star->radius  = c->starRadius;
    earth->radius = c->earthRadius;
    moon->radius  = c->moonRadius;

    star->gravityMultiplier  = clamp_gravity(c->starGravity);
    earth->gravityMultiplier = clamp_gravity(c->earthGravity);
    moon->gravityMultiplier  = clamp_gravity(c->moonGravity);

    star->active = earth->active = moon->active = 1;

    memset(&sim->lastCollision, 0, sizeof(sim->lastCollision));

    trail_set_max_length(&star->trail,  c->starTrailLength);
    trail_set_max_length(&earth->trail, c->earthTrailLength);
    trail_set_max_length(&moon->trail,  c->moonTrailLength);

    /*
     * Orbital plane is X-Z.
     *   Earth sits at +X of the Star and moves tangentially along +Z.
     *   Moon sits further out along +X and adds its own tangential speed.
     * Both speeds come from v = sqrt(G*M/r) using the two-body reduced mass,
     * so changing a mass or an orbit radius changes the velocity too.
     */
    double ve = physics_circular_orbit_speed(star->mass + earth->mass,
                                             c->earthOrbitRadius);
    double vm = physics_circular_orbit_speed(earth->mass + moon->mass,
                                             c->moonOrbitRadius);

    Vec3 earthPos = vec3_make(c->earthOrbitRadius, 0.0, 0.0);
    Vec3 earthVel = vec3_make(0.0, 0.0, ve);
    Vec3 moonPos  = vec3_make(c->earthOrbitRadius + c->moonOrbitRadius, 0.0, 0.0);
    Vec3 moonVel  = vec3_make(0.0, 0.0, ve + vm);

    body_set_state(star,  vec3_zero(), vec3_zero());
    body_set_state(earth, earthPos, earthVel);
    body_set_state(moon,  moonPos,  moonVel);

    /* 1. internal orbital velocities are set above
       2. remove the accidental centre-of-mass drift they introduce
       3. add the deliberate common translation of the whole system */
    physics_remove_com_velocity(sim->bodies, sim->bodyCount);
    physics_add_common_velocity(sim->bodies, sim->bodyCount, c->translation);

    /* Start with the centre of mass at the origin so the default camera
       framing is deterministic. */
    Vec3 com = physics_center_of_mass(sim->bodies, sim->bodyCount);
    for (size_t i = 0; i < sim->bodyCount; ++i)
        sim->bodies[i].position = vec3_sub(sim->bodies[i].position, com);

    /* An extreme configuration can start already overlapping, so resolve
       that before the first step rather than integrating through it. */
    collision_resolve_all(sim->bodies, sim->bodyCount, c->collisionMode,
                          &sim->lastCollision);

    physics_compute_accelerations(sim->bodies, sim->bodyCount);

    sim->time                = 0.0;
    sim->nextTrailSampleTime = 0.0;
    sim->initialEnergy       = physics_total_energy(sim->bodies, sim->bodyCount);


    for (size_t i = 0; i < sim->bodyCount; ++i)
        trail_push(&sim->bodies[i].trail, sim->bodies[i].position);
}

void simulation_step(Simulation *sim)
{
    if (!sim) return;

    SimConfig *c = &sim->config;
    if (!isfinite(c->dt) || c->dt <= 0.0) c->dt = 0.001;

    integrator_step(sim->bodies, sim->bodyCount, c->dt, c->integrator);

    if (collision_resolve_all(sim->bodies, sim->bodyCount, c->collisionMode,
                              &sim->lastCollision) > 0)
    {
        /* Masses changed, so the stored accelerations are stale. */
        physics_compute_accelerations(sim->bodies, sim->bodyCount);
    }

    sim->time += c->dt;

    if (sim->time >= sim->nextTrailSampleTime)
    {
        for (size_t i = 0; i < sim->bodyCount; ++i)
        {
            if (!sim->bodies[i].active) continue;
            trail_push(&sim->bodies[i].trail, sim->bodies[i].position);
        }
        double interval = (c->trailSample > 0.0) ? c->trailSample : c->dt;
        sim->nextTrailSampleTime = sim->time + interval;
    }
}

void simulation_update(Simulation *sim, double frameTime)
{
    (void)frameTime; /* physics is deliberately frame-rate independent */
    if (!sim || sim->paused) return;

    int steps = sim->config.stepsPerFrame;
    if (steps < MIN_SPEED) steps = MIN_SPEED;
    if (steps > MAX_SPEED) steps = MAX_SPEED;

    for (int i = 0; i < steps; ++i) simulation_step(sim);
}

void simulation_set_speed(Simulation *sim, int stepsPerFrame)
{
    if (!sim) return;
    if (stepsPerFrame < MIN_SPEED) stepsPerFrame = MIN_SPEED;
    if (stepsPerFrame > MAX_SPEED) stepsPerFrame = MAX_SPEED;
    sim->config.stepsPerFrame = stepsPerFrame;
}

void simulation_toggle_pause(Simulation *sim)
{
    if (sim) sim->paused = !sim->paused;
}

void simulation_set_mass(Simulation *sim, size_t index, double mass)
{
    if (!sim || index >= sim->bodyCount) return;

    mass = clamp_mass(mass);
    sim->bodies[index].mass = mass;

    if (index == SIM_STAR)       sim->config.starMass  = mass;
    else if (index == SIM_EARTH) sim->config.earthMass = mass;
    else if (index == SIM_MOON)  sim->config.moonMass  = mass;

    physics_compute_accelerations(sim->bodies, sim->bodyCount);
}

void simulation_set_gravity_multiplier(Simulation *sim, size_t index, double g)
{
    if (!sim || index >= sim->bodyCount) return;

    g = clamp_gravity(g);
    sim->bodies[index].gravityMultiplier = g;

    if (index == SIM_STAR)       sim->config.starGravity  = g;
    else if (index == SIM_EARTH) sim->config.earthGravity = g;
    else if (index == SIM_MOON)  sim->config.moonGravity  = g;

    physics_compute_accelerations(sim->bodies, sim->bodyCount);
}

void simulation_set_trail_length(Simulation *sim, size_t index, size_t length)
{
    if (!sim || index >= sim->bodyCount) return;

    trail_set_max_length(&sim->bodies[index].trail, length);

    if (index == SIM_STAR)       sim->config.starTrailLength  = length;
    else if (index == SIM_EARTH) sim->config.earthTrailLength = length;
    else if (index == SIM_MOON)  sim->config.moonTrailLength  = length;
}

Vec3 simulation_system_center(const Simulation *sim)
{
    if (!sim) return vec3_zero();
    return physics_center_of_mass(sim->bodies, sim->bodyCount);
}

Vec3 simulation_system_velocity(const Simulation *sim)
{
    if (!sim) return vec3_zero();
    return physics_center_of_mass_velocity(sim->bodies, sim->bodyCount);
}

double simulation_energy_drift(const Simulation *sim)
{
    if (!sim) return 0.0;
    double e0 = sim->initialEnergy;
    if (!(fabs(e0) > 0.0)) return 0.0;
    double e = physics_total_energy(sim->bodies, sim->bodyCount);
    return (e - e0) / fabs(e0);
}

const Body *simulation_body(const Simulation *sim, size_t index)
{
    if (!sim || index >= sim->bodyCount) return NULL;
    return &sim->bodies[index];
}
