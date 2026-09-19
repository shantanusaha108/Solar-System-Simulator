/*
 * renderer.c - all raylib drawing.
 *
 * Decomposed per body concern (item 33) rather than one large routine:
 * draw_starfield / draw_galaxy_band / draw_reference_orbits / draw_trails /
 * draw_bodies / draw_meteors, called in order from
 * renderer_draw. Texture/mesh generation happens once in renderer_init and
 * is reused every frame (item 39); only meteor motion and planet spin
 * advance per frame (renderer_update, driven by real time, not sim->time,
 * so the sky keeps moving even while physics is paused).
 */
#include <math.h>
#include <stdlib.h>

#include "raylib.h"

#include "renderer.h"

#define EARTH_TEX_W 256
#define EARTH_TEX_H 128
#define MOON_TEX_W  256
#define MOON_TEX_H  128

/*
 * Earth's axis is tilted away from the orbital normal, so the spin reads as
 * a planet turning rather than a texture sliding sideways: at a tilt the
 * poles stay put while the continents sweep across the disc.
 */
#define EARTH_AXIS_X 0.40f
#define EARTH_AXIS_Y 0.92f
#define MOON_AXIS_X  0.10f
#define MOON_AXIS_Y  0.99f

Vector3 renderer_to_render(Vec3 v)
{
    Vector3 out = { (float)v.x, (float)v.y, (float)v.z };
    return out;
}

/*
 * The Moon's real Earth-relative offset is exaggerated by
 * MOON_VISUAL_ORBIT_SCALE for display only - see renderer.h. Everything
 * that needs "where the Moon should be drawn" goes through this function
 * rather than reading Body.position directly.
 */
static Vec3 moon_display_position(Vec3 moonPos, Vec3 earthPos)
{
    Vec3 rel = vec3_sub(moonPos, earthPos);
    return vec3_add(earthPos, vec3_scale(rel, MOON_VISUAL_ORBIT_SCALE));
}

/* -------------------------------------------------------------------- */
/* Procedural textures, generated once.                                 */
/* -------------------------------------------------------------------- */

static unsigned int rng_state = 0x9e3779b9u;

static unsigned int xorshift32(void)
{
    unsigned int x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

static float rand01(void) { return (float)(xorshift32() % 100000) / 100000.0f; }

static Image make_earth_image(void)
{
    Image img = GenImageColor(EARTH_TEX_W, EARTH_TEX_H, (Color){ 28, 70, 150, 255 });

    /* Continents: irregular blobs made of overlapping soft-edged circles at
       varying green tones, seeded deterministically so re-runs look alike. */
    rng_state = 12345u;
    int blobCount = 34;
    for (int b = 0; b < blobCount; ++b)
    {
        int   cx = (int)(rand01() * EARTH_TEX_W);
        int   cy = (int)(4 + rand01() * (EARTH_TEX_H - 8)); /* avoid poles */
        int   baseR = 9 + (int)(rand01() * 18);
        Color land = (Color){
            (unsigned char)(40  + rand01() * 40),
            (unsigned char)(110 + rand01() * 70),
            (unsigned char)(40  + rand01() * 30),
            255
        };
        int lobes = 4 + (int)(rand01() * 4);
        for (int l = 0; l < lobes; ++l)
        {
            int lx = cx + (int)((rand01() - 0.5f) * baseR * 1.6f);
            int ly = cy + (int)((rand01() - 0.5f) * baseR * 1.2f);
            int r  = (int)(baseR * (0.5f + rand01() * 0.6f));
            /* Wrap horizontally so continents can cross the seam cleanly. */
            ImageDrawCircle(&img, lx,               ly, r, land);
            ImageDrawCircle(&img, lx - EARTH_TEX_W,  ly, r, land);
            ImageDrawCircle(&img, lx + EARTH_TEX_W,  ly, r, land);
        }
    }

    /* Lighter polar caps. */
    Color ice = (Color){ 225, 235, 245, 255 };
    for (int y = 0; y < EARTH_TEX_H; ++y)
    {
        float lat = fabsf((float)y / EARTH_TEX_H - 0.5f) * 2.0f; /* 0 eq, 1 pole */
        if (lat < 0.82f) continue;
        float t = (lat - 0.82f) / 0.18f;
        for (int x = 0; x < EARTH_TEX_W; ++x)
            if (rand01() < t) ImageDrawPixel(&img, x, y, ice);
    }

    return img;
}

static Image make_moon_image(void)
{
    Image img = GenImageColor(MOON_TEX_W, MOON_TEX_H, (Color){ 150, 148, 145, 255 });

    rng_state = 987654321u;

    /* Base shading noise: light/dark patches (maria vs highlands). */
    for (int y = 0; y < MOON_TEX_H; ++y)
        for (int x = 0; x < MOON_TEX_W; ++x)
        {
            int v = 130 + (int)(rand01() * 40.0f) - 20;
            if (v < 90) v = 90;
            if (v > 190) v = 190;
            ImageDrawPixel(&img, x, y, (Color){ (unsigned char)v, (unsigned char)v,
                                               (unsigned char)(v - 4), 255 });
        }

    /* Craters: large, medium and small, each a darker rim + brighter floor. */
    int counts[3] = { 8, 18, 40 };
    int radii[3]  = { 18, 10, 4 };
    for (int cls = 0; cls < 3; ++cls)
    {
        for (int c = 0; c < counts[cls]; ++c)
        {
            int cx = (int)(rand01() * MOON_TEX_W);
            int cy = (int)(rand01() * MOON_TEX_H);
            int r  = radii[cls] + (int)(rand01() * (radii[cls] / 2 + 1));

            Color rim  = (Color){ 100, 98, 95, 255 };
            Color floorC = (Color){ 165, 163, 160, 255 };

            ImageDrawCircle(&img, cx,             cy, r,     rim);
            ImageDrawCircle(&img, cx - MOON_TEX_W, cy, r,     rim);
            ImageDrawCircle(&img, cx + MOON_TEX_W, cy, r,     rim);
            if (r > 3)
            {
                ImageDrawCircle(&img, cx,              cy, r - 3, floorC);
                ImageDrawCircle(&img, cx - MOON_TEX_W, cy, r - 3, floorC);
                ImageDrawCircle(&img, cx + MOON_TEX_W, cy, r - 3, floorC);
            }
        }
    }

    return img;
}

static void init_planet_models(Renderer *renderer)
{
    renderer->texturesLoaded = 0;
    if (!IsWindowReady()) return; /* headless (tests) - skip GPU work */

    Image earthImg = make_earth_image();
    Image moonImg  = make_moon_image();

    renderer->earthTexture = LoadTextureFromImage(earthImg);
    renderer->moonTexture  = LoadTextureFromImage(moonImg);
    UnloadImage(earthImg);
    UnloadImage(moonImg);

    /* Unit-radius spheres; DrawModelEx scales them to the current visual
       radius each frame, so a runtime radius slider needs no regeneration. */
    renderer->earthModel = LoadModelFromMesh(GenMeshSphere(1.0f, 32, 32));
    renderer->moonModel  = LoadModelFromMesh(GenMeshSphere(1.0f, 24, 24));

    SetMaterialTexture(&renderer->earthModel.materials[0], MATERIAL_MAP_ALBEDO,
                       renderer->earthTexture);
    SetMaterialTexture(&renderer->moonModel.materials[0], MATERIAL_MAP_ALBEDO,
                       renderer->moonTexture);

    renderer->texturesLoaded = 1;
}

void renderer_unload(Renderer *renderer)
{
    if (!renderer || !renderer->texturesLoaded) return;
    UnloadModel(renderer->earthModel);
    UnloadModel(renderer->moonModel);
    UnloadTexture(renderer->earthTexture);
    UnloadTexture(renderer->moonTexture);
    renderer->texturesLoaded = 0;
}

/* -------------------------------------------------------------------- */
/* Init                                                                  */
/* -------------------------------------------------------------------- */

/*
 * The ONE source of renderer defaults. Restores every user-configurable value
 * (visual radii, colours, toggles, trail fade, spin rates) AND every piece of
 * runtime-generated visual state (spin angles, active meteors, the starfield
 * and galaxy point sets, the meteor RNG) to its deterministic first-launch
 * value. GPU resources (textures / models) are not touched: they are not
 * configuration, and reloading them on Reset would only leak or stall.
 * renderer_init calls this, so startup and Reset cannot drift apart.
 */
void renderer_reset(Renderer *renderer)
{
    if (!renderer) return;

    for (size_t i = 0; i < SIM_MAX_BODIES; ++i)
    {
        renderer->visuals[i].color        = LIGHTGRAY;
        renderer->visuals[i].trailColor   = GRAY;
        renderer->visuals[i].visualRadius = 0.02f;
        renderer->visuals[i].rings        = 12;
        renderer->visuals[i].slices       = 12;
    }

    /* Visual radii are deliberately far larger than the physical radii: at
       true scale the Earth would be roughly 4e-5 units across, i.e. invisible
       (item 34). These are render-only values - Body.radius, which is what
       the collision system uses, is never touched to make something easier
       to see. All three are raised substantially here (items 10-12) while
       keeping the hierarchy Sun > Earth > Moon legible at a glance, and the
       Moon's ORBIT is exaggerated separately at draw time
       (MOON_VISUAL_ORBIT_SCALE, see renderer.h) by a factor large enough
       that the enlarged Moon still clears Earth's enlarged disc. */
    renderer->visuals[SIM_STAR].color        = (Color){ 255, 214, 102, 255 };
    renderer->visuals[SIM_STAR].trailColor   = (Color){ 255, 190,  90, 255 };
    renderer->visuals[SIM_STAR].visualRadius = 0.280f;
    renderer->visuals[SIM_STAR].rings        = 20;
    renderer->visuals[SIM_STAR].slices       = 20;

    renderer->visuals[SIM_EARTH].color        = (Color){  92, 156, 255, 255 };
    renderer->visuals[SIM_EARTH].trailColor   = (Color){ 130, 190, 255, 255 };
    renderer->visuals[SIM_EARTH].visualRadius = 0.110f;
    renderer->visuals[SIM_EARTH].rings        = 16;
    renderer->visuals[SIM_EARTH].slices       = 16;

    renderer->visuals[SIM_MOON].color        = (Color){ 190, 190, 195, 255 };
    renderer->visuals[SIM_MOON].trailColor   = (Color){ 165, 165, 175, 255 };
    renderer->visuals[SIM_MOON].visualRadius = 0.045f;

    renderer->trailFadeExponent   = 2.2f;
    renderer->showTrails          = 1;
    renderer->showStarfield       = 1;
    renderer->showOrbitPlaneGrid  = 0;
    renderer->showReferenceOrbits = 1;
    renderer->showMeteors         = 1;
    renderer->showGalaxy          = 1;

    /* Static procedural star field: directions only, drawn at a fixed radius
       around the camera so the background never appears to be approached.
       Three brightness classes (item 19): ~80% faint, ~15% medium, ~5%
       bright, with a slight, mostly-white color tint (item 18). */
    renderer->starfieldRadius = 260.0f;
    rng_state = 0xc0ffee11u;
    for (int i = 0; i < STARFIELD_COUNT; ++i)
    {
        float u     = rand01() * 2.0f - 1.0f;
        float theta = rand01() * 6.2831853f;
        float s     = sqrtf(1.0f - u * u);
        renderer->starfield[i] = (Vector3){ s * cosf(theta), u, s * sinf(theta) };

        float cls = rand01();
        unsigned char alpha;
        float sizeJitter; /* used only to pick how many sub-points to stack */
        if (cls < 0.80f)      { alpha = (unsigned char)(45  + rand01() * 40); sizeJitter = 0.0f; }
        else if (cls < 0.95f) { alpha = (unsigned char)(110 + rand01() * 60); sizeJitter = 0.0f; }
        else                  { alpha = (unsigned char)(200 + rand01() * 55); sizeJitter = 1.0f; }

        float tint = rand01();
        Color c;
        if (tint < 0.55f)      c = (Color){ 255, 255, 255, alpha };        /* white   */
        else if (tint < 0.75f) c = (Color){ 200, 215, 255, alpha };        /* blue-white */
        else if (tint < 0.90f) c = (Color){ 255, 244, 214, alpha };        /* warm/yellow */
        else                   c = (Color){ 255, 220, 180, alpha };        /* orange  */

        /* Encode the "bright" class in an otherwise-unused high bit of alpha
           range is fragile, so just remember it via a second, cheap pass:
           bright stars additionally get drawn with a couple of neighbour
           offsets in draw_starfield. We store that decision implicitly by
           giving bright stars alpha >= 200, checked there. */
        (void)sizeJitter;
        renderer->starfieldColor[i] = c;
    }

    /* Subtle galaxy band: extra faint points biased toward a fixed plane, so
       they read as a soft band across the sky rather than uniform clutter
       (item 21). Kept low-alpha and grayish-blue so the planets stay the
       visual focus. */
    Vector3 bandNormal = { 0.35f, 0.92f, 0.17f };
    float   bn = sqrtf(bandNormal.x * bandNormal.x + bandNormal.y * bandNormal.y +
                       bandNormal.z * bandNormal.z);
    bandNormal.x /= bn; bandNormal.y /= bn; bandNormal.z /= bn;

    int placed = 0;
    while (placed < GALAXY_BAND_COUNT)
    {
        float u     = rand01() * 2.0f - 1.0f;
        float theta = rand01() * 6.2831853f;
        float s     = sqrtf(1.0f - u * u);
        Vector3 d   = { s * cosf(theta), u, s * sinf(theta) };
        float dot   = d.x * bandNormal.x + d.y * bandNormal.y + d.z * bandNormal.z;
        /* Reject points far from the band plane; keep density near dot==0. */
        if (rand01() > powf(1.0f - fabsf(dot), 6.0f)) continue;

        renderer->galaxyPoints[placed] = d;
        unsigned char a = (unsigned char)(10 + rand01() * 26);
        renderer->galaxyColor[placed]  = (Color){ 190, 200, 225, a };
        placed++;
    }

    /* Visual spin (items 13-16). 45 deg/s is one turn every 8 seconds:
       unmistakably rotating at a glance, without the blur of a planet
       spinning several times a second. The Moon is slower still, so the two
       never look like the same object. */
    renderer->earthSpin          = 0.0f;
    renderer->moonSpin           = 0.0f;
    renderer->earthSpinDegPerSec = 45.0f;
    renderer->moonSpinDegPerSec  = 14.0f;

    /* Clear every meteor entirely (not just the active flag) so no runtime
       effect survives a Reset, then re-arm the spawn timer from the same RNG
       state every time. */
    for (int i = 0; i < MAX_METEORS; ++i) renderer->meteors[i] = (Meteor){ 0 };
    renderer->meteorSpawnTimer = 3.0f + rand01() * 6.0f;
}

void renderer_init(Renderer *renderer)
{
    if (!renderer) return;
    renderer_reset(renderer);
    init_planet_models(renderer);
}

/* -------------------------------------------------------------------- */
/* Per-frame, real-time visual update (not physics time - see header)   */
/* -------------------------------------------------------------------- */

void renderer_update(Renderer *renderer, float frameTime)
{
    if (!renderer) return;
    if (frameTime < 0.0f || frameTime > 0.25f) frameTime = 0.016f; /* guard spikes */

    /* Frame-rate independent axial spin: an angular RATE integrated over the
       elapsed time, never a per-frame constant. Wrapped rather than left to
       grow, so precision stays even after hours of running. Advanced before
       the meteor early-out below, which used to skip it whenever meteors
       were switched off. */
    renderer->earthSpin = fmodf(renderer->earthSpin +
                                renderer->earthSpinDegPerSec * frameTime, 360.0f);
    renderer->moonSpin  = fmodf(renderer->moonSpin +
                                renderer->moonSpinDegPerSec * frameTime, 360.0f);

    if (!renderer->showMeteors) return;

    renderer->meteorSpawnTimer -= frameTime;
    if (renderer->meteorSpawnTimer <= 0.0f)
    {
        for (int i = 0; i < MAX_METEORS; ++i)
        {
            if (renderer->meteors[i].active) continue;

            float u     = rand01() * 2.0f - 1.0f;
            float theta = rand01() * 6.2831853f;
            float s     = sqrtf(1.0f - u * u);
            Vector3 pos = { s * cosf(theta) * renderer->starfieldRadius * 0.9f,
                            u * renderer->starfieldRadius * 0.9f,
                            s * sinf(theta) * renderer->starfieldRadius * 0.9f };

            /* Roughly tangential random direction, not radial, so it reads
               as crossing the sky rather than approaching the camera. */
            Vector3 dir = { rand01() * 2.0f - 1.0f, rand01() * 2.0f - 1.0f,
                            rand01() * 2.0f - 1.0f };
            float dl = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (dl < 1e-4f) dl = 1.0f;
            dir.x /= dl; dir.y /= dl; dir.z /= dl;

            renderer->meteors[i] = (Meteor){
                .position = pos, .direction = dir,
                .speed = 60.0f + rand01() * 60.0f,
                .life = 0.0f, .maxLife = 0.5f + rand01() * 0.6f,
                .active = 1
            };
            break;
        }
        renderer->meteorSpawnTimer = 4.0f + rand01() * 10.0f; /* infrequent */
    }

    for (int i = 0; i < MAX_METEORS; ++i)
    {
        Meteor *m = &renderer->meteors[i];
        if (!m->active) continue;
        m->position.x += m->direction.x * m->speed * frameTime;
        m->position.y += m->direction.y * m->speed * frameTime;
        m->position.z += m->direction.z * m->speed * frameTime;
        m->life += frameTime;
        if (m->life >= m->maxLife) m->active = 0;
    }
}

/* -------------------------------------------------------------------- */
/* Drawing, split per concern (item 33)                                  */
/* -------------------------------------------------------------------- */

static void draw_starfield(const Renderer *renderer, const SimCamera *cam)
{
    Vector3 eye = cam->camera.position;
    float   r   = renderer->starfieldRadius;

    for (int i = 0; i < STARFIELD_COUNT; ++i)
    {
        Vector3 d = renderer->starfield[i];
        Vector3 p = { eye.x + d.x * r, eye.y + d.y * r, eye.z + d.z * r };
        Color   c = renderer->starfieldColor[i];
        DrawPoint3D(p, c);

        /* Bright class (alpha >= 200, see renderer_init): a few extra
           sub-pixel offsets to read as a slightly larger, subtly glowing
           point without the cost of real billboards. */
        if (c.a >= 200)
        {
            Color halo = Fade(c, 0.35f);
            DrawPoint3D((Vector3){ p.x + 0.15f, p.y, p.z }, halo);
            DrawPoint3D((Vector3){ p.x - 0.15f, p.y, p.z }, halo);
            DrawPoint3D((Vector3){ p.x, p.y + 0.15f, p.z }, halo);
            DrawPoint3D((Vector3){ p.x, p.y - 0.15f, p.z }, halo);
        }
    }
}

static void draw_galaxy_band(const Renderer *renderer, const SimCamera *cam)
{
    Vector3 eye = cam->camera.position;
    float   r   = renderer->starfieldRadius;

    for (int i = 0; i < GALAXY_BAND_COUNT; ++i)
    {
        Vector3 d = renderer->galaxyPoints[i];
        Vector3 p = { eye.x + d.x * r, eye.y + d.y * r, eye.z + d.z * r };
        DrawPoint3D(p, renderer->galaxyColor[i]);
    }
}

/* Dashed circle in the X-Z plane (the orbital plane), centered on 'center'. */
static void draw_dashed_circle_xz(Vector3 center, float radius, Color color)
{
    const int segments = 96;
    const int dashOn = 2, dashOff = 1, period = dashOn + dashOff;

    for (int i = 0; i < segments; ++i)
    {
        if ((i % period) >= dashOn) continue;
        float t0 = (float)i       / segments * 2.0f * PI;
        float t1 = (float)(i + 1) / segments * 2.0f * PI;
        Vector3 p0 = { center.x + radius * cosf(t0), center.y, center.z + radius * sinf(t0) };
        Vector3 p1 = { center.x + radius * cosf(t1), center.y, center.z + radius * sinf(t1) };
        DrawLine3D(p0, p1, color);
    }
}

/*
 * Analytical reference orbits (items 11/12): NOT built from trail history.
 * Earth's circle tracks the Star's current position at the real orbit
 * radius; the Moon's tracks Earth's current position, using the same
 * MOON_VISUAL_ORBIT_SCALE exaggeration applied to the Moon's trail/sphere so
 * all three stay visually consistent with each other.
 */
static void draw_reference_orbits(const Renderer *renderer, const Simulation *sim)
{
    const Body *star  = simulation_body(sim, SIM_STAR);
    const Body *earth = simulation_body(sim, SIM_EARTH);

    if (star && star->active)
    {
        Vector3 c = renderer_to_render(star->position);
        draw_dashed_circle_xz(c, (float)sim->config.earthOrbitRadius,
                              Fade(renderer->visuals[SIM_EARTH].trailColor, 0.35f));
    }
    if (earth && earth->active)
    {
        Vector3 c = renderer_to_render(earth->position);
        float radius = (float)(sim->config.moonOrbitRadius * MOON_VISUAL_ORBIT_SCALE);
        draw_dashed_circle_xz(c, radius,
                              Fade(renderer->visuals[SIM_MOON].trailColor, 0.55f));
    }
}

/* Ordinary trail: thin fading line built only from recorded positions. */
static void draw_trail(const Trail *trail, Color color, float fadeExponent)
{
    size_t count = trail_count(trail);
    if (count < 2) return;

    float inv = 1.0f / (float)(count - 1);

    for (size_t i = 1; i < count; ++i)
    {
        Vector3 a = renderer_to_render(trail_get(trail, i - 1));
        Vector3 b = renderer_to_render(trail_get(trail, i));

        float age   = (float)i * inv;
        float alpha = powf(age, fadeExponent);
        if (alpha < 0.004f) continue;

        DrawLine3D(a, b, Fade(color, alpha));
    }
}

/*
 * Moon trail with the same real trajectory but the exaggerated Earth-
 * relative offset (item 14). Each Moon sample is paired with the Earth
 * sample recorded at the same simulation step: trail_push happens for every
 * body together in simulation_step, so "N-th from the newest end" identifies
 * the same instant in both trails regardless of their (possibly different)
 * configured lengths. This is a real recorded trajectory, exaggerated only
 * at draw time - not a synthesized decorative path (item 38).
 */
static void draw_moon_trail(const Trail *moonTrail, const Trail *earthTrail,
                            Color color, float fadeExponent)
{
    size_t mCount = trail_count(moonTrail);
    size_t eCount = trail_count(earthTrail);
    if (mCount < 2) return;

    float inv = 1.0f / (float)(mCount - 1);

    for (size_t i = 1; i < mCount; ++i)
    {
        size_t idxA = i - 1;
        size_t idxB = i;

        Vec3 moonA = trail_get(moonTrail, idxA);
        Vec3 moonB = trail_get(moonTrail, idxB);

        size_t earthAgeA = mCount - idxA;  /* samples from the newest */
        size_t earthAgeB = mCount - idxB;

        Vec3 earthA = (earthAgeA <= eCount)
                        ? trail_get(earthTrail, eCount - earthAgeA)
                        : trail_get(earthTrail, 0);
        Vec3 earthB = (earthAgeB <= eCount)
                        ? trail_get(earthTrail, eCount - earthAgeB)
                        : trail_get(earthTrail, 0);

        Vector3 a = renderer_to_render(moon_display_position(moonA, earthA));
        Vector3 b = renderer_to_render(moon_display_position(moonB, earthB));

        float age   = (float)i * inv;
        float alpha = powf(age, fadeExponent);
        if (alpha < 0.004f) continue;

        DrawLine3D(a, b, Fade(color, alpha));
    }
}

static void draw_body(const Renderer *renderer, const Simulation *sim, size_t index)
{
    const Body *body = simulation_body(sim, index);
    if (!body || !body->active) return;

    const BodyVisual *v = &renderer->visuals[index];
    float r = v->visualRadius > 0.0f ? v->visualRadius : 0.01f;

    Vector3 p;
    if (index == SIM_MOON)
    {
        const Body *earth = simulation_body(sim, SIM_EARTH);
        Vec3 earthPos = earth && earth->active ? earth->position : vec3_zero();
        p = renderer_to_render(moon_display_position(body->position, earthPos));
    }
    else
    {
        p = renderer_to_render(body->position);
    }

    /* The spin angle comes from renderer_update (real elapsed time), and the
       texture is on the sphere's material - so the continents themselves
       sweep across the disc as the angle advances, rather than any geometry
       being drawn on top of a static ball. Nothing here reads or writes
       physics state. */
    if (index == SIM_EARTH && renderer->texturesLoaded)
    {
        DrawModelEx(renderer->earthModel, p,
                   (Vector3){ EARTH_AXIS_X, EARTH_AXIS_Y, 0.0f },
                   renderer->earthSpin, (Vector3){ r, r, r }, WHITE);
    }
    else if (index == SIM_MOON && renderer->texturesLoaded)
    {
        DrawModelEx(renderer->moonModel, p,
                   (Vector3){ MOON_AXIS_X, MOON_AXIS_Y, 0.0f },
                   renderer->moonSpin, (Vector3){ r, r, r }, WHITE);
    }
    else
    {
        DrawSphereEx(p, r, v->rings, v->slices, v->color);
    }

    if (index == SIM_STAR)
    {
        /* Subtle glow via a couple of oversized, low-alpha spheres rather
           than many transparent objects (item 17/39). */
        DrawSphereEx(p, r * 1.30f, 16, 16, Fade(v->color, 0.16f));
        DrawSphereEx(p, r * 1.65f, 14, 14, Fade((Color){ 255, 235, 190, 255 }, 0.09f));
        DrawSphereEx(p, r * 2.10f, 12, 12, Fade((Color){ 255, 235, 190, 255 }, 0.04f));
    }
}

static void draw_meteors(const Renderer *renderer)
{
    for (int i = 0; i < MAX_METEORS; ++i)
    {
        const Meteor *m = &renderer->meteors[i];
        if (!m->active) continue;

        float t = m->life / m->maxLife; /* 0 new .. 1 about to vanish */
        float headAlpha = 1.0f - t;
        float tailLen = 6.0f + 10.0f * (1.0f - t);

        Vector3 tail = { m->position.x - m->direction.x * tailLen,
                         m->position.y - m->direction.y * tailLen,
                         m->position.z - m->direction.z * tailLen };

        DrawLine3D(tail, m->position, Fade((Color){ 255, 250, 230, 255 }, headAlpha * 0.8f));
        DrawPoint3D(m->position, Fade(WHITE, headAlpha));
    }
}

void renderer_draw(const Renderer *renderer, const Simulation *sim,
                   const SimCamera *cam)
{
    if (!renderer || !sim || !cam) return;

    BeginMode3D(cam->camera);

    if (renderer->showStarfield)
    {
        draw_starfield(renderer, cam);
        if (renderer->showGalaxy) draw_galaxy_band(renderer, cam);
    }
    if (renderer->showOrbitPlaneGrid) DrawGrid(20, 1.0f);
    if (renderer->showReferenceOrbits) draw_reference_orbits(renderer, sim);

    if (renderer->showTrails)
    {
        const Trail *earthTrail = &sim->bodies[SIM_EARTH].trail;
        for (size_t i = 0; i < sim->bodyCount; ++i)
        {
            if (i == SIM_MOON)
                draw_moon_trail(&sim->bodies[SIM_MOON].trail, earthTrail,
                               renderer->visuals[SIM_MOON].trailColor,
                               renderer->trailFadeExponent);
            else
                draw_trail(&sim->bodies[i].trail, renderer->visuals[i].trailColor,
                          renderer->trailFadeExponent);
        }
    }

    for (size_t i = 0; i < sim->bodyCount; ++i)
        draw_body(renderer, sim, i);

    if (renderer->showMeteors) draw_meteors(renderer);

    EndMode3D();
    /* No body-name labels are drawn in the 3D scene (item 9): the bodies
       read for themselves at these render radii, and the panel and the
       overlay carry the naming. Body.name is untouched - the collision
       system still reports through it. */
}
