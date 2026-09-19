/*
 * app_reset.h - the single, centralised factory reset.
 *
 * Every user-configurable value lives in exactly one of four subsystems, and
 * each subsystem owns its own defaults:
 *
 *     Simulation -> simulation_default_config() / simulation_restore_defaults()
 *     Camera     -> camera_reset()
 *     Renderer   -> renderer_reset()
 *     UI         -> ui_reset()  (panel-only state; values come from ui_sync)
 *
 * application_factory_reset() reaches all of them, then pulls the restored
 * state into the widgets, so the UI can never show a stale value.
 * APPLY CHANGES is a different operation and does not use this.
 */
#ifndef APP_RESET_H
#define APP_RESET_H

#include "camera_sim.h"
#include "renderer.h"
#include "simulation.h"
#include "ui.h"

void application_factory_reset(Simulation *sim, SimCamera *cam,
                               Renderer *renderer, UiState *ui);

#endif /* APP_RESET_H */
