/*
 * body.h - a celestial body.
 *
 * This structure holds PHYSICS state only (plus its recorded trail).
 * Colours, render radii and other purely visual properties live in the
 * renderer (see renderer.h : BodyVisual) so that changing how a body
 * looks can never change how it behaves.
 */
#ifndef BODY_H
#define BODY_H

#include "trail.h"
#include "vector3d.h"

#define BODY_NAME_LEN 16

typedef struct
{
    char name[BODY_NAME_LEN];

    double mass;              /* simulation mass units                     */
    double radius;            /* PHYSICAL radius, used for collisions only */
    double gravityMultiplier; /* experimental, non-physical (see physics.h)*/

    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;

    int   active;             /* cleared when merged away by a collision   */

    Trail trail;
} Body;

/* Initialises physics state and allocates the trail buffer. */
void body_init(Body *body, const char *name, double mass, double radius,
               Vec3 position, Vec3 velocity, size_t trailLength);
void body_free(Body *body);
/* Resets position/velocity/acceleration and clears trail history. */
void body_set_state(Body *body, Vec3 position, Vec3 velocity);
/* Volume conserving radius used by the inelastic merge. */
double body_merged_radius(double r1, double r2);

#endif /* BODY_H */
