/*
 * trail.h - recorded position history for a body.
 *
 * The trail stores ACTUAL integrated positions. Nothing here generates a
 * decorative path; the stretched-spring appearance emerges from orbital
 * motion combined with the common translation of the system.
 *
 * Implemented as a ring buffer so that dropping the oldest point is O(1).
 */
#ifndef TRAIL_H
#define TRAIL_H

#include <stddef.h>
#include "vector3d.h"

typedef struct
{
    Vec3  *points;    /* ring buffer storage                       */
    size_t capacity;  /* allocated slots                           */
    size_t count;     /* number of valid points (<= maxLength)     */
    size_t head;      /* index of the OLDEST point                 */
    size_t maxLength; /* user configurable history length          */
} Trail;

void   trail_init(Trail *trail, size_t maxLength);
void   trail_free(Trail *trail);
void   trail_clear(Trail *trail);
void   trail_push(Trail *trail, Vec3 point);
/* index 0 == oldest, count-1 == newest */
Vec3   trail_get(const Trail *trail, size_t index);
size_t trail_count(const Trail *trail);
/* Growing keeps existing history, shrinking discards the oldest points. */
void   trail_set_max_length(Trail *trail, size_t maxLength);

#endif /* TRAIL_H */
