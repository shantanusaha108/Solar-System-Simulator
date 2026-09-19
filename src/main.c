/*
 * main.c - application entry point.
 *
 * Responsibilities are deliberately thin: create the window, own the four
 * subsystems, and drive them in the right order each frame.
 *
 *     input -> simulation (fixed timestep) -> camera -> renderer -> UI
 *
 * No gravitational arithmetic appears anywhere in this file, and nothing in
 * simulation/physics/integrator/collision/trail knows that raylib exists.
 */
#include <math.h>
#include <stdio.h>

#include "raylib.h"

#include "camera_sim.h"
#include "renderer.h"
#include "simulation.h"
#include "theme.h"
#include "ui.h"
#include "app_reset.h"

#define WINDOW_WIDTH  1440
#define WINDOW_HEIGHT 900
#define TARGET_FPS    60

/* Keyboard shortcuts. Suppressed entirely while a numeric field in the panel
   has focus, or typing a value would also toggle pause, reset the run and
   flip the camera on the way past. */
static void handle_shortcuts(Simulation *sim, SimCamera *cam, UiState *ui,
                             Renderer *renderer)
{
    if (ui_is_editing(ui)) return;

    if (IsKeyPressed(KEY_SPACE)) simulation_toggle_pause(sim);

    if (IsKeyPressed(KEY_R))
    {
        application_factory_reset(sim, cam, renderer, ui); /* same as RESET */
    }

    if (IsKeyPressed(KEY_F))
    {
        camera_set_mode(cam, cam->mode == CAMERA_MODE_FREE ? CAMERA_MODE_FOLLOW
                                                           : CAMERA_MODE_FREE,
                        sim);
        if (cam->mode == CAMERA_MODE_FOLLOW)
            camera_set_follow_target(cam, ui->followTarget, sim);
        ui->cameraMode = (cam->mode == CAMERA_MODE_FOLLOW) ? 1 : 0;
    }
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Solar System Physics Simulation");
    SetTargetFPS(TARGET_FPS);

    /* Escape cancels an in-progress numeric edit in the panel, so it cannot
       also be raylib's built-in quit key - the first attempt to correct a
       typo would close the window. Quitting on Escape is reinstated by hand
       in the loop below, for every frame in which no field has focus. */
    SetExitKey(KEY_NULL);

    Simulation sim;
    SimCamera  cam;
    Renderer   renderer;
    UiState    ui;

    simulation_init(&sim);
    camera_init(&cam);
    renderer_init(&renderer);
    ui_init(&ui, &sim, &cam, &renderer);

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_ESCAPE) && !ui_is_editing(&ui)) break;

        float frameTime = GetFrameTime();

        /* Refresh hover/drag-capture state for THIS frame before anything
           else touches the mouse, so the camera below sees an up-to-date
           answer rather than last frame's (item 2). ui_draw() also calls
           this; doing it here first is what actually lets us gate
           camera_update with it in the same frame. */
        ui_begin_frame(&ui);

        handle_shortcuts(&sim, &cam, &ui, &renderer);

        /* Fixed timestep: the number of steps per frame is the speed control,
           dt itself never grows with the frame time. */
        simulation_update(&sim, (double)frameTime);

        /* Purely-visual background state (meteors) advances on real time,
           independent of the physics pause. */
        renderer_update(&renderer, frameTime);

        /* Camera input is suppressed for the whole duration of a drag that
           started on the UI, not just while the pointer is currently over
           it - see ui_wants_mouse(). */
        camera_update(&cam, &sim, frameTime, !ui_wants_mouse(&ui));

        BeginDrawing();
        ClearBackground(THEME_BG);

        renderer_draw(&renderer, &sim, &cam);
        /* Both overlays are drawn by the UI module, so they share its fonts
           and palette instead of being ad-hoc DrawText calls (item 8). */
        ui_draw(&ui, &sim, &cam, &renderer);
        ui_draw_hud(&ui, &sim, &cam);

        EndDrawing();
    }

    ui_unload(&ui);
    renderer_unload(&renderer);
    simulation_free(&sim);
    CloseWindow();
    return 0;
}
