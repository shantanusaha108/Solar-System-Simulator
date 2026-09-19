/* Headless test of the simulation half of the factory reset. */
#include <string.h>
#include "simulation.h"
#include "test_util.h"

int main(void)
{
    Simulation sim;
    simulation_init(&sim);
    const SimConfig def = simulation_default_config();

    /* Change everything reachable from the UI, run, pause. */
    sim.config.starMass = 5.0; sim.config.earthMass = 3.0e-5; sim.config.moonMass = 7.0e-7;
    sim.config.starGravity = 4.0; sim.config.earthGravity = 3.0; sim.config.moonGravity = 2.0;
    sim.config.earthOrbitRadius = 3.0; sim.config.moonOrbitRadius = 0.005;
    sim.config.translation = vec3_make(0.0, 0.9, 0.0);
    sim.config.stepsPerFrame = 40;
    sim.config.collisionMode = COLLISION_MODE_NONE;
    simulation_set_trail_length(&sim, SIM_EARTH, 300);
    simulation_reset(&sim);
    for (int i = 0; i < 200; ++i) simulation_step(&sim);
    sim.paused = 1;

    simulation_restore_defaults(&sim);

    CHECK_NEAR(sim.config.starMass, def.starMass, 0, "star mass default");
    CHECK_NEAR(sim.config.earthMass, def.earthMass, 0, "earth mass default");
    CHECK_NEAR(sim.config.moonMass, def.moonMass, 0, "moon mass default");
    CHECK_NEAR(sim.config.starGravity, 1.0, 0, "star gravity default");
    CHECK_NEAR(sim.config.earthGravity, 1.0, 0, "earth gravity default");
    CHECK_NEAR(sim.config.moonGravity, 1.0, 0, "moon gravity default");
    CHECK_NEAR(sim.config.earthOrbitRadius, def.earthOrbitRadius, 0, "earth orbit default");
    CHECK_NEAR(sim.config.moonOrbitRadius, def.moonOrbitRadius, 0, "moon orbit default");
    CHECK_NEAR(vec3_length(sim.config.translation), vec3_length(def.translation), 1e-12, "translation default");
    CHECK(sim.config.stepsPerFrame == def.stepsPerFrame, "speed default");
    CHECK(sim.config.collisionMode == def.collisionMode, "collision mode default");
    CHECK(sim.config.earthTrailLength == def.earthTrailLength, "earth trail length default");
    CHECK(sim.paused == 0, "paused cleared");
    CHECK_NEAR(sim.time, 0.0, 0, "time zero");
    CHECK_NEAR(sim.nextTrailSampleTime, 0.0, 0, "next trail sample zero");
    CHECK_NEAR(sim.bodies[SIM_EARTH].mass, def.earthMass, 0, "body mass rebuilt");
    CHECK_NEAR(sim.bodies[SIM_EARTH].radius, def.earthRadius, 0, "physical radius default");
    CHECK_NEAR(sim.bodies[SIM_EARTH].gravityMultiplier, 1.0, 0, "body gravity rebuilt");
    CHECK(sim.bodies[SIM_MOON].active == 1, "bodies active");

    /* Must equal a freshly initialised simulation, body by body. */
    Simulation fresh;
    simulation_init(&fresh);
    int same = 1;
    for (size_t i = 0; i < 3; ++i)
        same &= memcmp(&sim.bodies[i].position, &fresh.bodies[i].position, sizeof(Vec3)) == 0 &&
                memcmp(&sim.bodies[i].velocity, &fresh.bodies[i].velocity, sizeof(Vec3)) == 0;
    CHECK(same, "state identical to a fresh launch");
    CHECK_NEAR(sim.initialEnergy, fresh.initialEnergy, 0, "initial energy identical");

    /* Plain simulation_reset (APPLY path) must NOT factory-reset or unpause. */
    sim.config.starMass = 2.0; sim.paused = 1;
    simulation_reset(&sim);
    CHECK_NEAR(sim.config.starMass, 2.0, 0, "simulation_reset keeps mass");
    CHECK(sim.paused == 1, "simulation_reset keeps paused");

    simulation_free(&sim); simulation_free(&fresh);
    TEST_REPORT("test_reset");
}
