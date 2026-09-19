#include <stdlib.h>
#include <string.h>

#include "trail.h"

#define TRAIL_MIN_LENGTH 2u
#define TRAIL_MAX_LENGTH 200000u

static size_t clamp_length(size_t n)
{
    if (n < TRAIL_MIN_LENGTH) return TRAIL_MIN_LENGTH;
    if (n > TRAIL_MAX_LENGTH) return TRAIL_MAX_LENGTH;
    return n;
}

void trail_init(Trail *trail, size_t maxLength)
{
    if (!trail) return;
    memset(trail, 0, sizeof(*trail));
    maxLength = clamp_length(maxLength);
    trail->points = (Vec3 *)malloc(maxLength * sizeof(Vec3));
    trail->capacity  = trail->points ? maxLength : 0;
    trail->maxLength = trail->capacity;
}

void trail_free(Trail *trail)
{
    if (!trail) return;
    free(trail->points);
    memset(trail, 0, sizeof(*trail));
}

void trail_clear(Trail *trail)
{
    if (!trail) return;
    trail->count = 0;
    trail->head  = 0;
}

void trail_push(Trail *trail, Vec3 point)
{
    if (!trail || !trail->points || trail->maxLength == 0) return;
    if (!vec3_is_finite(point)) return; /* never record NaN/Inf */

    if (trail->count < trail->maxLength)
    {
        size_t slot = (trail->head + trail->count) % trail->capacity;
        trail->points[slot] = point;
        trail->count++;
    }
    else
    {
        /* Full: overwrite the oldest slot and advance the head. */
        trail->points[trail->head] = point;
        trail->head = (trail->head + 1) % trail->capacity;
    }
}

Vec3 trail_get(const Trail *trail, size_t index)
{
    if (!trail || !trail->points || index >= trail->count) return vec3_zero();
    return trail->points[(trail->head + index) % trail->capacity];
}

size_t trail_count(const Trail *trail)
{
    return trail ? trail->count : 0;
}

void trail_set_max_length(Trail *trail, size_t maxLength)
{
    if (!trail) return;
    maxLength = clamp_length(maxLength);
    if (maxLength == trail->maxLength) return;

    Vec3 *buffer = (Vec3 *)malloc(maxLength * sizeof(Vec3));
    if (!buffer) return; /* keep the old trail rather than losing it */

    /* Copy the newest min(count, maxLength) points, oldest first. */
    size_t keep  = trail->count < maxLength ? trail->count : maxLength;
    size_t first = trail->count - keep;
    for (size_t i = 0; i < keep; ++i) buffer[i] = trail_get(trail, first + i);

    free(trail->points);
    trail->points    = buffer;
    trail->capacity  = maxLength;
    trail->maxLength = maxLength;
    trail->count     = keep;
    trail->head      = 0;
}
