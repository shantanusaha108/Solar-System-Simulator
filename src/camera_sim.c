#include <math.h>

#include "cglm/cglm.h"
#include "raylib.h"

#include "camera_sim.h"

#define PITCH_LIMIT 1.50f /* just under 90 degrees, avoids gimbal flip */

static Vector3 to_render(Vec3 v)
{
    Vector3 out = { (float)v.x, (float)v.y, (float)v.z };
    return out;
}

/* Spherical offset -> cartesian eye offset, via cglm. */
static void camera_offset(const SimCamera *cam, vec3 out)
{
    float cp = cosf(cam->pitch);
    vec3  dir = { sinf(cam->yaw) * cp, sinf(cam->pitch), cosf(cam->yaw) * cp };
    glm_vec3_normalize(dir);
    glm_vec3_scale(dir, cam->distance, out);
}

static void camera_apply(SimCamera *cam)
{
    vec3 offset;
    camera_offset(cam, offset);

    vec3 pivot = { cam->pivot.x, cam->pivot.y, cam->pivot.z };
    vec3 eye;
    glm_vec3_add(pivot, offset, eye);

    cam->camera.position = (Vector3){ eye[0], eye[1], eye[2] };
    cam->camera.target   = cam->pivot;
    cam->camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
}

void camera_init(SimCamera *cam)
{
    if (!cam) return;
    camera_reset(cam);
}

/* The ONE source of camera defaults: camera_init is just this, so the
   first-launch camera and the factory-reset camera are the same thing. */
void camera_reset(SimCamera *cam)
{
    if (!cam) return;

    cam->camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->camera.fovy       = 45.0f;
    cam->camera.projection = CAMERA_PERSPECTIVE;

    cam->minDistance  = 0.05f;
    cam->maxDistance  = 60.0f;
    cam->moveSpeed    = 0.9f;
    cam->rotateSpeed  = 0.005f;
    cam->followLerp   = 0.12f;
    cam->followTarget = SIM_EARTH; /* the UI no longer offers a system-follow option */

    /*
     * Default framing: off to the side of the X-Z orbital plane and slightly
     * elevated, never straight down on it, and far enough back that the Star,
     * the Earth and a long stretch of the trail are visible at once.
     */
    cam->mode     = CAMERA_MODE_FOLLOW;
    cam->pivot    = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam->yaw      = 0.65f;
    cam->pitch    = 0.38f;
    cam->distance = 6.5f;

    camera_apply(cam);
}

void camera_set_mode(SimCamera *cam, SimCameraMode mode, const Simulation *sim)
{
    if (!cam || cam->mode == mode) return;

    cam->mode = mode;
    /* Switching modes must not jolt the view or touch the simulation. Snap
       the pivot straight to the follow target so the first frame doesn't
       lerp in from wherever the free camera happened to be pointed. */
    if (mode == CAMERA_MODE_FOLLOW && sim)
    {
        const Body *target = simulation_body(sim, (size_t)cam->followTarget);
        cam->pivot = to_render(target && target->active ? target->position
                                                        : simulation_system_center(sim));
    }

    camera_apply(cam);
}

void camera_set_follow_target(SimCamera *cam, int target, const Simulation *sim)
{
    if (!cam) return;
    cam->followTarget = target;

    /* Per-body defaults so the target body's surroundings stay visible
       instead of, e.g., a moon-follow leaving the camera pressed against a
       point with nothing recognisable around it. */
    float presetDistance;
    switch (target)
    {
        case SIM_MOON:  presetDistance = 0.09f; break;
        case SIM_EARTH: presetDistance = 2.2f;  break;
        case SIM_STAR:  default: presetDistance = 6.5f; break;
    }
    camera_set_distance(cam, presetDistance);

    if (sim)
    {
        const Body *body = simulation_body(sim, (size_t)target);
        cam->pivot = to_render(body && body->active ? body->position
                                                    : simulation_system_center(sim));
    }
    camera_apply(cam);
}

void camera_set_distance(SimCamera *cam, float distance)
{
    if (!cam) return;
    if (distance < cam->minDistance) distance = cam->minDistance;
    if (distance > cam->maxDistance) distance = cam->maxDistance;
    cam->distance = distance;
    camera_apply(cam);
}

void camera_zoom(SimCamera *cam, float amount)
{
    if (!cam || amount == 0.0f) return;
    /* Multiplicative so zooming feels even at every scale. */
    camera_set_distance(cam, cam->distance * powf(0.88f, amount));
}

static void camera_handle_rotation(SimCamera *cam)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        Vector2 delta = GetMouseDelta();
        cam->yaw   -= delta.x * cam->rotateSpeed;
        cam->pitch += delta.y * cam->rotateSpeed;

        if (cam->pitch >  PITCH_LIMIT) cam->pitch =  PITCH_LIMIT;
        if (cam->pitch < -PITCH_LIMIT) cam->pitch = -PITCH_LIMIT;
    }
}

static void camera_handle_zoom(SimCamera *cam)
{
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) camera_zoom(cam, wheel);
}

/* W/S/A/D pan the pivot in the camera plane, Q/E move it along world Y. */
static void camera_handle_movement(SimCamera *cam, float dt)
{
    vec3 offset;
    camera_offset(cam, offset);

    vec3 forward;
    glm_vec3_negate_to(offset, forward);
    forward[1] = 0.0f;
    if (glm_vec3_norm(forward) < 1e-6f) { forward[0] = 0.0f; forward[2] = -1.0f; }
    glm_vec3_normalize(forward);

    vec3 worldUp = { 0.0f, 1.0f, 0.0f };
    vec3 right;
    glm_vec3_cross(forward, worldUp, right);
    glm_vec3_normalize(right);

    vec3 move = { 0.0f, 0.0f, 0.0f };
    /* 'right' = cross(forward, worldUp) points toward the camera's actual
       right-hand side for a right-handed, Y-up basis looking along
       'forward'. D (right) must therefore ADD it and A (left) SUBTRACT it. */
    if (IsKeyDown(KEY_W)) glm_vec3_add(move, forward, move);
    if (IsKeyDown(KEY_S)) glm_vec3_sub(move, forward, move);
    if (IsKeyDown(KEY_D)) glm_vec3_add(move, right,   move);
    if (IsKeyDown(KEY_A)) glm_vec3_sub(move, right,   move);
    if (IsKeyDown(KEY_E)) move[1] += 1.0f;
    if (IsKeyDown(KEY_Q)) move[1] -= 1.0f;

    if (glm_vec3_norm(move) < 1e-6f) return;

    glm_vec3_normalize(move);
    /* Panning speed scales with zoom so it stays usable at every scale. */
    glm_vec3_scale(move, cam->moveSpeed * cam->distance * dt, move);

    cam->pivot.x += move[0];
    cam->pivot.y += move[1];
    cam->pivot.z += move[2];
}

void camera_update_free(SimCamera *cam, float dt, int inputEnabled)
{
    if (!cam) return;

    if (inputEnabled)
    {
        camera_handle_rotation(cam);
        camera_handle_zoom(cam);
        camera_handle_movement(cam, dt);
    }
    camera_apply(cam);
}

void camera_update_follow(SimCamera *cam, const Simulation *sim, float dt,
                          int inputEnabled)
{
    if (!cam) return;

    if (inputEnabled)
    {
        camera_handle_rotation(cam);
        camera_handle_zoom(cam);
    }

    if (sim)
    {
        Vec3 target;
        if (cam->followTarget >= 0 && (size_t)cam->followTarget < sim->bodyCount &&
            sim->bodies[cam->followTarget].active)
        {
            target = sim->bodies[cam->followTarget].position;
        }
        else
        {
            target = simulation_system_center(sim);
        }

        /*
         * Only the pivot tracks the system. The bodies keep moving relative to
         * it, so following does not freeze the scene.
         */
        Vector3 t = to_render(target);
        float   k = cam->followLerp;
        if (k < 0.0f) k = 0.0f;
        if (k > 1.0f) k = 1.0f;

        cam->pivot.x += (t.x - cam->pivot.x) * k;
        cam->pivot.y += (t.y - cam->pivot.y) * k;
        cam->pivot.z += (t.z - cam->pivot.z) * k;
    }

    (void)dt;
    camera_apply(cam);
}

void camera_update(SimCamera *cam, const Simulation *sim, float dt,
                   int inputEnabled)
{
    if (!cam) return;

    if (cam->mode == CAMERA_MODE_FOLLOW)
        camera_update_follow(cam, sim, dt, inputEnabled);
    else
        camera_update_free(cam, dt, inputEnabled);
}
