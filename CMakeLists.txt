cmake_minimum_required(VERSION 3.16)
project(SolarSystemSimulation C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SSS_BUILD_TESTS "Build the headless physics tests" ON)
option(SSS_BUILD_APP   "Build the raylib application"     ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# solar_physics: the engine. No raylib, no raygui, no rendering of any kind.
# It is a separate target precisely so that this separation cannot rot: if
# anything here started calling raylib, this target would stop linking.
# ---------------------------------------------------------------------------
add_library(solar_physics STATIC
    src/vector3d.c
    src/trail.c
    src/body.c
    src/physics.c
    src/integrator.c
    src/collision.c
    src/simulation.c
)
target_include_directories(solar_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_library(M_LIB m)
if(M_LIB)
    target_link_libraries(solar_physics PUBLIC ${M_LIB})
endif()

# ---------------------------------------------------------------------------
# Dependencies for the application only.
# ---------------------------------------------------------------------------
if(SSS_BUILD_APP)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    # raygui is populated without being configured (its own CMakeLists builds
    # examples we don't want), which CMake 3.30+ deprecates and CMake 4
    # refuses under the new policy. Ask for the old behaviour explicitly so
    # the same file configures on both.
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    # --- raylib ------------------------------------------------------------
    find_package(raylib 4.5 QUIET)
    if(NOT raylib_FOUND)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        5.5
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    # --- raygui (single header; its own CMake builds examples we don't want) -
    FetchContent_Declare(raygui
        GIT_REPOSITORY https://github.com/raysan5/raygui.git
        GIT_TAG        4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(raygui)
    if(NOT raygui_POPULATED)
        FetchContent_Populate(raygui)
    endif()
    add_library(raygui INTERFACE)
    target_include_directories(raygui INTERFACE ${raygui_SOURCE_DIR}/src)

    # --- cglm --------------------------------------------------------------
    set(CGLM_STATIC ON  CACHE BOOL "" FORCE)
    set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
    set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(cglm
        GIT_REPOSITORY https://github.com/recp/cglm.git
        GIT_TAG        v0.9.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(cglm)

    # -----------------------------------------------------------------------
    # The application: rendering, camera, UI, entry point.
    # -----------------------------------------------------------------------
    add_executable(solar_system
        src/main.c
        src/camera_sim.c
        src/renderer.c
        src/ui.c
        src/app_reset.c
    )
    target_link_libraries(solar_system PRIVATE solar_physics raylib raygui cglm)

    # stb_image is optional and only needed once textures are added:
    #   target_include_directories(solar_system PRIVATE external/stb)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/stb/stb_image.h)
        target_include_directories(solar_system PRIVATE external/stb)
        target_compile_definitions(solar_system PRIVATE SSS_HAVE_STB_IMAGE)
    endif()

    if(APPLE)
        target_link_libraries(solar_system PRIVATE
            "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()
    if(WIN32)
        target_link_libraries(solar_system PRIVATE winmm)
    endif()

    # The UI loads its TTF faces from assets/fonts at runtime, relative to the
    # working directory. Copying the whole assets tree next to the executable
    # means the app finds them whether it is launched from the project root or
    # from the build directory (src/ui.c also probes ../assets as a fallback).
    add_custom_command(TARGET solar_system POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:solar_system>/assets
        COMMENT "Copying assets (fonts) next to the executable")
endif()

# ---------------------------------------------------------------------------
# Tests. These link solar_physics only, so `ctest` runs with no display,
# no GPU and no window - exactly the Phase 1 requirement.
# ---------------------------------------------------------------------------
if(SSS_BUILD_TESTS)
    enable_testing()
    foreach(test_name test_vectors test_gravity test_orbit test_collision test_reset)
        add_executable(${test_name} tests/${test_name}.c)
        target_link_libraries(${test_name} PRIVATE solar_physics)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
