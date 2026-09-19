/*
 * camera_sim.h - orbit-style 3D camera with FREE and FOLLOW modes.
 *
 * The camera reads the simulation but never writes to it.
 */
#ifndef CAMERA_SIM_H
#define CAMERA_SIM_H

#include "raylib.h"

#include "simulation.h"

typedef enum
{
    CAMERA_MODE_FREE   = 0,
    CAMERA_MODE_FOLLOW = 1
} SimCameraMode;

typedef struct
{
    Camera3D      camera;
    SimCameraMode mode;

    Vector3 pivot;        /* point the camera orbits around            */
    float   yaw;          /* radians, around the world Y axis          */
    float   pitch;        /* radians, clamped away from the poles      */
    float   distance;     /* zoom: pivot-to-eye distance               */

    float   minDistance;
    float   maxDistance;
    float   moveSpeed;    /* world units per second, scales with zoom  */
    float   rotateSpeed;  /* radians per pixel of mouse drag           */
    float   followLerp;   /* follow smoothing, 0..1 per frame          */

    /* -1 follows the system centre of mass, otherwise a body index. */
    int     followTarget;
} SimCamera;

void camera_init(SimCamera *cam);
/* Factory reset: mode, follow target, pivot, yaw, pitch, distance and every
   tuning value (limits, speeds, smoothing) return to their first-launch
   values. camera_init is built on this. */
void camera_reset(SimCamera *cam);

void camera_set_mode(SimCamera *cam, SimCameraMode mode, const Simulation *sim);
/* Switches which body FOLLOW mode tracks (SIM_STAR/SIM_EARTH/SIM_MOON), snaps
   the pivot to it immediately, and picks a sensible default distance for
   that body so, e.g., following the Moon doesn't leave Earth's context
   completely out of frame. Only called when the target actually changes -
   it does not fight the user's own zooming afterwards. */
void camera_set_follow_target(SimCamera *cam, int target, const Simulation *sim);
void camera_zoom(SimCamera *cam, float amount);
void camera_set_distance(SimCamera *cam, float distance);

void camera_update_free(SimCamera *cam, float dt, int inputEnabled);
void camera_update_follow(SimCamera *cam, const Simulation *sim, float dt,
                          int inputEnabled);
/* Dispatches on the current mode and refreshes the raylib Camera3D. */
void camera_update(SimCamera *cam, const Simulation *sim, float dt,
                   int inputEnabled);

#endif /* CAMERA_SIM_H */
