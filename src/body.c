#include <math.h>
#include <string.h>

#include "body.h"

void body_init(Body *body, const char *name, double mass, double radius,
               Vec3 position, Vec3 velocity, size_t trailLength)
{
    if (!body) return;

    memset(body->name, 0, BODY_NAME_LEN);
    if (name)
    {
        strncpy(body->name, name, BODY_NAME_LEN - 1);
    }

    body->mass              = mass;
    body->radius            = radius > 0.0 ? radius : 0.0;
    body->gravityMultiplier = 1.0;
    body->position          = position;
    body->velocity          = velocity;
    body->acceleration      = vec3_zero();
    body->active            = 1;

    trail_init(&body->trail, trailLength);
}

void body_free(Body *body)
{
    if (!body) return;
    trail_free(&body->trail);
}

void body_set_state(Body *body, Vec3 position, Vec3 velocity)
{
    if (!body) return;
    body->position     = position;
    body->velocity     = velocity;
    body->acceleration = vec3_zero();
    trail_clear(&body->trail);
}

double body_merged_radius(double r1, double r2)
{
    return cbrt(r1 * r1 * r1 + r2 * r2 * r2);
}
