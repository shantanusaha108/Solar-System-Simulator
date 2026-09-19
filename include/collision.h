/*
 * collision.h - sphere collision detection and response.
 *
 * Bodies are treated as spheres using their PHYSICAL radius (never the
 * enlarged render radius). A collision is:  |p2 - p1| <= r1 + r2.
 *
 * Response for this version is a perfectly inelastic merger that conserves
 * mass and linear momentum. The heavier body survives (so its identity and
 * recorded trail continue); the lighter one is deactivated rather than being
 * removed from the array, which keeps body indices stable for the renderer.
 */
#ifndef COLLISION_H
#define COLLISION_H

#include <stddef.h>

#include "body.h"

typedef enum
{
    COLLISION_MODE_NONE = 0,
    COLLISION_MODE_MERGE = 1
} CollisionMode;

typedef struct
{
    int    occurred;
    size_t survivorIndex;
    size_t absorbedIndex;
    char   survivorName[BODY_NAME_LEN];
    char   absorbedName[BODY_NAME_LEN];
} CollisionEvent;

int collision_detect(const Body *a, const Body *b);
void collision_merge(Body *a, Body *b, CollisionEvent *outEvent,
                     size_t indexA, size_t indexB);
/* Returns the number of merges performed during this pass. */
int collision_resolve_all(Body *bodies, size_t count, CollisionMode mode,
                          CollisionEvent *lastEvent);

#endif /* COLLISION_H */
