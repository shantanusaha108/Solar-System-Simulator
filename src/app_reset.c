#include "app_reset.h"

void application_factory_reset(Simulation *sim, SimCamera *cam,
                               Renderer *renderer, UiState *ui)
{
    /* 1+2. Simulation: default config (masses, gravity, orbit geometry,
       translation, speed, trail lengths, collision mode, integrator), then
       a full rebuild (bodies, time, paused, energy, collision state, trails).
       Physical radii come from the same config; visual radii are not here. */
    if (sim) simulation_restore_defaults(sim);

    /* 3. Renderer: visual radii (Star/Earth/Moon), toggles, trail fade,
       spin rates and angles, starfield/galaxy, active meteors. */
    if (renderer) renderer_reset(renderer);

    /* 4. Camera: mode, follow target, orientation, pivot, zoom, tuning. */
    if (cam) camera_reset(cam);

    /* 5. UI-only state: scroll position, in-progress edit, drag capture. */
    if (ui) ui_reset(ui);

    /* 6. Actual state -> widgets (also clears the "staged changes" state,
       since the pending orbit/translation values are re-read from config). */
    if (ui) ui_sync(ui, sim, cam, renderer);
}
