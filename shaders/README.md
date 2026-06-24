# shaders/

GLSL 330 core shaders. Every fragment shader receives UV coordinates from the shared vertex shader via `vUV`.

## `fullscreen.vert`

Trivial vertex shader for the full-screen quad. Takes the 2D clip-space position (`aPos ∈ [−1, 1]`) and maps it to `vUV ∈ [0, 1]` for texture-coordinate use by the fragment shaders.

## `blackhole.frag`

The core physics shader – traces one light ray per pixel through Schwarzschild spacetime.

**Ray setup**
- Constructs a world-space ray direction from the camera basis and vertical FOV, corrected for aspect ratio.

**Geodesic integration**
- Uses the effective-potential formulation for null geodesics. The conserved quantity `L²` (squared angular momentum of the cross product `pos × vel`) is computed once at the start.
- `geodesicAccel()` returns the acceleration `−1.5 · L² / r⁵ · pos`, which is the Newtonian-like force term that encodes the Schwarzschild photon deflection.
- `rk4Step()` advances position and velocity by one 4th-order Runge–Kutta step. Step size `h` is adaptive: `0.05 · r`, clamped to `[0.02, 0.8]`.

**Termination**
- `r < 1.001` (event horizon) → returns accumulated colour (black if nothing was hit).
- `r > 40` (escape) → samples the skybox and returns.
- Max 300 steps.

**Accretion disk**
- Detected by a sign change in `pos.y` (ray crosses the equatorial plane y = 0) between consecutive integration steps. The crossing point is linearly interpolated.
- Only sampled if the crossing radius falls within `[DISK_INNER, DISK_OUTER]` Schwarzschild radii.
- **`diskDensity()`** – looks up the procedural noise texture using an azimuthal coordinate that rotates at the local Keplerian angular velocity (`ω = √(M/r³)`), producing a swirling pattern. Because `ω ∝ r^−3/2`, unbounded advection shears the texture into ever-finer spirals that eventually wind below pixel size and average to a featureless disk. To avoid this the advection runs on a looping global sawtooth (period `SWIRL_LOOP`) with two half-offset copies cross-faded: the instantaneous rate is still exactly `ω` (the motion is unchanged) but the winding is capped at `ω·SWIRL_LOOP` turns, so detail persists indefinitely while the loop reset stays hidden.
- **`diskEmission()`** – computes physically-motivated colour:
  - Gravitational redshift: `g_grav = √(1 − 1/r)`.
  - Relativistic Doppler: full special-relativistic formula using the local Keplerian orbital velocity, `β = √(M/(r − 2M))`, with the Lorentz factor and `1/(γ(1 − β·n̂))`.
  - The combined frequency shift `g = doppler · g_grav` is applied as `g⁴` brightness scaling (∝ ν⁴ for bolometric intensity).
  - Disk temperature follows a power-law `T(r) = T_inner · (r_inner/r)^0.75` (standard thin-disk profile). The observed temperature is Doppler-shifted: `T_obs = g · T(r)`.
  - **`blackbodyRGB()`** converts a temperature in Kelvin to an approximate sRGB colour using Tanner Helland's curve-fit of CIE data.
- Volumetric-style compositing: each crossing accumulates colour weighted by `α = density · DISK_DENSITY` and reduces transmittance by `(1 − α)`, so a ray passing through the disk twice (front and back) blends both contributions.

**Kerr mode (rotating black hole)**
- Selected by the `uMetric` uniform (`0` = Schwarzschild, `1` = Kerr); `uSpin` sets the spin parameter `a` (geometrized units, clamped to `|a| < M`). The spin axis is `+y`, so the equatorial plane stays at `y = 0` and the whole tracer collapses back to the Schwarzschild geometry as `a → 0`. The event horizon moves to `r₊ = M + √(M² − a²)` (which is `1` when `a = 0`, matching the Schwarzschild path).
- `traceKerr()` integrates the **exact** null geodesics in **Kerr–Schild Cartesian** coordinates. Unlike Boyer–Lindquist, these are regular on the spin axis *and* at the horizon, so there is no coordinate singularity to special-case (no streak along the axis) and no trigonometry in the inner loop. The metric is `g_μν = η_μν + f lₘ lₙ` with inverse `g^μν = η^μν − f l^μ l^ν` (`l` null), giving the photon Hamiltonian
  `H = ½ g^μν p_μ p_ν = ½[ −E² + |p|² − f(E + l·p)² ]`,
  where `f = 2Mr³/(r⁴ + a²y²)`, `l = (1, (rx+az)/(r²+a²), y/r, (rz−ax)/(r²+a²))`, and the Kerr radius `r(x,y,z)` solves `(x²+z²)/(r²+a²) + y²/r² = 1`. The Cartesian position `X` and momentum `p` are advanced directly by RK4; `E = −p_t` is conserved.
- **Why this method** – it is *exact* (the Hamiltonian / null condition holds to ~10⁻⁷ along every ray, vs. the Boyer–Lindquist form which blows up near the axis), needs **no** softening/reflection/step-cap hacks, and is cheaper: ~half the steps and only rational functions + one `sqrt` per evaluation instead of two transcendentals.
- **Initial conditions** – `kerrInit()` picks the photon momentum so its coordinate velocity `dX/dλ` is parallel to the camera ray `dir` (energy normalised to `E = 1`), solving the null condition in closed form. No tetrad or oblate-spheroidal inversion needed.
- **Step control** – the same position-based adaptive step as the Schwarzschild path: `h = clamp(0.05·R, 0.02, 0.8)`, finer near the hole. Capped at `KERR_STEPS = 300`.
- **Termination / disk** – black when the Kerr radius `r < r₊`, skybox when `|X| > 40` (the escape direction is just the Cartesian velocity `dX/dλ`, already in world space). Disk crossings are detected by a sign change in `X.y` (the ray passing through the equatorial plane `y = 0`), exactly like the Schwarzschild path, and reuse `diskDensity()` / `diskEmission()`, so the disk model is shared between both metrics.

## `bright.frag`

Bloom threshold extraction. Computes per-pixel luminance using Rec. 709 coefficients (`0.2126 R + 0.7152 G + 0.0722 B`). Pixels with luminance above `1.0` pass through; all others are zeroed. This isolates the super-white regions (the bright side of the Doppler-shifted disk and lensed starfield) for the bloom pipeline.

## `blur.frag`

Separable two-pass Gaussian blur. Uses a 5-tap kernel with hardcoded weights `[0.227, 0.195, 0.122, 0.054, 0.016]` (normalised to sum ≈ 1). The `uHorizontal` uniform selects the blur direction; `uTexelSize` is the reciprocal of the half-resolution FBO dimensions. Called 10 times (5 horizontal + 5 vertical) in a ping-pong fashion between the two bloom FBOs to produce a wide, smooth glow.

## `post.frag`

Final compositing and tone-mapping. Adds the bloom texture (scaled by `BLOOM_STRENGTH = 0.5`) to the HDR scene colour, applies **ACES filmic tone-mapping** to compress HDR values into `[0, 1]`, and finally applies gamma correction (`γ = 2.2`). The result is written to the default framebuffer for display.
