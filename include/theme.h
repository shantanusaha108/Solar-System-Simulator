/*
 * theme.h - the single source of truth for the interface's colours.
 *
 * One restrained palette, shared by the control panel (ui.c) and the
 * information overlay, so the application reads as one designed system
 * rather than raygui's defaults plus ad-hoc greys. The 3D scene deliberately
 * does NOT draw from this palette: body colours are a rendering concern and
 * live in renderer.c (BodyVisual).
 *
 * Usage rule of thumb (item 1 of the polish spec):
 *   - surfaces: BG -> PANEL -> SURFACE -> SURFACE_HI, darkest to lightest
 *   - accent  : used sparingly, for the active portion of a control only
 *   - amber   : reserved for experimental / non-physical controls
 *   - text    : TEXT_PRIMARY for values, TEXT_SECONDARY for labels,
 *               TEXT_MUTED for explanatory copy
 */
#ifndef THEME_H
#define THEME_H

#include "raylib.h"

/* --- surfaces ------------------------------------------------------- */
#define THEME_BG          (Color){ 0x0B, 0x10, 0x20, 0xFF } /* #0B1020 */
#define THEME_PANEL       (Color){ 0x0F, 0x17, 0x2A, 0xF2 } /* #0F172A */
#define THEME_PANEL_SOLID (Color){ 0x0F, 0x17, 0x2A, 0xFF }
#define THEME_SURFACE     (Color){ 0x1E, 0x29, 0x3B, 0xFF } /* #1E293B */
#define THEME_SURFACE_HI  (Color){ 0x24, 0x32, 0x44, 0xFF } /* #243244 */
#define THEME_OVERLAY     (Color){ 0x0B, 0x10, 0x20, 0xD8 } /* translucent HUD */

/* --- lines ---------------------------------------------------------- */
#define THEME_BORDER      (Color){ 0x1E, 0x29, 0x3B, 0xFF }
#define THEME_BORDER_HI   (Color){ 0x33, 0x45, 0x5E, 0xFF }
#define THEME_DIVIDER     (Color){ 0x1E, 0x29, 0x3B, 0xFF }

/* --- accent --------------------------------------------------------- */
#define THEME_ACCENT      (Color){ 0x38, 0xBD, 0xF8, 0xFF } /* #38BDF8 */
#define THEME_ACCENT_DIM  (Color){ 0x22, 0x63, 0x8C, 0xFF }
#define THEME_ACCENT_DEEP (Color){ 0x0E, 0x7A, 0xA8, 0xFF }

/* --- semantic ------------------------------------------------------- */
#define THEME_AMBER       (Color){ 0xFB, 0xBF, 0x24, 0xFF } /* #FBBF24 */
#define THEME_DANGER      (Color){ 0xF8, 0x71, 0x71, 0xFF }

/* --- text ----------------------------------------------------------- */
#define THEME_TEXT        (Color){ 0xE5, 0xE7, 0xEB, 0xFF } /* #E5E7EB */
#define THEME_TEXT_DIM    (Color){ 0x94, 0xA3, 0xB8, 0xFF } /* #94A3B8 */
#define THEME_TEXT_MUTED  (Color){ 0x64, 0x74, 0x8B, 0xFF } /* #64748B */

/* raygui takes packed 0xRRGGBBAA ints rather than Color structs. */
#define THEME_HEX_PANEL       0x0F172AFF
#define THEME_HEX_SURFACE     0x1E293BFF
#define THEME_HEX_SURFACE_HI  0x243244FF
#define THEME_HEX_BORDER      0x1E293BFF
#define THEME_HEX_BORDER_HI   0x33455EFF
#define THEME_HEX_ACCENT      0x38BDF8FF
#define THEME_HEX_ACCENT_DIM  0x22638CFF
#define THEME_HEX_ACCENT_DEEP 0x0E7AA8FF
#define THEME_HEX_TEXT        0xE5E7EBFF
#define THEME_HEX_TEXT_DIM    0x94A3B8FF
#define THEME_HEX_TEXT_MUTED  0x64748BFF
#define THEME_HEX_BG          0x0B1020FF

#endif /* THEME_H */
