#include <math.h>
#include <stdio.h>
#include <string.h>

#include "collision.h"

int collision_detect(const Body *a, const Body *b)
{
    if (!a || !b || !a->active || !b->active) return 0;

    double sum = a->radius + b->radius;
    if (!(sum > 0.0)) return 0;

    Vec3   d  = vec3_sub(b->position, a->position);
    double d2 = vec3_length_sq(d);
    if (!isfinite(d2)) return 0;

    return d2 <= sum * sum;
}

void collision_merge(Body *a, Body *b, CollisionEvent *outEvent,
                     size_t indexA, size_t indexB)
{
    if (!a || !b) return;

    /* The heavier body survives so that the dominant trail keeps running. */
    Body  *keep = (a->mass >= b->mass) ? a : b;
    Body  *gone = (keep == a) ? b : a;
    size_t keepIndex = (keep == a) ? indexA : indexB;
    size_t goneIndex = (keep == a) ? indexB : indexA;

    double m1 = keep->mass;
    double m2 = gone->mass;
    double m  = m1 + m2;

    if (m > 0.0)
    {
        keep->velocity = vec3_scale(vec3_add(vec3_scale(keep->velocity, m1),
                                             vec3_scale(gone->velocity, m2)), 1.0 / m);
        keep->position = vec3_scale(vec3_add(vec3_scale(keep->position, m1),
                                             vec3_scale(gone->position, m2)), 1.0 / m);
    }

    keep->mass   = m;
    keep->radius = body_merged_radius(keep->radius, gone->radius);

    gone->active       = 0;
    gone->mass         = 0.0;
    gone->velocity     = vec3_zero();
    gone->acceleration = vec3_zero();

    if (outEvent)
    {
        outEvent->occurred      = 1;
        outEvent->survivorIndex = keepIndex;
        outEvent->absorbedIndex = goneIndex;
        snprintf(outEvent->survivorName, BODY_NAME_LEN, "%s", keep->name);
        snprintf(outEvent->absorbedName, BODY_NAME_LEN, "%s", gone->name);
    }
}

int collision_resolve_all(Body *bodies, size_t count, CollisionMode mode,
                          CollisionEvent *lastEvent)
{
    if (!bodies || mode == COLLISION_MODE_NONE) return 0;

    int merges = 0;
    for (size_t i = 0; i < count; ++i)
    {
        if (!bodies[i].active) continue;
        for (size_t j = i + 1; j < count; ++j)
        {
            if (!bodies[j].active) continue;
            if (collision_detect(&bodies[i], &bodies[j]))
            {
                collision_merge(&bodies[i], &bodies[j], lastEvent, i, j);
                merges++;
            }
        }
    }

    /* Masses and body count changed, so accelerations are stale. */
    return merges;
}
