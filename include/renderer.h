/*
 * renderer.h - all raylib drawing.
 *
 * Visual properties live here, never in Body: changing a render radius can
 * never change a collision radius (see item 34 / body.h). Bodies are drawn
 * far larger than scale so that they are visible at all.
 *
 * MOON VISUAL ORBIT SCALE
 *   The physical Moon-Earth distance (sim->config.moonOrbitRadius, ~0.00257
 *   AU) is real and untouched. At that scale the Moon's orbit is a few
 *   pixels wide next to a 1 AU Earth-Star orbit, so purely for DISPLAY the
 *   renderer draws the Moon (its sphere, its trail, and its reference orbit)
 *   at an exaggerated offset from Earth: MOON_VISUAL_ORBIT_SCALE times the
 *   real Earth-relative offset. The factor is chosen together with the
 *   visual radii below: the drawn separation (0.00257 AU x 96 = 0.247) must
 *   stay comfortably larger than Earth's visual radius plus the Moon's, or
 *   the enlarged Moon would sit inside Earth's disc. This is a rendering transform applied at
 *   draw time only - it never touches Body.position/velocity/mass, the
 *   physics never sees it, and the trail continues to record real positions
 *   (see renderer.c: moon_display_offset / moon trail rendering).
 */
#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#include "camera_sim.h"
#include "simulation.h"

#define STARFIELD_COUNT   2400
#define GALAXY_BAND_COUNT 1400
#define MAX_METEORS       6

#define MOON_VISUAL_ORBIT_SCALE 96.0

typedef struct
{
    Color  color;
    Color  trailColor;
    float  visualRadius; /* render only; independent of Body.radius */
    int    rings;        /* sphere tessellation                     */
    int    slices;
} BodyVisual;

typedef struct
{
    Vector3 position;
    Vector3 direction; /* unit, world space */
    float   speed;
    float   life;
    float   maxLife;
    int     active;
} Meteor;

typedef struct
{
    BodyVisual visuals[SIM_MAX_BODIES];

    float trailFadeExponent; /* alpha = age^exponent, age 0(old)..1(new) */
    int   showTrails;
    int   showStarfield;
    int   showOrbitPlaneGrid;
    int   showReferenceOrbits;
    int   showMeteors;
    int   showGalaxy;

    /* Procedural, generated once in renderer_init and reused every frame
       (see item 39). Star field / galaxy are direction vectors drawn at a
       fixed radius around the camera so the background never appears to
       be approached; each has a paired, precomputed color. */
    Vector3 starfield[STARFIELD_COUNT];
    Color   starfieldColor[STARFIELD_COUNT];
    float   starfieldRadius;

    Vector3 galaxyPoints[GALAXY_BAND_COUNT];
    Color   galaxyColor[GALAXY_BAND_COUNT];

    Meteor meteors[MAX_METEORS];
    float  meteorSpawnTimer;

    /* Axial spin, in degrees, advanced by renderer_update from the real
       frame time (angle += rate * frameTime), never by a fixed amount per
       frame - so the planets turn at the same rate at 30 fps and at 144.
       This is presentation state and lives here rather than in Body: no
       physics routine can see it, and no position, velocity or acceleration
       is touched to produce it (items 13-16, 26). */
    float earthSpin;          /* current angle, degrees, wrapped to 360 */
    float moonSpin;
    float earthSpinDegPerSec; /* ~8 s per visible rotation by default   */
    float moonSpinDegPerSec;  /* deliberately slower than Earth's       */

    /* Procedural sphere textures, built once (item 15/16/39). */
    Texture2D earthTexture;
    Texture2D moonTexture;
    Model     earthModel;
    Model     moonModel;
    int       texturesLoaded; /* 0 if GPU texture creation failed (headless) */
} Renderer;

void renderer_init(Renderer *renderer);
/* Factory reset of all renderer state: every configurable value and every
   piece of generated runtime visual state returns to its first-launch value.
   Keeps the GPU textures/models. renderer_init is built on this. */
void renderer_reset(Renderer *renderer);
void renderer_unload(Renderer *renderer);

/* Advances purely-visual, real-time state (meteor spawns/motion, planet
   axial spin). Independent of the physics timestep and of pause, since these
   are presentation effects, not simulation state. Call once per frame before
   renderer_draw. */
void renderer_update(Renderer *renderer, float frameTime);

/* Draws the 3D scene: starfield, galaxy, reference orbits, trails, bodies,
   meteors. Body names are deliberately NOT drawn (item 9): Body.name is
   still used for collision messages, it simply never reaches the 3D view. */
void renderer_draw(const Renderer *renderer, const Simulation *sim,
                   const SimCamera *cam);

/* Converts authoritative double physics state to float render space. */
Vector3 renderer_to_render(Vec3 v);

#endif /* RENDERER_H */
