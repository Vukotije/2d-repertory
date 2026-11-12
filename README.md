# 2D Repertory: Cinema Seat Manager

A 2D cinema hall simulation written in C++ with OpenGL 3.3 (core profile), GLFW and GLEW. Built as the 2D project for the Computer Graphics course. The original assignment, in Serbian, is in [`Docs/Specifikacija.md`](Docs/Specifikacija.md) (general requirements) and [`Docs/Tema.md`](Docs/Tema.md) (the cinema theme).

## Overview

The program shows a top-down view of a cinema hall with 60 seats (6 rows × 10). The entrance is in the top left and the screen is along the top edge. The app repeats a four-phase cycle:

| Phase | Behaviour |
|---|---|
| **Reservation** | A dark overlay at 50% opacity covers the hall. Seats can be reserved, unreserved or bought. |
| **Entering** | The overlay is removed and the door opens. A random number of people, up to the number of taken seats, walk in. Each one goes down the aisle to their row, then along the row to their seat. |
| **Movie** | The door closes. For 20 seconds the screen switches to a new random colour every 12 frames. |
| **Leaving** | The screen turns white and people leave the way they came in. When the hall is empty, the program goes back to Reservation. |

Seat states are shown by colour: **blue** for free, **yellow** for reserved and **red** for bought.

## Controls

| Input | Action |
|---|---|
| Left click on a seat | Toggle between free and reserved |
| `1`–`9` | Buy N adjacent free seats, searching from the rightmost seat of the last row |
| `Enter` | Start the projection |
| `Escape` | Quit (works in any phase) |

Seats can only be changed during the Reservation phase.

## Requirements covered

- Programmable pipeline (OpenGL 3.3 core) with custom vertex and fragment shaders
- Uniforms for position, size, colour and texture tiling
- Alpha blending for the overlay, the name tag and the screen's light on the hall
- Textures, including a tiled carpet and a tinted seat sprite
- Mouse and keyboard input
- Custom cursor shaped like a film camera
- Full-screen window with a 75 FPS frame limiter
- Semi-transparent name tag in the bottom right corner

## Building

### Windows (Visual Studio 2022)

1. Open `2d-repertory.sln`.
2. Right-click the solution and choose **Restore NuGet Packages**. This installs GLFW and GLEW.
3. Build and run the **x64** configuration.

### macOS / Linux (CMake)

```sh
brew install cmake glfw glew        # or your distribution's package manager
cmake -B build
cmake --build build --target run
```

The `run` target starts the program from the repository root. This is required because shaders and textures are loaded from relative paths.

## Project structure

```
Source/
  Main.cpp       application state, input handling, update logic, rendering, main loop
  Util.cpp       shader compilation, texture and cursor loading (from the course skeleton)
Header/
  Util.h
  stb_image.h    image loading (third party)
Shaders/
  basic.vert     scales and positions a unit quad through uniforms
  basic.frag     draws a flat colour or a texture tinted by that colour
Resources/       pixel-art textures and the cursor image
Docs/            assignment text
```

## Implementation notes

- **One quad, one shader.** The whole scene is drawn with a single unit-quad VAO and a single shader program. `drawQuad()` sets the position, size, colour and optional texture through uniforms, then issues one draw call.
- **Virtual coordinates.** All layout uses a fixed 1600 × 900 space with the origin in the top left corner. It is converted to normalised device coordinates when drawing, and mouse input is mapped back into the same space. Because of this, the layout does not depend on the monitor's resolution.
- **Frame-independent movement.** People move at a constant 300 virtual px/s, scaled by the time since the last frame.
- **Frame limiter.** VSync is disabled and each frame busy-waits until 1/75 s has passed. This keeps the frame rate at 75 FPS whatever the monitor's refresh rate.
