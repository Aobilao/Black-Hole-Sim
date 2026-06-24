# Bakes the grayscale cloud texture (assets/disk.png) that the shader wraps
# around the accretion disk. This is NOT physics -- it is just procedural art:
# a tiling noise pattern that looks like turbulent gas. The shader scrolls this
# texture around the disk to fake the swirling motion.
#
# Width is the angle around the disk (must tile seamlessly so the swirl has no
# visible seam), height is the radius (inner edge -> outer edge).
import numpy as np
from PIL import Image

rng = np.random.default_rng(7)   # fixed seed so the texture is reproducible
H, W = 256, 1024                 # H = radial pixels, W = angular pixels

# Make smooth, seamlessly-tiling noise. We start from pure random "white" noise
# and, in the frequency domain (FFT), divide each frequency by its magnitude r.
# Suppressing high frequencies turns harsh static into soft, billowy clouds
# ("1/f" or pink-ish noise). Doing it via the FFT guarantees the result wraps
# around perfectly. ax/ay stretch the cloud cells along angle vs. radius.
def periodic_noise(ax, ay):
    white = rng.standard_normal((H, W))
    F = np.fft.fft2(white)
    fy = np.fft.fftfreq(H)[:, None]
    fx = np.fft.fftfreq(W)[None, :]
    r = np.sqrt((fx * ax) ** 2 + (fy * ay) ** 2) + 1e-6
    F *= 1.0 / r
    return np.fft.ifft2(F).real

# Layer three noise scales (like octaves) for both broad and fine structure,
# then normalize to zero mean / unit spread.
n  = 1.00 * periodic_noise(ax=3.4, ay=0.8)
n += 0.55 * periodic_noise(ax=2.6, ay=1.3)
n += 0.30 * periodic_noise(ax=1.8, ay=2.0)
n = (n - n.mean()) / n.std()

# tanh gently squashes outliers into a pleasing 0..1 contrast centred on 0.5.
v = 0.5 + 0.36 * np.tanh(n * 0.95)

# Add a faint radial ripple so there are subtle concentric banding lines.
rad = np.linspace(0.0, 1.0, H)[:, None]
v += 0.05 * np.sin(rad * np.pi * 13.0)

v = np.clip(v, 0.0, 1.0)

img = (v * 255.0).astype(np.uint8)
Image.fromarray(img, mode="L").save("assets/disk.png")
print("wrote disk.png", img.shape, "mean", img.mean())
