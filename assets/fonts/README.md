# UI fonts

The application ships the two faces it uses and loads them once at startup
(`src/ui.c: load_ui_fonts`). Every piece of text the program draws — section
headings, control labels, numeric readouts, buttons, the explanatory copy and
the FPS/status overlay — is rendered with them, so there is no mix of a modern
typeface and raygui's bitmap font anywhere.

| File | Used for |
| --- | --- |
| `Inter-Regular.ttf` | labels, values, notes, overlay rows |
| `Inter-SemiBold.ttf` | section headings, overlay title, overlay values |

Inter is by Rasmus Andersson, licensed under the SIL Open Font License 1.1 —
see `Inter-LICENSE.txt`. It is freely redistributable, so no download step is
needed to build or run the project.

The atlas is baked once at 48 px with bilinear filtering and drawn at the
12–16 px sizes the panel uses; nothing is loaded per frame.

## Search order

`load_ui_fonts()` tries, in order:

1. `assets/fonts/Inter-Regular.ttf` / `Inter-SemiBold.ttf` (the shipped pair),
   also probed as `../assets/...` and `../../assets/...` so the binary finds
   them when run from a build directory. CMake additionally copies the whole
   `assets/` tree next to the executable after each build.
2. `assets/fonts/ui.ttf` / `ui-bold.ttf` — drop any redistributable
   sans-serif here under those names to override the shipped font.
3. Common system faces (DejaVu Sans / Liberation Sans on Linux, Segoe UI /
   Arial on Windows, Helvetica on macOS).
4. raylib's built-in bitmap font, as a last resort, so a missing file can
   never stop the program from running.
