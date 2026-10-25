# Directional lightmaps get the full bounce light

Branch: [`fix/lightmap-sh-bounce`](https://github.com/lefx-gmbh/godot/tree/fix/lightmap-sh-bounce), on top of Godot `master`. It is in `fixes/master`. It is not in `fixes/4.7` yet.

## Problem

You bake a `LightmapGI` with **Directional** on. Indirect light, such as light from the sky, is darker than in the same bake with **Directional** off. Under a uniform white sky, a flat white floor gets 0.81 of the value of the non-directional bake. Bakes with many rays can lose light in both modes.

## Causes and fixes

Both changes are in `modules/lightmapper_rd/lm_compute.glsl`.

1. **SH weights.** The bounce pass traces cosine-weighted rays. It stored each ray in the spherical harmonics with fixed weights. These weights suit uniform rays. With cosine-weighted rays, they count the cosine two times. Now the pass stores each ray like a direct light from the ray direction. It uses the same ratio of L1 to L0 weights as the direct light pass. The directional value at the surface normal is then the plain average of the rays. That is the value of a non-directional bake.
2. **Rounding.** The bounce pass adds the result of each dispatch to a half-float image. A bake with 4096 rays and the default rays per pass writes each texel 1024 times. Some drivers round toward zero when they store a half-float value. Each store then loses a small part of the light, and over many stores the loss becomes large. Now the shader rounds the sum to the nearest half-float value before the store. The stored value is then exact, and the driver rounding has no effect.

## Measured

No Godot issue found. We measured the behavior itself: a 20 m white plane under a white custom-color environment, bounces 1, quality Ultra with 4096 rays, denoiser off. We baked it with **Directional** on and off with the editor's **Bake Lightmaps** button. Then we read the baked lightmap back and evaluated it at the plane normal, as the renderer does.

| Build | Directional / non-directional |
| --- | --- |
| Unchanged build of the same Godot `master` commit | 0.81 |
| `fix/lightmap-sh-bounce` | 0.97 |

The non-directional value is 1.00 on all builds, which is the exact value for this case. `fixes/master` has the same lightmapper code as `fix/lightmap-sh-bounce`. We did not measure it separately.

We also measured the fix with our own test scenes, on a build with the same lightmapper code on an earlier Godot `master` commit. Godot changed no lightmapper file between the two commits. With other sky shapes and a tilted plane, the ratio is between 0.95 and 1.02. Before the fix it was between 0.81 and 1.05. Scenes with only a baked sun do not change.

We measured on an NVIDIA GeForce RTX 3070 (driver 610.74), which rounds toward zero on this store. On a driver that rounds to the nearest value, we expect a smaller change. We did not measure that.

## Limits

About 3 % of the light is still missing in the white sky case. The rounding fix does not remove the error of the last half-float bit.

## Cost

A few arithmetic operations per texel and dispatch in the bounce pass. In our test scenes the bake time did not change measurably. No cost at run time, and no extra memory.

## Tests

None automated. The lightmapper needs a GPU and the editor, which the unit tests do not start. To check it by hand: bake a white plane under a white environment, once with **Directional** on and once off, and compare the brightness.
