# Compatibility: sky light keeps its HDR values

Branch: [`fix/compat-sky-radiance-hdr`](https://github.com/lefx-gmbh/godot/tree/fix/compat-sky-radiance-hdr), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

You use the Compatibility renderer and a sky made from an HDR panorama (a `.exr` or `.hdr` file in a `PanoramaSkyMaterial`). The sky gives almost no light. A white sphere under such a sky looks gray, and a chrome sphere reflects a dark sky. We measured a studio HDRI on Godot 4.7.2. The blurry level of the sky map that diffuse light reads averages 0.11 in Compatibility and 0.63 in Forward+. A sky with an exposure below 1.0 is darker than its exposure says, because the exposure counted twice. At exposure 1.0, a sky made from a normal 8-bit image, and a `ProceduralSkyMaterial`, look right.

## Cause and fix

The renderer bakes the sky into a cubemap (the radiance map) that ambient light and reflections read. Four faults in that bake:

- The map was `GL_RGB10_A2`. It clips at 1.0, so the bright parts of an HDR panorama were lost. In the studio HDRI we tested, 16.5 % of the pixels are above 1.0.
- `sky.glsl` decoded the sky output as sRGB. That is right for an 8-bit panorama. A float panorama is already linear, so dark values became much darker (0.06 became about 0.005).
- The bake multiplied by the exposure, and the scene shader multiplied by it again. Sky light scaled with the exposure squared.
- The filter that makes the blurry levels averaged sRGB values. Small bright lights can fade out. We found this in the source and did not measure it alone.

The radiance map is now `GL_RGBA16F` and holds linear values. A GPU that cannot render to float textures keeps the old `GL_RGB10_A2`. The sky shader writes linear values to the map, without exposure and without the sRGB encode. The filter takes a flag for linear data and averages linear values. Reflection probes keep the old sRGB filter path.

`PanoramaSkyMaterial` encodes a float panorama as sRGB in the Compatibility renderer. The sky shader then decodes it with the exact sRGB curve, so the round trip gives back the linear values, also above 1.0. The round trip needs the exact curve, so it applies to every sky shader. Before, the sky shader used an approximation. Between 0 and 1 the two differ by about 1/255. Above 1.0 the exact curve gives lower values: 2.0 becomes 4.95 instead of 5.19. So sky output above 1.0 is darker than before, for example the sun disk of a `ProceduralSkyMaterial` or a custom sky shader with HDR values.

Files: `drivers/gles3/rasterizer_scene_gles3.cpp`, `drivers/gles3/effects/cubemap_filter.cpp`, `drivers/gles3/effects/cubemap_filter.h`, `drivers/gles3/shaders/effects/copy.glsl`, `drivers/gles3/shaders/effects/cubemap_filter.glsl`, `drivers/gles3/shaders/scene.glsl`, `drivers/gles3/shaders/sky.glsl`, `scene/resources/3d/sky_material.cpp`.

## Measured

An unchanged build of Godot `master`, this fork, and a path-traced reference (Blender Cycles, direct light) rendered the same scene. The scene has a white sphere, a floor, a chrome sphere and a ceiling under an HDR sky, Linear tonemap, exposure 1. Values are linear, the mean of the RGB channels over a region.

| Region | Unchanged `master` | This fork | Reference |
| --- | ---: | ---: | ---: |
| White sphere, top | 0.114 | 0.553 | 0.522 |
| White sphere, side | 0.039 | 0.207 | 0.233 |
| Floor | 0.100 | 0.445 | 0.321 |
| Chrome sphere, reflection of a bright wall | 0.365 | 0.635 | 0.773 |
| Chrome sphere, reflection of a dark ceiling | 0.000 | 0.023 | 0.025 |

At exposure 0.5 the top of the sphere goes from 0.055 to 0.276 (reference 0.261). The same scenes with a plain colour as ambient light and no sky do not change on any region.

## Limits

- Ambient light from the sky still uses the blurriest level of the radiance map, not an irradiance map. It depends on direction differently than the reference. The floor is too bright (0.445 against 0.321), and the side of the sphere is a little too dark. The fix [Compatibility: sky ambient light comes from the right direction](fix-compat-sky-ambient-direction.md) fixes this.
- Reflections are still darker than the reference next to bright areas (0.635 against 0.773), because the cubemap is coarse.
- The chrome sphere reads the reflection probe. At exposure 0.5 the probe still applies the exposure twice (0.159 against 0.386). So the fix [Compatibility: reflection probes render without exposure and stale shadows](fix-compat-probe-capture.md) is also necessary.
- A scene that made its sky brighter to make up for the fault looks brighter with this fix.

## Cost

Memory: the radiance map has 8 bytes per pixel instead of 4. On a GPU without float render targets the map stays at 4 bytes per pixel. We measured one web build of Godot 4.7 (WebGL 2, Chrome) with the multiply, sky and probe fixes together. It had no measurable frame time cost (0 to −3 %). We did not measure web on `master`.

## Tests

No automated test, because the check needs a rendered image. To check it by hand, use the Compatibility renderer and the Linear tonemapper. Add a `WorldEnvironment` with a `Sky` that uses a `PanoramaSkyMaterial` with an HDR panorama, and ambient light from the sky. Look at a white sphere with albedo 1 and no lights. On an unchanged build of `master` the top of the sphere is dark gray. On this fork it is much brighter (0.114 against 0.553 in linear values). With a flat panorama of 0.5, the sphere reads 128 on an unchanged 4.7.2 build, 188 with this fix, and 187 in Forward+.
