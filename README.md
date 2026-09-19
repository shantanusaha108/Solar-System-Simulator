# Solar System Physics Simulation

A real-time three-body (Star / Earth / Moon) gravitational sandbox written in
**C**, rendered with **raylib**, driven by a **raygui** control panel, and built
with **CMake**.

The orbits are not animated. Every position on screen is the output of
Newtonian pair interactions integrated with Velocity Verlet, and every trail is
a recording of positions the integrator actually produced.

## The visual signature

The whole system carries a common translational velocity. Earth therefore
orbits the Star *while the Star moves*, so Earth's path through the inertial
frame is a helix — the long, thin, stretched-spring trail that is the point of
the project. The Star gets a short fading tick of a trail (it shows the system
is travelling), the Moon a shorter, tighter one coiled around Earth's path.

Nothing anywhere generates `sin(t)`/`cos(t)` for display. Remove the
translation (slider to 0, then Reset) and the spring collapses back into a
circle, which is the honest test that it was never faked.

## Build

```
cmake -S . -B build
cmake --build build
./build/solar_system
```

raylib 5.5, raygui 4.0 and cglm 0.9.4 are fetched automatically by CMake. An
already-installed raylib (>= 4.5) is used instead if CMake finds one. On Linux
you need the usual X11/GL development packages that raylib itself requires
(`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`
on Debian/Ubuntu).

### Tests

The physics engine builds and runs with no window, no GPU and no display:

```
cmake -S . -B build-tests -DSSS_BUILD_APP=OFF
cmake --build build-tests
cd build-tests && ctest --output-on-failure
```

Current results: all four suites pass, with a relative energy drift of about
`7e-14` over five Earth orbits (31,416 steps).

## Architecture

```
                        main.c
                          |
            +-------------+-------------+
            |                           |
      Simulation                     Renderer / UI
            |                           |
   +--------+--------+            +-----+-----+
   |        |        |            |           |
Physics Integrator Collision    Bodies      Trails
   |        |        |            |           |
   +--------+--------+            +--- raylib / raygui
            |
          Bodies / Trail / Vec3
```

`solar_physics` is a **separate CMake target that does not link raylib**. If a
gravitational calculation ever wandered into drawing code, or a draw call into
the engine, that target would stop building. The separation is enforced by the
build, not by convention.

| Module | Responsibility |
|---|---|
| `vector3d.c` | double-precision `Vec3`; no raylib, no cglm |
| `body.c` | body state, volume-conserving merged radius |
| `trail.c` | O(1) ring-buffer position history |
| `physics.c` | pairwise gravity, energy, momentum, centre of mass |
| `integrator.c` | Velocity Verlet (default) and symplectic Euler |
| `collision.c` | sphere detection, inelastic merge |
| `simulation.c` | config, initial conditions, fixed-timestep loop, reset |
| `camera_sim.c` | FREE / FOLLOW orbit camera (uses cglm for the vector math) |
| `renderer.c` | textured spheres, visual axial spin, trails, star field, meteors |
| `ui.c` | themed control panel + information overlay; the only translation unit with `RAYGUI_IMPLEMENTATION` |
| `main.c` | window, frame order, keyboard shortcuts |

## Units

Normalised, with `G = 1`:

| quantity | value |
|---|---|
| length | 1.0 = 1 AU-equivalent |
| mass | 1.0 = default Star mass |
| Earth mass | 3.0e-6 |
| Moon mass | 3.69e-8 |
| Earth–Star distance | 1.0 |
| Moon–Earth distance | 0.00257 |
| Earth orbital period | 2π ≈ 6.283 |
| Moon orbital period | ≈ 0.47 |
| fixed timestep `dt` | 1e-3 (≈ 6,280 steps per Earth orbit) |

Initial velocities come from `v = sqrt(G*M/r)` and are tangential, using the
two-body total mass, so changing a mass or an orbit radius changes the velocity
with it. The centre-of-mass drift the construction introduces is removed, and
*then* the deliberate common translation is added — which is why the translation
is a visible system motion rather than an accidental numerical artefact.

## Controls

| Input | Action |
|---|---|
| Mouse drag | Rotate camera |
| Mouse wheel | Zoom |
| `W` / `S` | Move camera forward / backward |
| `A` / `D` | Move camera left / right |
| `Q` / `E` | Move camera down / up |
| `Space` | Pause / resume |
| `R` | Reset |
| `F` | Toggle FREE / FOLLOW |

Panel controls: play/pause, reset, simulation speed, camera mode and follow
target, zoom, per-body mass, per-body gravity multiplier, per-body visual
radius, Earth's visual spin rate, Earth–Star and Moon–Earth initial distances,
system translation speed, three trail lengths, trail fade exponent, and view
toggles.

Every numeric readout is also an input: click the number beside a slider and
type a value. `Enter` commits it, `Escape` restores the previous one, and the
result is clamped to that slider's own minimum and maximum, so a typed `-5` on
a `0.1 … 10` control arrives as `0.1` and a typed `999` arrives as `10`. The
slider and the number are always the same value — there is only one of them.

### Physical vs visual

Three things on screen are deliberately not to scale, and none of them is
visible to the physics:

| | source of truth | drawn as |
|---|---|---|
| body size | `Body.radius` (collisions) | `BodyVisual.visualRadius` (renderer) |
| Moon's orbit | real 0.00257 AU | × `MOON_VISUAL_ORBIT_SCALE` at draw time |
| axial spin | not simulated at all | `Renderer.earthSpin` / `moonSpin`, degrees/second × frame time |

Earth and the Moon carry procedural textures on a real sphere mesh, so the
continents and craters rotate *with* the planet rather than sitting on top of
a static ball. Body names are not drawn in the 3D scene; `Body.name` remains
for collision reporting.

## Things worth knowing

**Mass and gravity are not secretly decoupled.** Changing a mass changes
gravitational influence, as Newton requires. The per-body *gravity multiplier*
is an extra sandbox knob that scales the pull a body exerts
(`a_i = Σ G·g_j·m_j/r³·(p_j − p_i)`). At 1.0 the model is exactly Newtonian;
any other value deliberately breaks the third law, and the panel labels it
experimental.

**Orbit-distance sliders are staged.** They edit initial conditions and take
effect on Reset, which recomputes the matching circular velocity. Applying them
live would teleport a moving body and inject a non-physical impulse.

**Speed is steps per frame, not a bigger `dt`.** The timestep never grows with
the frame rate or the speed setting, so 64× is still a stable integration.

**Escape is real.** Drop the Star mass and Earth's existing velocity becomes
too large for the weaker well; it goes eccentric and leaves. Nothing pulls it
back and nothing plays an "escaped" animation. `test_orbit` asserts that the
specific orbital energy actually turns positive.

**Visual size ≠ physical size.** Collisions use the physical radii
(Star 4.65e-3, Earth 4.26e-5, Moon 1.16e-5 AU); the spheres you see are
hundreds of times larger so they are visible at all. The two never touch each
other — moving a visual-radius slider cannot cause a collision.

**Softening** (`PHYS_SOFTENING = 1e-7`) exists only to keep a coincident pair
from producing `Inf`. It is orders of magnitude smaller than any physical
radius, so contact is always detected long before softening matters; it is not
covering for the collision code.

### Known limitations

- A fixed timestep can step *over* a contact if a body moves further than its
  own radius in one step. This shows up only in extreme configurations (a very
  tight, very fast orbit); reduce `dt` there. `test_collision` does exactly
  that.
- Render positions are `float`. The physics stays `double`, but after the
  system has travelled a few thousand AU the drawn trail will start to shimmer.
  The fix, if it ever matters, is to render relative to a moving origin.
- The Moon's orbit must stay inside Earth's Hill radius (≈ 0.01 AU at 1 AU), so
  the Moon-distance slider stops at 0.006. Beyond that the Star strips it away —
  correctly, but it stops being a Moon.

## Phase status

Phases 1–7 of the specification are implemented: physics, window, real
trajectories, system translation, camera, UI, collisions. Phase 8 (textures,
shaders, lighting) is deliberately left out — `stb_image` is wired into CMake
behind an existence check, so dropping `stb_image.h` into `external/stb/`
defines `SSS_HAVE_STB_IMAGE` and nothing else needs to change.
