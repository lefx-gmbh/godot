# Compatibility: rough reflections are smooth and bright

Branch: [`fix/compat-rough-reflection`](https://github.com/lefx-gmbh/godot/tree/fix/compat-rough-reflection), on top of Godot `master`. It needs the fix [Compatibility: sky ambient light comes from the right direction](fix-compat-sky-ambient-direction.md), and the branch contains that fix and the fix it needs. It is in `fixes/master` and `fixes/4.7`.

## Problem

You use the Compatibility renderer and a sky for reflections. Metal with medium roughness, for example 0.5, shows patchy blotches in its reflection. The reflection is also too dark. Rough dielectric materials, for example plastic, have specular reflections that are too dark too. Forward+ does not have these problems.

## Cause and fix

There are two causes.

**The filter of the radiance map.** The renderer bakes the sky into a radiance map with several levels. Each level is blurrier than the one before. A GGX filter makes each level from samples of the sky cubemap. Each sample reads a mipmap of the sky that matches the area that the sample covers. Two faults made the samples read the wrong mipmap:

- Godot 4.7.2 compares the area of a sample with a texel of the level that the filter writes. The samples read the sky cubemap, which is larger. So they read mipmaps that are 2 to 4 levels too sharp. Too few samples then alias, and each level shows blotches.
- Godot 4.7.2 and `master` add a mipmap bias to each sample. On `master` the area comparison uses the texel of the sky cubemap, so the bias makes each level blurrier than its lobe needs.

The fix compares the area with a texel of the sky cubemap, removes the bias, and takes more samples for the middle levels. Reflection probes use the same filter. A probe that stores sRGB encoded data (on a GPU without float render targets) keeps the previous filter, because its small mipmaps are too dark.

**The environment BRDF.** The scene shader scales reflections with an analytic fit of the environment BRDF (Lazarov 2013). Its scale term falls too fast with roughness. For white metal at roughness 0.5 the fit gives 0.725. The exact single-scatter value is 0.84 to 0.92, depending on the view angle. The fix replaces the term `-0.572 * roughness` with `-0.566 * roughness²`. Roughness 0 does not change.

Files: `drivers/gles3/effects/cubemap_filter.cpp`, `drivers/gles3/rasterizer_scene_gles3.cpp`, `drivers/gles3/shaders/scene.glsl`, `drivers/gles3/storage/config.cpp`, `drivers/gles3/storage/config.h`, `servers/rendering/rendering_server.cpp`, `doc/classes/ProjectSettings.xml`, and a test.

## Project setting

The project setting `rendering/reflections/sky_reflections/accurate_rough_reflections` switches both changes. It is on by default. Set it to off to get the previous output. With the setting off, our renders are identical to the build without this fix, pixel for pixel. The renderer reads the setting at startup, so a change needs a restart. Forward+ and Mobile do not use it.

## Measured

The scene has two white metal spheres with roughness 0.5 and 1.0 under a studio HDRI, no lights, Linear tonemap, exposure 1. The reference is a path-traced render (Blender Cycles, single-scatter GGX, direct light). Brightness is the mean over a sphere, as a ratio to the reference. Blotch is the RMS of the patchy error over the sphere. Lower is smoother. The reference itself scores 0.029, which is its sampling noise.

| Build | Roughness 0.5: brightness | Roughness 0.5: blotch | Roughness 1.0: brightness |
| --- | ---: | ---: | ---: |
| `fix/compat-sky-ambient-direction` (Godot `master`), before this fix | 0.73 | 0.021 | 1.11 |
| `fix/compat-rough-reflection` (Godot `master`) | 0.85 | 0.016 | 1.13 |
| `fixes/4.7`, setting off | 0.71 | 0.041 | 1.11 |
| `fixes/4.7`, setting on | 0.85 | 0.016 | 1.13 |
| `fixes/4.7`, Forward+ | 1.02 | 0.020 | 2.85 |

At roughness 1.0 Forward+ is much brighter, because it adds multi-scatter energy compensation. The reference does not have it.

Rough dielectric: three black spheres with specular 0.5 and roughness 0.25, 0.5 and 0.75, exposure 4. Brightness over the sphere as a ratio to the reference, on `fix/compat-rough-reflection` (Godot `master`):

| Roughness | Before this fix | This fix |
| --- | ---: | ---: |
| 0.25 | 0.79 | 0.86 |
| 0.5 | 0.63 | 0.71 |
| 0.75 | 0.68 | 0.78 |

A second scene has a white sphere, a chrome sphere, a glass pane and a floor under the same HDRI. It has no rough materials. It gives the same pixels with and without this fix.

## Cost

Scene shading adds one multiply-add for each pixel.

The filter reads about twice as many texels for each sky update. We measured it on `fixes/4.7`, desktop OpenGL 3.3, RTX 3070, with a sky shader that updates every frame. GPU time of the whole frame, median of 3 runs:

| Radiance size | Setting off | Setting on | Difference |
| --- | ---: | ---: | ---: |
| 256 px (default) | 0.94 ms | 0.94 ms | none measured |
| 2048 px | 2.41 ms | 3.19 ms | +0.78 ms |

A panorama sky updates only when it changes, so it pays this cost once. A sky shader that uses `TIME` or `POSITION` pays it every frame. We did not measure mobile GPUs. In Chrome (WebGL 2) the rendered image is within 1 of 255 of desktop, with no errors in the console.

## Limits

- The Compatibility renderer still uses the split-sum approximation. White metal at roughness 0.5 stays at 0.85 of the reference.
- For white metal the BRDF fit depends on roughness only, not on the view angle. Rough dielectrics get brighter, but they stay too dark at grazing angles. Near the rim, one sphere (roughness 0.5) is 10 % too bright.
- Probes that store sRGB encoded data keep the previous filter and its blotches.
- The Mobile renderer uses the same BRDF fit and is not changed.

## Tests

A unit test checks that the project setting exists, is on by default and needs a restart. The image changes need a rendered image, so check them by hand. Use the Compatibility renderer and the Linear tonemapper. Add a `WorldEnvironment` with a `PanoramaSkyMaterial` that uses a studio HDRI, and reflections from the sky. Look at a white metal sphere with roughness 0.5. Without this fix, or with the setting off, the reflection has blotches and is darker than in Forward+. With this fix it is smooth and closer to Forward+.
