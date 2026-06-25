# Black Hole Simulation

Real-time black hole visualisation using GPU ray marching. Light rays are traced along geodesics of the Schwarzschild and Kerr metric via a 4th-order Runge–Kutta integrator, producing gravitational lensing of a background starfield and an orbiting accretion disk with relativistic Doppler shifting and blackbody colouring.

## Features

- **Geodesic ray tracing** – per-pixel null-geodesic integration in the fragment shader
- **Schwarzschild & Kerr** – switch at runtime between a static and a rotating (spinning) black hole, with frame dragging
- **Accretion disk** – Keplerian velocity, gravitational redshift, relativistic Doppler & beaming, blackbody colour
- **Bloom post-processing** – brightness extraction + two-pass Gaussian blur + ACES tonemapping
- **Interactive camera** – orbit, elevation & zoom with mouse/scroll
- **Hot-reload shaders** at runtime

## Requirements

- C++17 compiler (MSVC / GCC / Clang)
- CMake ≥ 3.16
- OpenGL 3.3+ capable GPU
- Python 3 + NumPy + Pillow *(only to regenerate `assets/disk.png`)*

GLFW 3.4 and GLM 1.0.1 are fetched automatically by CMake. GLAD and stb_image are vendored in `external/`.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the executable from the repo root so shader/asset paths resolve correctly:

```bash
./build/Release/blackhole      # Windows: .\build\Release\blackhole.exe
```

## Regenerate Disk Texture

```bash
python -m venv .venv
source .venv/bin/activate     # Windows: .venv\Scripts\activate
pip install numpy pillow
python DiskGenerator.py
```

This overwrites `assets/disk.png` with a fresh procedural noise texture used for the accretion disk.

## Controls

| Input | Action |
|---|---|
| Left-click drag | Orbit camera |
| Scroll wheel | Zoom in/out |
| `D` | Toggle accretion disk |
| `K` | Toggle metric (Schwarzschild ↔ Kerr) |
| `[` / `]` | Decrease / increase Kerr spin `a` (held) |
| `G` | Cycle render scale (100% → 75% → 50% → 40%) for performance |
| `R` | Hot-reload shaders |
| `Esc` | Quit |

## Project Structure

```
├── src/
│   ├── main.cpp          # Window, render loop, bloom pipeline
│   ├── Camera.h/cpp      # Orbital camera
│   ├── Shader.h/cpp      # Shader loading & hot-reload
│   └── Framebuffer.h/cpp # FBO wrapper for HDR / bloom
├── shaders/
│   ├── blackhole.frag    # Geodesic ray marcher + disk shading
│   ├── bright.frag       # Brightness threshold extraction
│   ├── blur.frag         # Separable Gaussian blur
│   ├── post.frag         # Bloom composite + ACES tonemap
│   └── fullscreen.vert   # Full-screen quad vertex shader
├── assets/
│   ├── starfield.jpg     # Equirectangular sky background
│   └── disk.png          # Procedural accretion disk texture
├── DiskGenerator.py      # Script to regenerate disk.png
└── CMakeLists.txt
```
