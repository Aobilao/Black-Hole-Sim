# src/

C++ source files for the black hole renderer.

## `main.cpp`

Entry point and render loop.

1. **Windowing & GL context** – creates a GLFW window (1280×720, OpenGL 3.3 core) and loads GL function pointers via GLAD.
2. **Asset loading** – loads `starfield.jpg` (equirectangular skybox) and `disk.png` (accretion-disk noise texture) into OpenGL textures with sRGB internal formats. Single-channel textures (the disk) fall back to `GL_R8`.
3. **Full-screen quad** – builds a two-triangle VAO covering clip space; all rendering is fragment-shader-only against this quad.
4. **Shader compilation** – loads four shader programs: `blackhole` (ray marcher), `bright` (bloom threshold), `blur` (Gaussian), `post` (tonemapping + composite).
5. **Framebuffer management** – allocates one full-resolution `RGBA16F` FBO for the HDR scene and two half-resolution `RGBA16F` FBOs for ping-pong bloom blurring. All FBOs are resized on window resize.
6. **Orbital camera** – the camera orbits the origin. Left-click drag adjusts azimuth/elevation; scroll wheel adjusts distance (clamped 5–35 Schwarzschild radii). Camera position is derived from spherical coordinates each frame.
7. **Render loop** (per frame):
   - Render the black hole scene into the HDR FBO (`blackhole` shader).
   - Extract bright pixels into the first bloom FBO (`bright` shader).
   - Run 10 passes of ping-pong Gaussian blur across the two bloom FBOs (`blur` shader).
   - Composite the HDR scene + bloom to the default framebuffer with tonemapping (`post` shader).
8. **Input** – `R` hot-reloads all shaders from disk, `D` toggles the accretion disk, `K` switches between the Schwarzschild and Kerr metrics, `[` / `]` decrease / increase the Kerr spin `a` (passed to the shader as `uMetric` / `uSpin`), `Esc` quits.

## `Camera.h` / `Camera.cpp`

A minimal look-at camera stored as position + yaw/pitch + vertical FOV.

- `updateBasis()` recomputes the orthonormal basis (`forward`, `right`, `up`) from yaw and pitch using standard spherical-to-Cartesian conversion. World-up is always +Y.
- Pitch is clamped to ±89° to avoid gimbal degeneracy.
- The camera doesn't build a view matrix – it passes its basis vectors directly to the shader as uniforms (`uCamPos`, `uCamForward`, `uCamRight`, `uCamUp`).

## `Shader.h` / `Shader.cpp`

OpenGL shader program wrapper with hot-reload support.

- **Construction** – takes vertex and fragment shader file paths, immediately calls `reload()`.
- **`reload()`** – reads both source files from disk, compiles each stage, links the program, replaces the old program handle, and clears the uniform location cache. Returns `false` on any failure without touching the existing program.
- **Uniform setters** (`setInt`, `setFloat`, `setVec2`, `setVec3`) – look up uniform locations via a `std::unordered_map` cache to avoid repeated `glGetUniformLocation` calls.
- Move-only (copy deleted) to prevent accidental GL resource duplication.

## `Framebuffer.h` / `Framebuffer.cpp`

Thin FBO + texture wrapper for off-screen HDR rendering.

- `create(w, h, internalFormat)` – allocates both the FBO and a single `GL_TEXTURE_2D` colour attachment with the given internal format (always `GL_RGBA16F` in this project). No depth/stencil attachment since rendering is a full-screen quad.
- `resize(w, h)` – reallocates the texture storage without recreating the FBO, and re-attaches it.
- `bind()` – binds the FBO and sets `glViewport` to the stored dimensions.
- RAII cleanup in the destructor. Copy deleted.

## `stb_image_impl.cpp`

Single-translation-unit include of stb_image (`#define STB_IMAGE_IMPLEMENTATION`). The header itself lives in `external/stb/`.
