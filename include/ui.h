/*
 * ui.h - the control panel and the information overlay.
 *
 * The UI only ever talks to the simulation through the simulation_* API or by
 * editing sim->config; it never reaches into body state directly.
 *
 * LIVE vs INITIAL-CONDITION controls (item 27):
 *   Live (applied immediately, every frame the slider is touched): masses,
 *   gravity multipliers, trail lengths, speed, camera, rendering toggles.
 *   Initial conditions (staged here, applied only by APPLY CHANGES, which
 *   also recomputes the matching circular velocity; RESET is a separate,
 *   whole-application factory reset - see app_reset.h): Earth-Star distance,
 *   Moon-Earth distance, system translation speed.
 *
 * MOUSE CAPTURE (item 2):
 *   ui->mouseCaptured latches to true the moment a left-click begins over the
 *   panel, and stays true until the button is released - regardless of where
 *   the mouse moves meanwhile. ui_wants_mouse() ORs that with a plain
 *   hover test, and the caller (main.c) uses it to gate ALL camera input,
 *   not just an instantaneous "is the pointer over the panel right now"
 *   check, which is what let a drag that started on a slider also spin the
 *   camera once the mouse crossed back over the 3D view.
 *
 * TYPOGRAPHY:
 *   Two real TTF faces are loaded once at startup (regular + semibold) and
 *   reused for every piece of text the application draws, the FPS/status
 *   overlay included. Nothing is ever loaded per frame.
 *
 * DIRECT NUMERIC ENTRY:
 *   Every slider's readout is itself a control: click it and it becomes a
 *   text field. Enter commits (clamped to the slider's own min/max), Escape
 *   restores the previous value, clicking elsewhere commits. Exactly one
 *   field can be editing at a time, identified by the address of the float
 *   the slider drives (editTarget) rather than by a layout index, so the
 *   identity survives the panel's conditional rows.
 */
#ifndef UI_H
#define UI_H

#include "raylib.h"

#include "camera_sim.h"
#include "renderer.h"
#include "simulation.h"

#define UI_EDIT_BUF 32

typedef struct
{
    float     panelWidth;
    Rectangle panelBounds;   /* the fixed on-screen panel rect        */
    Rectangle clipRect;      /* visible (scissored) area of the panel */
    Vector2   scroll;        /* raygui GuiScrollPanel scroll offset   */
    float     contentHeight; /* total (unclipped) content height      */

    int mouseCaptured;  /* latched true for the whole drag - see above */
    int mouseOverPanel; /* plain hover test, refreshed every frame     */

    /* Fonts, loaded once in ui_init. Both fall back to raylib's default
       font if no TTF could be found anywhere. */
    Font uiFont;
    Font uiFontBold;
    int  fontLoaded;

    /* Custom slider drag: address of the float being dragged, or NULL. */
    const void *dragTarget;

    /* Direct numeric entry (see header note). */
    const void *editTarget;
    char        editBuf[UI_EDIT_BUF];
    int         editLen;
    float       editRepeatTimer; /* backspace auto-repeat */

    /* staged initial conditions (applied on Reset) */
    float pendingEarthOrbit;
    float pendingMoonOrbit;
    float pendingTranslation;

    /* live values, edited as log10 exponents because the masses span decades */
    float starMassExp;
    float earthMassExp;
    float moonMassExp;
    float starGravity;
    float earthGravity;
    float moonGravity;

    float starTrail;
    float earthTrail;
    float moonTrail;

    float speed;
    float zoom;

    int cameraMode;   /* 0 FREE, 1 FOLLOW                             */
    int followTarget; /* 0 Star, 1 Earth, 2 Moon (no "System" option) */

    bool showTrails;
    bool showStarfield;
    bool showGrid;
    bool showReferenceOrbits;
    bool showMeteors;
    bool showGalaxy;
    bool collisionsEnabled;
} UiState;

void ui_init(UiState *ui, const Simulation *sim, const SimCamera *cam,
             const Renderer *renderer);
void ui_unload(UiState *ui);
/* Factory reset of the UI's own state: panel scroll back to the top, any
   in-progress numeric edit / slider drag / mouse capture dropped. It does NOT
   touch widget values - those are pulled from the real state by ui_sync(). */
void ui_reset(UiState *ui);
/* Pulls widget values back from the simulation (after a Reset). */
void ui_sync(UiState *ui, const Simulation *sim, const SimCamera *cam,
            const Renderer *renderer);
/* Draws the panel and applies any changes. */
void ui_draw(UiState *ui, Simulation *sim, SimCamera *cam, Renderer *renderer);
/* Draws the top-left information overlay (FPS/status/distances/camera) and
   the shortcut strip, using the same fonts and palette as the panel. */
void ui_draw_hud(const UiState *ui, const Simulation *sim, const SimCamera *cam);
/* True while the pointer is over the panel (plain hover, no drag memory). */
int  ui_mouse_over(const UiState *ui);
/* True if the camera should ignore mouse input this frame: hovering the
   panel OR a drag that started on a control is still in progress. Call this
   (not ui_mouse_over) to gate camera_update. */
int  ui_wants_mouse(const UiState *ui);
/* True while a numeric field has keyboard focus. Callers must suppress their
   own keyboard shortcuts while this holds, or typing into a field would also
   trigger whatever those characters are bound to. */
int  ui_is_editing(const UiState *ui);
/* Updates panelBounds/mouseOverPanel/mouseCaptured for this frame. Call once,
   before camera_update, so the camera sees this frame's capture state rather
   than last frame's. ui_draw() also refreshes it (idempotent - the mouse
   position doesn't change mid-frame) so widgets stay in sync. */
void ui_begin_frame(UiState *ui);

#endif /* UI_H */
