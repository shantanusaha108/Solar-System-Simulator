# Solar System Simulation

A real-time **three-body gravitational simulation** written in **C**, rendered with **raylib**, controlled with **raygui**, and built using **CMake**.

The current version simulates a **Star, Earth, and Moon** using Newtonian gravity and **Velocity Verlet** integration. The orbital paths and trails come from the actual simulation rather than predefined animation curves.

The system also has a common translational velocity, so the Star itself moves while Earth orbits it. This produces the long, stretched-spring / helix-like path visible in the simulation.

---

## Simulation Preview



https://github.com/user-attachments/assets/431b2abd-a931-4a4d-8240-5dcfb153d512



---

# User Interface

## Simulation & Camera

Pause, reset, simulation speed, camera mode, follow target, and zoom.

![Simulation and camera controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/simControlAndCam%2C.png)

## Basic Statistics

The HUD shows simulation time, speed, energy drift, distances, and camera state.

![Basic statistics](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/basic_stats.png)

## Mass

Mass can be changed independently for the Star, Earth, and Moon.

![Mass controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/mass.png)

## Gravity Multiplier

An experimental control that changes how strongly each body pulls on the others.

![Gravity multiplier](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/gravity.png)

## Initial Orbit

Controls the starting Earth–Star distance, Moon–Earth distance, and system translation speed.

![Initial orbit controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/orbit.png)

## Trails

Controls the trail length of the Star, Earth, and Moon and the trail fading.

![Trail controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/trail.png)

## Visual Size

Changes the rendered size of the bodies without changing their physical collision radius.

![Visual size controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/visualSize.png)

## Rendering

Controls visual elements such as the star field, trails, grid, meteors, and other scene effects.

![Rendering controls](https://github.com/shantanusaha108/Solar-System-Simulator/blob/main/assets/media/ren.png)

---

# Project Files

| File | Purpose |
|---|---|
| `include/body.h` / `src/body.c` | Body data, initialization, state changes, and merged radius calculation. |
| `include/vector3d.h` / `src/vector3d.c` | `Vec3` mathematics and utility functions. |
| `include/trail.h` / `src/trail.c` | Ring-buffer storage for simulated positions. |
| `include/physics.h` / `src/physics.c` | Gravity, energy, momentum, mass, centre of mass, and orbital calculations. |
| `include/integrator.h` / `src/integrator.c` | Velocity Verlet and Symplectic Euler integration. |
| `include/collision.h` / `src/collision.c` | Collision detection and body merging. |
| `include/simulation.h` / `src/simulation.c` | Simulation setup, reset, timestep updates, parameters, trails, and collisions. |
| `include/camera_sim.h` / `src/camera_sim.c` | FREE / FOLLOW camera, movement, rotation, and zoom. |
| `include/renderer.h` / `src/renderer.c` | 3D rendering, bodies, trails, star field, and visual effects. |
| `include/ui.h` / `src/ui.c` | raygui control panel and UI state. |
| `src/main.c` | Window creation, main loop, input, simulation update, rendering, and shutdown. |
| `tests/test_vectors.c` | Vector mathematics tests. |
| `tests/test_gravity.c` | Gravity behaviour tests. |
| `tests/test_orbit.c` | Orbital and integration tests. |
| `tests/test_collision.c` | Collision and merging tests. |
| `tests/test_util.h` | Shared test macros. |
| `CMakeLists.txt` | Build configuration for the application and tests. |

---

# Units

The simulation uses normalized units with:

$$
G = 1
$$

| Quantity | Value |
|---|---:|
| Length | `1.0` = 1 AU-equivalent |
| Mass | `1.0` = default Star mass |
| Earth mass | `3.0 \times 10^{-6}` |
| Moon mass | `3.69 \times 10^{-8}` |
| Earth–Star distance | `1.0` |
| Moon–Earth distance | `0.00257` |
| Earth orbital period | `2\pi \approx 6.283` |
| Moon orbital period | `\approx 0.47` |
| Fixed timestep | `dt = 10^{-3}` |

---

# Physics

## Gravity

For two bodies, the gravitational force magnitude is:

$$
F = \frac{G m_1 m_2}{r^2}
$$

The corresponding acceleration of body 1 is:

$$
\vec{a}_1 =
\frac{G m_2}{r^3}
(\vec{r}_2-\vec{r}_1)
$$

The project also has an experimental gravity multiplier:

$$
\vec{a}_1 =
\frac{G\,g_2\,m_2}{r^3}
(\vec{r}_2-\vec{r}_1)
$$

where `g₂` is the gravity multiplier of the source body.

## Integration

The default integrator is **Velocity Verlet**:

1. Half-step velocity update
2. Full position update
3. Recalculate acceleration
4. Second half-step velocity update

The simulation uses a fixed `dt`; increasing the speed means more physics steps per frame.

## Collisions

The project contains collision-detection and merging code, but the **current collision behaviour is not working as intended**. In particular, an Earth–Star encounter does not currently produce the intended swallowing/destruction effect.

## Physical vs Visual Size

Physical radii are used by the physics system, while much larger visual radii are used for rendering so that the bodies remain visible.

---

# Camera

The camera supports:

- **FREE** mode
- **FOLLOW** mode
- Mouse rotation
- Mouse-wheel zoom
- `W/A/S/D` movement
- `Q/E` vertical movement

---

# Controls

| Input | Action |
|---|---|
| Mouse drag | Rotate camera |
| Mouse wheel | Zoom |
| `W` / `S` | Move forward / backward |
| `A` / `D` | Move left / right |
| `Q` / `E` | Move down / up |
| `Space` | Pause / resume |
| `R` | Reset |
| `F` | Toggle FREE / FOLLOW |

---

# Build

The project uses **CMake** and a C11-compatible compiler.

```bash
cmake -S . -B build
cmake --build build
```

On Linux:

```bash
./build/solar_system
```

CMake fetches the required application dependencies when needed:

- raylib `5.5`
- raygui `4.0`
- cglm `0.9.4`

---

# Tests

The physics tests can be built without the graphical application:

```bash
cmake -S . -B build-tests -DSSS_BUILD_APP=OFF
cmake --build build-tests
cd build-tests
ctest --output-on-failure
```

The tests run without a window or renderer and check the behaviour of the physics functions.

---

# Known Problems

| Problem | Description |
|---|---|
| **Moon camera** | Following Earth and following the Moon currently looks too similar. The camera behaviour needs to make Moon-follow mode clearly distinguishable. |
| **Collision** | Collision detection/merging does not currently produce the intended result. An Earth–Star collision should eventually result in a proper impact, swallowing, or destruction effect instead of Earth continuing in a very small orbit. |

---

# Future Goals

## 1. Add More Planets

Expand the current Star/Earth/Moon system to include the other planets of our Solar System and let all of them interact through the same gravitational simulation.

## 2. Dynamic Orbit Changes

Allow orbital parameters such as distance and velocity to be changed while the simulation is running instead of only through reset-based initial conditions.

## 3. Add the Asteroid Belt

Add a large population of asteroids between Mars and Jupiter with different orbital paths and gravitational interactions with the planets.

## 4. Improve Collisions

Make collisions behave more realistically, including proper swallowing, impact effects, or fragmentation where appropriate.
