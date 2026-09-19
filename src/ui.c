/*
 * ui.c - the control panel and the information overlay.
 *
 * raygui is a single-header library: this is the one translation unit that
 * instantiates it.
 *
 * WHAT RAYGUI DRAWS AND WHAT WE DRAW
 *   raygui still owns the scroll panel, the buttons, the toggle groups and
 *   the checkboxes - restyled from its defaults into the palette in
 *   theme.h. Sliders and numeric readouts are drawn here instead
 *   (theme_slider / numeric_field), because the appearance asked for - dark
 *   track, lighter inactive remainder, cyan active section, a distinct knob
 *   with hover and pressed states, and a readout that turns into a text
 *   field on click - is not expressible through raygui's style properties.
 *   No new dependency is introduced: both are plain raylib shapes and text.
 *
 * DATA FLOW (item 36): every live slider below writes straight into the
 * simulation on every frame it is touched, unconditionally - it does NOT
 * gate on a "value changed this frame" flag, because a drag produces no such
 * edge on most frames. That was the original cause of the speed / mass /
 * gravity / trail-length sliders appearing to do nothing.
 * ui_sync() is the only function allowed to write simulation state INTO the
 * UI, and it is only ever called right after init or right after Reset -
 * never once per frame - so it can never fight the user's own dragging.
 *
 * INPUT ISOLATION: every interactive element here tests the pointer against
 * ui->clipRect as well as its own rectangle, so a control scrolled out of
 * sight cannot be grabbed through the panel's edge; and ui_begin_frame
 * latches mouseCaptured for the whole of a drag so the camera stays still.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "theme.h"
#include "ui.h"
#include "app_reset.h"

/* ---------------------------------------------------------------- metrics */

#define PAD            18.0f   /* panel inner margin                        */
#define SECTION_TOP    26.0f   /* space above a section heading             */
#define SECTION_BOTTOM 14.0f   /* space below a section heading rule        */
#define HEADING_H      16.0f
#define LABEL_H        18.0f   /* label / value line                        */
#define SLIDER_H       20.0f   /* slider row (track is centred inside it)   */
#define LABEL_GAP       4.0f   /* label line -> slider                      */
#define ROW_GAP        16.0f   /* slider -> next control                    */
#define BUTTON_H       32.0f
#define FIELD_W       104.0f
#define FIELD_H        20.0f

#define FS_HEADING   13.0f
#define FS_LABEL     14.0f
#define FS_VALUE     14.0f
#define FS_NOTE      12.0f
#define FS_HUD       13.0f
#define FS_HUD_TITLE 12.0f

#define SP_TEXT       0.4f   /* DrawTextEx letter spacing, body text   */
#define SP_HEADING    1.5f   /* headings are tracked out a little      */

/* The default translation direction; the slider scales its magnitude. */
static const Vec3 TRANSLATION_DIR = { 0.9713, 0.2331, 0.0 }; /* unit (0.25,0.06,0) */

/* Layout cursor, reset at the top of every layout_panel() pass. */
static float cursor_y = 0.0f;

/* ------------------------------------------------------------------ text */

static void text_at(Font f, const char *s, float x, float y, float size,
                    float spacing, Color c)
{
    DrawTextEx(f, s, (Vector2){ x, y }, size, spacing, c);
}

static float text_width(Font f, const char *s, float size, float spacing)
{
    return MeasureTextEx(f, s, size, spacing).x;
}

static void text_right(Font f, const char *s, float xRight, float y, float size,
                       float spacing, Color c)
{
    text_at(f, s, xRight - text_width(f, s, size, spacing), y, size, spacing, c);
}

/*
 * Context-appropriate numeric formatting (item 7): plain decimals in the
 * range a reader can hold in their head, scientific notation outside it, and
 * never a wall of meaningless digits. The exponent is normalised from C's
 * "e-06" to "e-6" so the column stays narrow.
 */
static const char *fmt_number(double v)
{
    static char out[64];
    double a = fabs(v);

    if (a == 0.0) return "0";

    if (a >= 1e5 || a < 1e-3)
    {
        char raw[32];
        snprintf(raw, sizeof raw, "%.2e", v);

        char *e = strchr(raw, 'e');
        if (e)
        {
            int exp = atoi(e + 1);
            *e = '\0';
            snprintf(out, sizeof out, "%se%d", raw, exp);
        }
        else
        {
            snprintf(out, sizeof out, "%s", raw);
        }
        return out;
    }

    if (a >= 1000.0)     snprintf(out, sizeof out, "%.0f", v);
    else if (a >= 10.0)  snprintf(out, sizeof out, "%.2f", v);
    else if (a >= 0.01)  snprintf(out, sizeof out, "%.3f", v);
    /* Below 0.01 three decimals would round the Moon's 0.00257 AU orbit to a
       flat 0.003 and hide the very digits that control is there to set. */
    else                 snprintf(out, sizeof out, "%.5f", v);
    return out;
}

/* Same, with a unit suffix appended for display only - the edit buffer is
   always rebuilt from the raw double, so the suffix never has to be parsed
   back out. */
static const char *fmt_unit(double v, const char *unit)
{
    static char out[80];
    snprintf(out, sizeof out, "%s %s", fmt_number(v), unit);
    return out;
}

/* ------------------------------------------------------------------ fonts */

/*
 * One TTF pair, loaded once (item 2). The project ships Inter under the SIL
 * Open Font License in assets/fonts/, and the search also covers a build
 * directory layout and the usual system paths so the binary still finds a
 * real font if it is run from somewhere else. Failing everything, raygui's
 * bitmap font is used rather than aborting the build or the run.
 *
 * The atlas is baked at 48 px and filtered bilinearly, so drawing it at the
 * 12-15 px sizes below stays smooth instead of blocky.
 */
#define UI_FONT_BAKE 48

static Font load_font_candidates(const char **paths, size_t count, int *ok)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!FileExists(paths[i])) continue;
        Font f = LoadFontEx(paths[i], UI_FONT_BAKE, NULL, 0);
        if (f.texture.id != 0)
        {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            *ok = 1;
            return f;
        }
    }
    *ok = 0;
    return GetFontDefault();
}

static void load_ui_fonts(UiState *ui)
{
    static const char *regular[] = {
        "assets/fonts/Inter-Regular.ttf",
        "../assets/fonts/Inter-Regular.ttf",
        "../../assets/fonts/Inter-Regular.ttf",
        "assets/fonts/ui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };
    static const char *bold[] = {
        "assets/fonts/Inter-SemiBold.ttf",
        "../assets/fonts/Inter-SemiBold.ttf",
        "../../assets/fonts/Inter-SemiBold.ttf",
        "assets/fonts/ui-bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };

    int okRegular = 0, okBold = 0;
    ui->uiFont     = load_font_candidates(regular, sizeof regular / sizeof *regular,
                                          &okRegular);
    ui->uiFontBold = load_font_candidates(bold, sizeof bold / sizeof *bold, &okBold);

    if (!okBold && okRegular) ui->uiFontBold = ui->uiFont; /* share the regular */
    ui->fontLoaded = okRegular | (okBold << 1);

    if (okRegular) GuiSetFont(ui->uiFont);
}

/* --------------------------------------------------------------- raygui style */

static void apply_gui_theme(void)
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, THEME_HEX_PANEL);
    GuiSetStyle(DEFAULT, LINE_COLOR, THEME_HEX_BORDER);
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, THEME_HEX_PANEL);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, THEME_HEX_TEXT_DIM);

    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, THEME_HEX_TEXT_DIM);

    /* Buttons: dark surface -> brighter on hover -> cyan when pressed. */
    GuiSetStyle(BUTTON, BORDER_WIDTH, 1);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,   THEME_HEX_SURFACE);
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, THEME_HEX_BORDER_HI);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,   THEME_HEX_TEXT);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,  THEME_HEX_SURFACE_HI);
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED,THEME_HEX_ACCENT_DIM);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,  THEME_HEX_TEXT);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED,  THEME_HEX_ACCENT_DEEP);
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED,THEME_HEX_ACCENT);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,  THEME_HEX_TEXT);

    /* Segmented toggles: the active segment is the only cyan thing. */
    GuiSetStyle(TOGGLE, BORDER_WIDTH, 1);
    GuiSetStyle(TOGGLE, GROUP_PADDING, 4);
    GuiSetStyle(TOGGLE, BASE_COLOR_NORMAL,    THEME_HEX_SURFACE);
    GuiSetStyle(TOGGLE, BORDER_COLOR_NORMAL,  THEME_HEX_BORDER_HI);
    GuiSetStyle(TOGGLE, TEXT_COLOR_NORMAL,    THEME_HEX_TEXT_DIM);
    GuiSetStyle(TOGGLE, BASE_COLOR_FOCUSED,   THEME_HEX_SURFACE_HI);
    GuiSetStyle(TOGGLE, BORDER_COLOR_FOCUSED, THEME_HEX_ACCENT_DIM);
    GuiSetStyle(TOGGLE, TEXT_COLOR_FOCUSED,   THEME_HEX_TEXT);
    GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED,   THEME_HEX_ACCENT_DEEP);
    GuiSetStyle(TOGGLE, BORDER_COLOR_PRESSED, THEME_HEX_ACCENT);
    GuiSetStyle(TOGGLE, TEXT_COLOR_PRESSED,   THEME_HEX_TEXT);

    GuiSetStyle(CHECKBOX, BORDER_WIDTH, 1);
    GuiSetStyle(CHECKBOX, CHECK_PADDING, 3);
    GuiSetStyle(CHECKBOX, BORDER_COLOR_NORMAL,  THEME_HEX_BORDER_HI);
    GuiSetStyle(CHECKBOX, BASE_COLOR_NORMAL,    THEME_HEX_SURFACE);
    GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL,    THEME_HEX_TEXT_DIM);
    GuiSetStyle(CHECKBOX, BORDER_COLOR_FOCUSED, THEME_HEX_ACCENT_DIM);
    GuiSetStyle(CHECKBOX, BASE_COLOR_FOCUSED,   THEME_HEX_SURFACE_HI);
    GuiSetStyle(CHECKBOX, TEXT_COLOR_FOCUSED,   THEME_HEX_TEXT);
    GuiSetStyle(CHECKBOX, BORDER_COLOR_PRESSED, THEME_HEX_ACCENT);
    GuiSetStyle(CHECKBOX, BASE_COLOR_PRESSED,   THEME_HEX_ACCENT);
    GuiSetStyle(CHECKBOX, TEXT_COLOR_PRESSED,   THEME_HEX_TEXT);

    /* The scroll panel borrows LISTVIEW's border and, for its thumb,
       SLIDER's border colours (see GuiScrollBar) - the slider control
       itself is drawn by hand below, so these are free to repurpose. */
    GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL,  THEME_HEX_BORDER);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_FOCUSED, THEME_HEX_BORDER);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_PRESSED, THEME_HEX_BORDER);
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL,  THEME_HEX_BORDER_HI);
    GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED, THEME_HEX_ACCENT_DIM);
    GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED, THEME_HEX_ACCENT);
    GuiSetStyle(SCROLLBAR, BORDER_WIDTH, 0);
    GuiSetStyle(SCROLLBAR, ARROWS_VISIBLE, 0);
    GuiSetStyle(SCROLLBAR, SCROLL_SLIDER_PADDING, 2);
    GuiSetStyle(SCROLLBAR, SCROLL_PADDING, 2);
}

/* ------------------------------------------------------------------ init */

void ui_init(UiState *ui, const Simulation *sim, const SimCamera *cam,
             const Renderer *renderer)
{
    if (!ui) return;

    ui->panelWidth    = 344.0f;
    ui->scroll        = (Vector2){ 0.0f, 0.0f };
    ui->panelBounds   = (Rectangle){ 0.0f, 0.0f, ui->panelWidth, 0.0f };
    ui->clipRect      = ui->panelBounds;
    ui->contentHeight = 1750.0f; /* generous; measured for real below */
    ui->mouseCaptured  = 0;
    ui->mouseOverPanel = 0;

    ui->dragTarget      = NULL;
    ui->editTarget      = NULL;
    ui->editBuf[0]      = '\0';
    ui->editLen         = 0;
    ui->editRepeatTimer = 0.0f;

    ui->showTrails          = renderer ? (bool)renderer->showTrails : true;
    ui->showStarfield       = renderer ? (bool)renderer->showStarfield : true;
    ui->showGrid            = renderer ? (bool)renderer->showOrbitPlaneGrid : false;
    ui->showReferenceOrbits = renderer ? (bool)renderer->showReferenceOrbits : true;
    ui->showMeteors         = renderer ? (bool)renderer->showMeteors : true;
    ui->showGalaxy          = renderer ? (bool)renderer->showGalaxy : true;
    ui->collisionsEnabled   = true;
    ui->followTarget        = SIM_EARTH;

    load_ui_fonts(ui);
    apply_gui_theme();

    ui_sync(ui, sim, cam, renderer);
}

void ui_unload(UiState *ui)
{
    if (!ui) return;
    if (ui->fontLoaded & 2)
    {
        if (ui->uiFontBold.texture.id != ui->uiFont.texture.id)
            UnloadFont(ui->uiFontBold);
    }
    if (ui->fontLoaded & 1) UnloadFont(ui->uiFont);
    ui->fontLoaded = 0;
}

void ui_reset(UiState *ui)
{
    if (!ui) return;

    ui->scroll         = (Vector2){ 0.0f, 0.0f };
    ui->mouseCaptured  = 0;
    ui->dragTarget     = NULL;
    ui->editTarget     = NULL; /* discards an uncommitted edit */
    ui->editBuf[0]     = '\0';
    ui->editLen        = 0;
    ui->editRepeatTimer = 0.0f;
}

void ui_sync(UiState *ui, const Simulation *sim, const SimCamera *cam,
            const Renderer *renderer)
{
    if (!ui || !sim) return;

    const SimConfig *c = &sim->config;

    ui->starMassExp  = (float)log10(c->starMass  > 0.0 ? c->starMass  : 1e-12);
    ui->earthMassExp = (float)log10(c->earthMass > 0.0 ? c->earthMass : 1e-12);
    ui->moonMassExp  = (float)log10(c->moonMass  > 0.0 ? c->moonMass  : 1e-12);

    ui->starGravity  = (float)c->starGravity;
    ui->earthGravity = (float)c->earthGravity;
    ui->moonGravity  = (float)c->moonGravity;

    ui->starTrail  = (float)c->starTrailLength;
    ui->earthTrail = (float)c->earthTrailLength;
    ui->moonTrail  = (float)c->moonTrailLength;

    ui->speed = (float)c->stepsPerFrame;

    ui->pendingEarthOrbit  = (float)c->earthOrbitRadius;
    ui->pendingMoonOrbit   = (float)c->moonOrbitRadius;
    ui->pendingTranslation = (float)vec3_length(c->translation);

    ui->collisionsEnabled = (c->collisionMode != COLLISION_MODE_NONE);

    if (cam)
    {
        ui->cameraMode  = (cam->mode == CAMERA_MODE_FOLLOW) ? 1 : 0;
        ui->followTarget = cam->followTarget >= SIM_STAR && cam->followTarget <= SIM_MOON
                            ? cam->followTarget : SIM_EARTH;
        ui->zoom = cam->distance;
    }

    if (renderer)
    {
        ui->showTrails          = (bool)renderer->showTrails;
        ui->showStarfield       = (bool)renderer->showStarfield;
        ui->showGrid            = (bool)renderer->showOrbitPlaneGrid;
        ui->showReferenceOrbits = (bool)renderer->showReferenceOrbits;
        ui->showMeteors         = (bool)renderer->showMeteors;
        ui->showGalaxy          = (bool)renderer->showGalaxy;
    }
}

int ui_mouse_over(const UiState *ui)
{
    if (!ui) return 0;
    return CheckCollisionPointRec(GetMousePosition(), ui->panelBounds) ? 1 : 0;
}

int ui_wants_mouse(const UiState *ui)
{
    if (!ui) return 0;
    return ui->mouseOverPanel || ui->mouseCaptured;
}

int ui_is_editing(const UiState *ui)
{
    return (ui && ui->editTarget != NULL) ? 1 : 0;
}

void ui_begin_frame(UiState *ui)
{
    if (!ui) return;

    float w = ui->panelWidth;
    float x = (float)GetScreenWidth() - w;
    ui->panelBounds = (Rectangle){ x, 0.0f, w, (float)GetScreenHeight() };
    ui->mouseOverPanel = ui_mouse_over(ui);

    /* Latch on press-over-panel, release on button-up: a drag that starts on
       a slider keeps the camera locked out for its whole duration even after
       the mouse crosses back over the 3D view (item 2). */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui->mouseOverPanel)
        ui->mouseCaptured = 1;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        ui->mouseCaptured = 0;
        ui->dragTarget    = NULL; /* a drag can never outlive the button */
    }
}

/* -------------------------------------------------------------- controls */

static int point_in_panel(const UiState *ui, Rectangle r)
{
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, ui->clipRect) && CheckCollisionPointRec(m, r);
}

static double clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
 * Hand-drawn slider (see file header for why): dark track, cyan active
 * section, a knob that brightens on hover and tightens when pressed. The
 * drag is owned by the address of the value, so moving the pointer off the
 * track - or off the panel entirely - keeps the same slider under control
 * until the button is released, and no second slider can steal it.
 */
static void theme_slider(UiState *ui, Rectangle r, float *value,
                         float min, float max, Color accent)
{
    if (max <= min) return;

    Rectangle hit = { r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f };
    int hover     = point_in_panel(ui, hit);
    int dragging  = (ui->dragTarget == (const void *)value);

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui->dragTarget == NULL)
    {
        ui->dragTarget = (const void *)value;
        dragging = 1;
    }

    if (dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        float t = (GetMousePosition().x - r.x) / r.width;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        *value = min + t * (max - min);
    }

    /* A value typed into the numeric field is clamped there, but clamp here
       too so the knob can never be drawn off the end of its track. */
    if (*value < min) *value = min;
    if (*value > max) *value = max;

    float t = (*value - min) / (max - min);
    float cy = r.y + r.height * 0.5f;

    Rectangle track = { r.x, cy - 3.0f, r.width, 6.0f };
    Rectangle fill  = { r.x, cy - 3.0f, r.width * t, 6.0f };

    DrawRectangleRounded(track, 1.0f, 6, THEME_SURFACE);
    if (fill.width > 1.0f)
        DrawRectangleRounded(fill, 1.0f, 6, dragging ? accent : Fade(accent, 0.85f));

    float kx = r.x + r.width * t;
    float kr = dragging ? 7.5f : (hover ? 8.0f : 7.0f);

    if (hover || dragging) DrawCircleV((Vector2){ kx, cy }, kr + 4.0f, Fade(accent, 0.18f));
    DrawCircleV((Vector2){ kx, cy }, kr, dragging ? accent : THEME_SURFACE_HI);
    DrawCircleV((Vector2){ kx, cy }, kr - 2.5f, dragging ? THEME_BG : accent);
}

/*
 * A readout that is also an input (items 5 and 6).
 *
 * Returns 1 exactly on the frame the user commits a value, with *out set to
 * the parsed number already clamped into [dmin, dmax] - so a typed -5 on a
 * 0.1..10 control arrives as 0.1 and a typed 999 arrives as 10.0, and an
 * out-of-range number never reaches the simulation. Enter commits, Escape
 * restores the previous value, and a click anywhere else abandons the edit.
 */
static int numeric_field(UiState *ui, Rectangle r, const void *id, double value,
                         double dmin, double dmax, const char *display,
                         double *out)
{
    const int editing = (ui->editTarget == id);
    const int hover   = point_in_panel(ui, r);
    int committed = 0;

    if (!editing)
    {
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            /* Seed the buffer from the raw number, not from the formatted
               display string, so units and rounding never round-trip. */
            snprintf(ui->editBuf, UI_EDIT_BUF, "%s", fmt_number(value));
            ui->editLen         = (int)strlen(ui->editBuf);
            ui->editTarget      = id;
            ui->editRepeatTimer = 0.0f;
        }
        else
        {
            DrawRectangleRounded(r, 0.30f, 6,
                                 hover ? THEME_SURFACE : Fade(THEME_SURFACE, 0.55f));
            if (hover)
                DrawRectangleLinesEx(r, 1.0f, Fade(THEME_ACCENT, 0.45f));
            text_right(ui->uiFont, display, r.x + r.width - 8.0f,
                       r.y + (r.height - FS_VALUE) * 0.5f - 1.0f,
                       FS_VALUE, SP_TEXT, THEME_TEXT);
            return 0;
        }
    }

    /* --- editing ---------------------------------------------------- */

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hover)
    {
        ui->editTarget = NULL; /* click away: abandon, keep the old value */
        return 0;
    }

    int ch;
    while ((ch = GetCharPressed()) != 0)
    {
        int accepted = (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
                       ch == '+' || ch == 'e' || ch == 'E';
        if (accepted && ui->editLen < UI_EDIT_BUF - 1)
        {
            ui->editBuf[ui->editLen++] = (char)ch;
            ui->editBuf[ui->editLen]   = '\0';
        }
    }

    /* Backspace with our own auto-repeat: IsKeyPressedRepeat only exists in
       newer raylib, and the project supports 4.5 upwards. */
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        if (ui->editLen > 0) ui->editBuf[--ui->editLen] = '\0';
        ui->editRepeatTimer = 0.45f;
    }
    else if (IsKeyDown(KEY_BACKSPACE))
    {
        ui->editRepeatTimer -= GetFrameTime();
        if (ui->editRepeatTimer <= 0.0f)
        {
            if (ui->editLen > 0) ui->editBuf[--ui->editLen] = '\0';
            ui->editRepeatTimer = 0.04f;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        ui->editTarget = NULL;
    }
    else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        char *end = NULL;
        double parsed = strtod(ui->editBuf, &end);
        if (end != ui->editBuf && isfinite(parsed))
        {
            *out = clampd(parsed, dmin, dmax);
            committed = 1;
        }
        ui->editTarget = NULL;
    }

    DrawRectangleRounded(r, 0.30f, 6, THEME_BG);
    DrawRectangleLinesEx(r, 1.0f, THEME_ACCENT);

    const char *shown = ui->editBuf;
    float tw = text_width(ui->uiFont, shown, FS_VALUE, SP_TEXT);
    float tx = r.x + r.width - 8.0f - tw;
    if (tx < r.x + 6.0f) tx = r.x + 6.0f;
    float ty = r.y + (r.height - FS_VALUE) * 0.5f - 1.0f;

    text_at(ui->uiFont, shown, tx, ty, FS_VALUE, SP_TEXT, THEME_TEXT);

    if (fmodf((float)GetTime(), 1.0f) < 0.55f)
        DrawRectangle((int)(tx + tw + 1.0f), (int)ty, 1, (int)FS_VALUE, THEME_ACCENT);

    return committed;
}

/* -------------------------------------------------------------- sections */

static void section(const UiState *ui, float x, float w, const char *title)
{
    cursor_y += SECTION_TOP;
    text_at(ui->uiFontBold, title, x, cursor_y, FS_HEADING, SP_HEADING, THEME_TEXT);
    cursor_y += HEADING_H + 6.0f;
    DrawLine((int)x, (int)cursor_y, (int)(x + w), (int)cursor_y, THEME_DIVIDER);
    cursor_y += SECTION_BOTTOM;
}

/* Small muted paragraph. Lines are passed pre-split so the caller controls
   the wrap and nothing collides with the control below it. */
static void note(const UiState *ui, float x, const char **lines, int count)
{
    for (int i = 0; i < count; ++i)
    {
        text_at(ui->uiFont, lines[i], x, cursor_y, FS_NOTE, SP_TEXT, THEME_TEXT_MUTED);
        cursor_y += FS_NOTE + 4.0f;
    }
    cursor_y += 4.0f;
}

/*
 * One complete control: label on the left, editable value on the right,
 * slider underneath, then a gap. Every numeric control in the panel is built
 * from this, which is what keeps the column alignment and vertical rhythm
 * identical everywhere.
 *
 * logScale: the slider carries log10(value) because masses span decades; the
 * label, the readout and the typed input all work in real units, and the
 * conversion happens only here.
 */
typedef struct
{
    const char *label;
    float      *value;     /* what the slider drives */
    float       min, max;  /* in slider units        */
    int         logScale;
    const char *display;   /* formatted readout, in display units */
    Color       accent;
} Control;

static void control(UiState *ui, float x, float w, Control c)
{
    float labelY = cursor_y;

    Rectangle field = { x + w - FIELD_W, labelY - 2.0f, FIELD_W, FIELD_H };

    text_at(ui->uiFont, c.label, x, labelY + 1.0f, FS_LABEL, SP_TEXT, THEME_TEXT_DIM);

    double shown = c.logScale ? pow(10.0, (double)*c.value) : (double)*c.value;
    double dmin  = c.logScale ? pow(10.0, (double)c.min) : (double)c.min;
    double dmax  = c.logScale ? pow(10.0, (double)c.max) : (double)c.max;

    double typed = 0.0;
    if (numeric_field(ui, field, (const void *)c.value, shown, dmin, dmax,
                      c.display, &typed))
    {
        /* Already clamped to [dmin, dmax] by numeric_field, so the slider
           and the value can never disagree after a commit. */
        *c.value = c.logScale ? (float)log10(typed > 0.0 ? typed : dmin)
                              : (float)typed;
        if (*c.value < c.min) *c.value = c.min;
        if (*c.value > c.max) *c.value = c.max;
    }

    cursor_y += LABEL_H + LABEL_GAP;

    Rectangle sliderRec = { x, cursor_y, w, SLIDER_H };
    theme_slider(ui, sliderRec, c.value, c.min, c.max, c.accent);

    cursor_y += SLIDER_H + ROW_GAP;
}

/* ---------------------------------------------------------------- layout */

static float layout_panel(UiState *ui, Simulation *sim, SimCamera *cam,
                          Renderer *renderer, float originX, float originY,
                          float iw)
{
    cursor_y = originY;
    const float ix = originX;
    Rectangle r;

    /* ------------------------------------------------ simulation --------- */
    cursor_y += 4.0f;
    text_at(ui->uiFontBold, "SOLAR SYSTEM", ix, cursor_y, 16.0f, SP_HEADING, THEME_TEXT);
    cursor_y += 20.0f;
    text_at(ui->uiFont, "N-body gravitational sandbox", ix, cursor_y, FS_NOTE,
            SP_TEXT, THEME_TEXT_MUTED);
    cursor_y += 10.0f;

    section(ui, ix, iw, "SIMULATION");

    r = (Rectangle){ ix, cursor_y, iw * 0.48f, BUTTON_H };
    Rectangle r2 = { ix + iw * 0.52f, cursor_y, iw * 0.48f, BUTTON_H };
    if (GuiButton(r, sim->paused ? "#131#  Play" : "#132#  Pause"))
        simulation_toggle_pause(sim);
    if (GuiButton(r2, "#211#  Reset"))
    {
        /* FACTORY reset of the whole application - simulation, camera,
           renderer and UI - through the one centralised function. */
        application_factory_reset(sim, cam, renderer, ui);
    }
    cursor_y += BUTTON_H + ROW_GAP + 4.0f;

    control(ui, ix, iw, (Control){
        .label = "Physics speed", .value = &ui->speed,
        .min = 1.0f, .max = 64.0f, .logScale = 0,
        .display = TextFormat("%d steps", (int)ui->speed),
        .accent = THEME_ACCENT });
    simulation_set_speed(sim, (int)ui->speed); /* live: applied every frame */

    /* ------------------------------------------------ camera ------------- */
    section(ui, ix, iw, "CAMERA");

    int prevMode = ui->cameraMode;
    GuiToggleGroup((Rectangle){ ix, cursor_y, iw * 0.5f - 2.0f, 28.0f },
                   "FREE;FOLLOW", &ui->cameraMode);
    if (cam && ui->cameraMode != prevMode)
    {
        camera_set_mode(cam, ui->cameraMode ? CAMERA_MODE_FOLLOW
                                            : CAMERA_MODE_FREE, sim);
        if (ui->cameraMode) camera_set_follow_target(cam, ui->followTarget, sim);
    }
    cursor_y += 28.0f + 10.0f;

    if (ui->cameraMode == 1)
    {
        int prevTarget = ui->followTarget;
        /* Only Star/Earth/Moon - the "System" option is removed per item 8. */
        GuiToggleGroup((Rectangle){ ix, cursor_y, iw / 3.0f - 3.0f, 28.0f },
                       "STAR;EARTH;MOON", &ui->followTarget);
        if (cam && ui->followTarget != prevTarget)
            camera_set_follow_target(cam, ui->followTarget, sim);
        cursor_y += 28.0f + 10.0f;
    }

    cursor_y += 6.0f;

    if (cam)
    {
        control(ui, ix, iw, (Control){
            .label = "Zoom (distance)", .value = &ui->zoom,
            .min = cam->minDistance, .max = cam->maxDistance, .logScale = 0,
            .display = fmt_unit(ui->zoom, "AU"), .accent = THEME_ACCENT });

        /* Live in both directions: dragging the slider zooms the camera, and
           scrolling the wheel over the 3D view keeps the slider in step. */
        if (fabsf(ui->zoom - cam->distance) > 1e-5f)
            camera_set_distance(cam, ui->zoom);
        else
            ui->zoom = cam->distance;
    }

    /* ------------------------------------------------ masses ------------- */
    section(ui, ix, iw, "MASS");

    static const char *massNote[] = {
        "Star = 1.0 by definition. A small Earth-mass change",
        "barely moves its orbit; the Star's mass dominates.",
    };
    note(ui, ix, massNote, 2);

    struct { const char *name; float *exp; float lo; float hi; size_t index; }
    massRows[3] = {
        { "Star mass",  &ui->starMassExp,  -2.0f,  1.0f, SIM_STAR  },
        { "Earth mass", &ui->earthMassExp, -8.0f, -1.0f, SIM_EARTH },
        { "Moon mass",  &ui->moonMassExp, -10.0f, -2.0f, SIM_MOON  },
    };

    for (int i = 0; i < 3; ++i)
    {
        double value = pow(10.0, (double)*massRows[i].exp);
        control(ui, ix, iw, (Control){
            .label = massRows[i].name, .value = massRows[i].exp,
            .min = massRows[i].lo, .max = massRows[i].hi, .logScale = 1,
            .display = fmt_number(value), .accent = THEME_ACCENT });
        /* Live: applied every frame, not gated on a "changed" flag. */
        simulation_set_mass(sim, massRows[i].index,
                            pow(10.0, (double)*massRows[i].exp));
    }

    /* ------------------------------------------------ gravity ------------ */
    section(ui, ix, iw, "GRAVITY MULTIPLIER");

    static const char *gravNote[] = {
        "Experimental - real gravity depends on mass alone.",
        "Each pair's pull scales by g_i * g_j, so a high Star",
        "value can bind or destabilise Earth's orbit.",
    };
    note(ui, ix, gravNote, 3);

    struct { const char *name; float *g; size_t index; } gravRows[3] = {
        { "Star gravity",  &ui->starGravity,  SIM_STAR  },
        { "Earth gravity", &ui->earthGravity, SIM_EARTH },
        { "Moon gravity",  &ui->moonGravity,  SIM_MOON  },
    };

    for (int i = 0; i < 3; ++i)
    {
        /* Amber, not cyan: this section is not physical (item 1). */
        control(ui, ix, iw, (Control){
            .label = gravRows[i].name, .value = gravRows[i].g,
            .min = 0.0f, .max = 5.0f, .logScale = 0,
            .display = TextFormat("%.2fx", *gravRows[i].g),
            .accent = THEME_AMBER });
        simulation_set_gravity_multiplier(sim, gravRows[i].index,
                                          (double)*gravRows[i].g);
    }

    /* ------------------------------------------------ initial orbit ------ */
    section(ui, ix, iw, "INITIAL ORBIT");

    static const char *orbitNote[] = {
        "Staged values. Applying restarts the run from the new",
        "geometry and recomputes the circular orbital velocities.",
    };
    note(ui, ix, orbitNote, 2);

    control(ui, ix, iw, (Control){
        .label = "Earth-Star distance", .value = &ui->pendingEarthOrbit,
        .min = 0.05f, .max = 4.0f, .logScale = 0,
        .display = fmt_unit(ui->pendingEarthOrbit, "AU"), .accent = THEME_ACCENT });

    control(ui, ix, iw, (Control){
        .label = "Moon-Earth distance", .value = &ui->pendingMoonOrbit,
        .min = 0.0005f, .max = 0.006f, .logScale = 0,
        .display = fmt_unit(ui->pendingMoonOrbit, "AU"), .accent = THEME_ACCENT });

    const char *moonScaleNote = TextFormat("Real distance; drawn %.0fx larger for visibility.",
                                           (double)MOON_VISUAL_ORBIT_SCALE);
    note(ui, ix, &moonScaleNote, 1);

    control(ui, ix, iw, (Control){
        .label = "System translation", .value = &ui->pendingTranslation,
        .min = 0.0f, .max = 1.0f, .logScale = 0,
        .display = fmt_number(ui->pendingTranslation), .accent = THEME_ACCENT });

    int staged =
        fabs((double)ui->pendingEarthOrbit - sim->config.earthOrbitRadius) > 1e-6 ||
        fabs((double)ui->pendingMoonOrbit  - sim->config.moonOrbitRadius)  > 1e-9 ||
        fabs((double)ui->pendingTranslation -
             vec3_length(sim->config.translation)) > 1e-6;

    if (staged)
    {
        Rectangle badge = { ix, cursor_y - 6.0f, iw, 24.0f };
        DrawRectangleRounded(badge, 0.35f, 6, Fade(THEME_AMBER, 0.12f));
        text_at(ui->uiFont, "Changes are staged", ix + 10.0f, cursor_y,
                FS_NOTE, SP_TEXT, THEME_AMBER);
        cursor_y += 28.0f;
    }
    else
    {
        text_at(ui->uiFont, "No pending changes", ix + 10.0f, cursor_y,
                FS_NOTE, SP_TEXT, THEME_TEXT_MUTED);
        cursor_y += 28.0f;
    }

    /* Deliberately a separate button from Reset, and never called "Reset":
       this applies ONLY the three staged initial conditions and leaves every
       other live parameter exactly as the user has it. Disabled-looking but
       still laid out when nothing is staged, so the section doesn't jump. */
    GuiSetState(staged ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton((Rectangle){ ix, cursor_y, iw, BUTTON_H }, "APPLY CHANGES") &&
        staged)
    {
        /* Edit only the staged fields, then hand off to the same
           simulation_reset() every other initialisation uses - so the new
           orbital velocities come from the one authoritative path rather
           than a second copy of the orbit maths. Masses, gravity, speed,
           trail lengths, collision mode and integrator are untouched. */
        sim->config.earthOrbitRadius = ui->pendingEarthOrbit;
        sim->config.moonOrbitRadius  = ui->pendingMoonOrbit;
        sim->config.translation      = vec3_scale(TRANSLATION_DIR,
                                                  ui->pendingTranslation);
        simulation_reset(sim);

        /* The camera is deliberately NOT reset: the user's FREE/FOLLOW mode
           and target survive an Apply. */
        ui_sync(ui, sim, cam, renderer);

        /* ui_sync has just pulled the applied values back into the pending
           fields, so staged is now false and the badge above disappears on
           the next frame with nothing else to do. */
    }
    GuiSetState(STATE_NORMAL);
    cursor_y += BUTTON_H + 4.0f;

    /* ------------------------------------------------ trails ------------- */
    section(ui, ix, iw, "TRAILS");

    struct { const char *name; float *v; float lo; float hi; size_t index; }
    trailRows[3] = {
        { "Earth trail", &ui->earthTrail, 200.0f, 30000.0f, SIM_EARTH },
        { "Moon trail",  &ui->moonTrail,  100.0f, 12000.0f, SIM_MOON  },
        { "Star trail",  &ui->starTrail,   20.0f,  4000.0f, SIM_STAR  },
    };

    for (int i = 0; i < 3; ++i)
    {
        control(ui, ix, iw, (Control){
            .label = trailRows[i].name, .value = trailRows[i].v,
            .min = trailRows[i].lo, .max = trailRows[i].hi, .logScale = 0,
            .display = TextFormat("%d pts", (int)*trailRows[i].v),
            .accent = THEME_ACCENT });
        simulation_set_trail_length(sim, trailRows[i].index,
                                    (size_t)*trailRows[i].v);
    }

    if (renderer)
    {
        control(ui, ix, iw, (Control){
            .label = "Trail fade", .value = &renderer->trailFadeExponent,
            .min = 0.5f, .max = 6.0f, .logScale = 0,
            .display = TextFormat("%.2f", renderer->trailFadeExponent),
            .accent = THEME_ACCENT });

        /* ------------------------------------------------ visual size ---- */
        section(ui, ix, iw, "VISUAL SIZE");

        static const char *sizeNote[] = {
            "Render radii only - collision radii are unchanged",
            "and stay at their true physical values.",
        };
        note(ui, ix, sizeNote, 2);

        struct { const char *name; float *v; float lo; float hi; } sizeRows[3] = {
            { "Star radius",  &renderer->visuals[SIM_STAR].visualRadius,  0.02f, 1.2f },
            { "Earth radius", &renderer->visuals[SIM_EARTH].visualRadius, 0.01f, 0.5f },
            { "Moon radius",  &renderer->visuals[SIM_MOON].visualRadius,  0.005f, 0.25f },
        };

        for (int i = 0; i < 3; ++i)
            control(ui, ix, iw, (Control){
                .label = sizeRows[i].name, .value = sizeRows[i].v,
                .min = sizeRows[i].lo, .max = sizeRows[i].hi, .logScale = 0,
                .display = fmt_number(*sizeRows[i].v), .accent = THEME_ACCENT });

        control(ui, ix, iw, (Control){
            .label = "Earth spin rate", .value = &renderer->earthSpinDegPerSec,
            .min = 0.0f, .max = 180.0f, .logScale = 0,
            .display = TextFormat("%.0f deg/s", renderer->earthSpinDegPerSec),
            .accent = THEME_ACCENT });

        /* ------------------------------------------------ rendering ------ */
        section(ui, ix, iw, "RENDERING");

        const float colW = iw * 0.5f;
        struct { const char *label; bool *flag; } toggles[7] = {
            { "Trails",      &ui->showTrails },
            { "Star field",  &ui->showStarfield },
            { "Orbit refs",  &ui->showReferenceOrbits },
            { "Plane grid",  &ui->showGrid },
            { "Meteors",     &ui->showMeteors },
            { "Galaxy band", &ui->showGalaxy },
            { "Collisions",  &ui->collisionsEnabled },
        };

        for (int i = 0; i < 7; ++i)
        {
            float cx = ix + (i % 2) * colW;
            GuiCheckBox((Rectangle){ cx, cursor_y, 18.0f, 18.0f },
                        toggles[i].label, toggles[i].flag);
            if (i % 2 == 1 || i == 6) cursor_y += 26.0f;
        }

        renderer->showTrails          = ui->showTrails ? 1 : 0;
        renderer->showStarfield       = ui->showStarfield ? 1 : 0;
        renderer->showOrbitPlaneGrid  = ui->showGrid ? 1 : 0;
        renderer->showReferenceOrbits = ui->showReferenceOrbits ? 1 : 0;
        renderer->showMeteors         = ui->showMeteors ? 1 : 0;
        renderer->showGalaxy          = ui->showGalaxy ? 1 : 0;
    }

    sim->config.collisionMode = ui->collisionsEnabled ? COLLISION_MODE_MERGE
                                                      : COLLISION_MODE_NONE;

    cursor_y += 24.0f; /* bottom padding */
    return cursor_y;
}

void ui_draw(UiState *ui, Simulation *sim, SimCamera *cam, Renderer *renderer)
{
    if (!ui || !sim) return;

    ui_begin_frame(ui);

    const float w = ui->panelWidth;

    Rectangle panelRec   = ui->panelBounds;
    Rectangle contentRec = { 0.0f, 0.0f, w - 16.0f, ui->contentHeight };
    Rectangle view;

    /* GuiScrollPanel paints its own background (DEFAULT BACKGROUND_COLOR,
       set to the translucent panel colour) and then its scrollbar, so
       nothing may be painted over it afterwards - doing exactly that is what
       used to hide the scrollbar. */
    GuiScrollPanel(panelRec, NULL, contentRec, &ui->scroll, &view);

    ui->clipRect = view;

    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);

    float originX = panelRec.x + ui->scroll.x + PAD;
    float originY = panelRec.y + ui->scroll.y;
    float iw      = contentRec.width - 2.0f * PAD;

    float usedHeight = layout_panel(ui, sim, cam, renderer, originX, originY, iw);

    /* Track the real content height (minus the scroll offset already baked
       into originY) so the scrollbar thumb stays honest as sections are
       added or removed, instead of a hand-tuned constant going stale. */
    float measured = usedHeight - (panelRec.y + ui->scroll.y) + PAD;
    if (measured > ui->contentHeight - 40.0f || measured < ui->contentHeight - 400.0f)
        ui->contentHeight = measured + 40.0f;

    EndScissorMode();

    /* A one-pixel rule where the panel meets the 3D view. */
    DrawLine((int)panelRec.x, 0, (int)panelRec.x, GetScreenHeight(), THEME_BORDER_HI);

    ui->mouseOverPanel = ui_mouse_over(ui);
}

/* ------------------------------------------------------------------- HUD */

static void hud_row(const UiState *ui, float x, float w, float y,
                    const char *label, const char *value, Color valueColor)
{
    text_at(ui->uiFont, label, x, y, FS_HUD, SP_TEXT, THEME_TEXT_MUTED);
    text_right(ui->uiFontBold, value, x + w, y, FS_HUD, SP_TEXT, valueColor);
}

/*
 * The information overlay (item 8): a compact, aligned panel rather than a
 * stack of debug prints. Two columns - muted label, bright right-aligned
 * value - in the same fonts and palette as the control panel.
 */
void ui_draw_hud(const UiState *ui, const Simulation *sim, const SimCamera *cam)
{
    if (!ui || !sim || !cam) return;

    const float x = 18.0f, y = 16.0f;
    const float w = 250.0f;
    const float innerX = x + 14.0f, innerW = w - 28.0f;

    const Body *star  = simulation_body(sim, SIM_STAR);
    const Body *earth = simulation_body(sim, SIM_EARTH);
    const Body *moon  = simulation_body(sim, SIM_MOON);

    /* 47 px of title block, one 19 px line per row, two 7 px group gaps and
       a bottom margin - computed rather than guessed so the box always ends
       below the last row it draws. */
    int rows = 9;
    if (sim->lastCollision.occurred) rows += 1;
    float h = 47.0f + rows * 19.0f + 24.0f;

    Rectangle box = { x, y, w, h };
    DrawRectangleRounded(box, 0.06f, 8, THEME_OVERLAY);
    DrawRectangleLinesEx(box, 1.0f, THEME_BORDER_HI);

    float cy = y + 15.0f;
    text_at(ui->uiFontBold, "SOLAR SYSTEM SIMULATION", innerX, cy,
            FS_HUD_TITLE, SP_HEADING, THEME_ACCENT);
    cy += 20.0f;
    DrawLine((int)innerX, (int)cy, (int)(innerX + innerW), (int)cy, THEME_BORDER_HI);
    cy += 12.0f;

    hud_row(ui, innerX, innerW, cy, "FPS", TextFormat("%d", GetFPS()), THEME_TEXT);
    cy += 19.0f;
    hud_row(ui, innerX, innerW, cy, "STATUS",
            sim->paused ? "PAUSED" : "RUNNING",
            sim->paused ? THEME_AMBER : THEME_TEXT);
    cy += 19.0f;
    hud_row(ui, innerX, innerW, cy, "TIME", TextFormat("%.2f", sim->time), THEME_TEXT);
    cy += 19.0f;
    hud_row(ui, innerX, innerW, cy, "SPEED",
            TextFormat("%dx", sim->config.stepsPerFrame), THEME_TEXT);
    cy += 19.0f;
    hud_row(ui, innerX, innerW, cy, "ENERGY DRIFT",
            fmt_number(simulation_energy_drift(sim)), THEME_TEXT_DIM);
    cy += 26.0f;

    if (star && earth && star->active && earth->active)
        hud_row(ui, innerX, innerW, cy, "EARTH-STAR",
                fmt_unit(vec3_distance(earth->position, star->position), "AU"),
                THEME_TEXT);
    cy += 19.0f;
    if (earth && moon && earth->active && moon->active)
        hud_row(ui, innerX, innerW, cy, "MOON-EARTH",
                fmt_unit(vec3_distance(moon->position, earth->position), "AU"),
                THEME_TEXT);
    cy += 26.0f;

    hud_row(ui, innerX, innerW, cy, "CAMERA",
            cam->mode == CAMERA_MODE_FOLLOW ? "FOLLOW" : "FREE", THEME_TEXT);
    cy += 19.0f;
    hud_row(ui, innerX, innerW, cy, "ZOOM",
            TextFormat("%.2f", cam->distance), THEME_TEXT);
    cy += 19.0f;

    if (sim->lastCollision.occurred)
        hud_row(ui, innerX, innerW, cy, "MERGED",
                TextFormat("%s + %s", sim->lastCollision.survivorName,
                           sim->lastCollision.absorbedName), THEME_DANGER);

    /* Shortcut strip, same typeface, bottom-left and clear of the panel. */
    const char *keys = "WASD/QE move    drag rotate    wheel zoom    "
                       "SPACE pause    R reset    F follow";
    text_at(ui->uiFont, keys, 18.0f, (float)GetScreenHeight() - 28.0f,
            FS_NOTE, SP_TEXT, THEME_TEXT_MUTED);
}
