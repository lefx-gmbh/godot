# Compatibility: sky ambient light comes from the right direction

Branch: [`fix/compat-sky-ambient-direction`](https://github.com/lefx-gmbh/godot/tree/fix/compat-sky-ambient-direction), on top of Godot `master`. It needs the fix [Compatibility: sky light keeps its HDR values](fix-compat-sky-radiance-hdr.md), and the branch contains that fix too. It is in `fixes/master` and `fixes/4.7`.

## Problem

You use the Compatibility renderer and a sky as ambient light. Surfaces that face bright parts of the sky get too much ambient light. Surfaces that face away from them get too little. Under a studio HDRI with bright softboxes overhead, a floor is too bright and the sides of a sphere are too dark. Forward+ does not have this problem.

## Cause and fix

The renderer bakes the sky into a radiance map with several levels. Each level is blurrier than the one before. The last level has roughness 1.0, which is the cosine lobe that ambient light needs. The scene shader reads ambient light at the fixed level 5. A 256 px sky has 7 levels, so level 5 has roughness 0.83. That lobe is much narrower than a cosine lobe.

Two changes:

- The sky radiance map now has at most 6 levels, so level 5 is the roughness 1.0 level. The scene shader gets the level number from the same constant. The raw sky copy keeps all its levels, because the filter reads its small levels.
- The filter fills the roughness 1.0 level with an exact cosine convolution of the 8 px source level. Before, it used about 64 GGX samples, which gave a noisy result. Reflection probes use the same filter, so their ambient light also gets the exact convolution. Probes store sRGB encoded data, and their small levels are too dark. For that data the convolution reads the 16 px level.

The irradiance panorama bake (`RenderingServer.sky_bake_panorama` with irradiance on) reads the same level, so it also gets more accurate.

Files: `drivers/gles3/effects/cubemap_filter.cpp`, `drivers/gles3/rasterizer_scene_gles3.cpp`, `drivers/gles3/rasterizer_scene_gles3.h`, `drivers/gles3/shaders/effects/cubemap_filter.glsl`, `drivers/gles3/shaders/scene.glsl`.

## Measured

We measured this fix on Godot 4.7.2, with the sky radiance fix in both builds. We did not measure it on `master`. The scene has a white sphere and a floor under a studio HDRI, no lights, Linear tonemap, exposure 1. The reference is a path-traced render (Blender Cycles, direct light). Values are the ratio to the reference, linear, the mean of the RGB channels over a region:

| Region | Before this fix | This fix | Forward+ |
| --- | ---: | ---: | ---: |
| White sphere, side | 0.81 | 1.05 | 1.16 |
| White sphere, top | 1.02 | 0.99 | 0.91 |
| Floor | 1.39 | 1.06 | 0.98 |

Against the exact cosine irradiance of the same HDRI, over all directions of a sphere at 256 px:

| Check | Before this fix | This fix |
| --- | ---: | ---: |
| Sky ambient light, mean ratio and RMS error | 0.84, 20.6 % | 1.00, 1.9 % |
| Reflection probe ambient light, mean ratio and RMS error | 1.00, 9.2 % | 1.02, 6.1 % |
| Irradiance panorama bake, RMS error | 10.7 % | 1.6 % |

## Visible side effect

Rough sky reflections get blurrier. Forward+ shows the same blur. A material with roughness 1.0 read the roughness 0.83 level before, so its reflection was too sharp. We measured the contrast over a metal sphere with roughness 0.5. Contrast is the standard deviation divided by the mean, and lower is blurrier. Reference 1.42, before this fix 1.56, this fix 1.26, Forward+ 1.11.

## Limits

- The undersides of objects and the floor next to them still get too much sky light. That needs occlusion (SSAO or baked), not a better irradiance.
- A sky of 32 to 128 px has a roughness 1.0 level of only 4 px. Its error is about 5 % RMS.
- A large reflection probe costs more per update, because the convolution reads a larger level. At 2048 px and 4096 px that can be expensive, and with the update mode Always it repeats every frame. We did not measure it. At the default probe size of 256 px the cost is very small.
- A GPU without float render targets keeps the 10-bit sky map, which clips at 1.0.

## Cost

Scene shading does not change. The shader reads the same single level as before.

The cost is per sky update. We measured it on Godot 4.7.2, desktop OpenGL 3.3, RTX 3070, with a sky shader that updates every frame. GPU time of the whole frame, median:

| Radiance size | Before this fix | This fix | Difference |
| --- | ---: | ---: | ---: |
| 256 px (default) | 0.72 ms | 0.95 ms | +0.23 ms |
| 512 px | 1.04 ms | 0.96 ms | −0.08 ms |
| 1024 px | 1.59 ms | 1.20 ms | −0.39 ms |
| 2048 px | 3.87 ms | 2.44 ms | −1.43 ms |

At 512 px and up this fix is faster, because the map has fewer levels to filter. A panorama sky updates only when it changes, so it pays this cost once. A sky shader that uses `TIME` or `POSITION` pays it every frame. We did not measure mobile GPUs.

## Tests

No automated test, because the check needs a rendered image. To check it by hand, use the Compatibility renderer and the Linear tonemapper. Add a `WorldEnvironment` with a `PanoramaSkyMaterial` that uses a studio HDRI with bright lights overhead, and ambient light from the sky. Look at a white sphere with albedo 1 on a floor, with no lights. Compare it with the same scene in Forward+. Without this fix the floor is clearly brighter than in Forward+, and the sides of the sphere are darker. With this fix the two renderers are close.
