# Compatibility: reflection probes render without exposure and stale shadows

Branch: [`fix/compat-probe-capture`](https://github.com/lefx-gmbh/godot/tree/fix/compat-probe-capture), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

You use the Compatibility renderer, a camera with an exposure other than 1.0, and a `ReflectionProbe`. A mirror surface shows the scene too dark. The reflected floor is much darker than the floor itself, and the horizon in the reflection has a hard edge. With a shadowed directional light and a probe that has `enable_shadows` off (the default), the reflection can also show black blobs.

## Cause and fix

Two causes:

- **Exposure and tonemapping counted twice.** The probe pass got the scene exposure, and the scene shader applied it. The surface that samples the probe applied it again. With glow, SSAO and adjustments off, the shader also tonemapped the probe, and the main pass did it again. The cleared background got the exposure only once, which gave the hard edge at the horizon. A probe pass now uses exposure 1.0 and does not tonemap in the shader. Forward+ also renders probes without post-processing.
- **Shadows without a shadow map.** A probe with `enable_shadows` off gets no directional shadow map from the scene cull. But `_setup_lights` was always called with shadows on. The probe then sampled the shadow map of the main view with the wrong matrices. `_setup_lights` now gets shadows only when the render has a shadow atlas. A directional light counts as shadowed only then.

File: `drivers/gles3/rasterizer_scene_gles3.cpp`.

## Measured

An unchanged build of Godot `master`, this fork, and a path-traced reference (Blender Cycles, direct light) rendered the same scene. The scene has a mirror sphere and a floor with colour ambient light and no other light. Values are linear, the mean of the RGB channels over a region, exposure applied.

| Scene | Region | Unchanged `master` | This fork | Reference |
| --- | --- | ---: | ---: | ---: |
| Exposure 0.5 | Mirror sphere, reflection of the floor | 0.054 | 0.109 | 0.104 |
| Exposure 0.5 | The floor itself | 0.109 | 0.109 | 0.087 |

The look-dev scene has a key light and shadow, an HDR sky and exposure 0.5. We ran it without the sky fix. In that scene the reflected floor goes from 0.231 to 0.485 (reference 0.348). The floor itself is 0.462 there, against 0.343 in the reference, because of the shadowed-light difference in Limits.

A scene with a key light, a shadow and colour ambient light gives 0.275 and 0.503 (reference 0.273). In that scene the old value matched the reference by accident. The shadowed-light difference makes the floor twice too bright, and the probe fault halved it again. This fix does not change the floor itself.

Measurements on Godot 4.7.2 in 8-bit values, with colour ambient. They compare the floor with the floor in the reflection. Exposure 0.5: 138 and 97 before, 138 and 134 after. Exposure 0.25: 100 and 48 before, 100 and 97 after.

## Limits

- The probe clips at 1.0 linear now, not at 1 divided by the exposure, because the probe atlas is `GL_RGB10_A2`.
- A remaining Compatibility difference: a shadowed light is added in sRGB space, not in linear space. With a key light and shadow, the lit floor is about twice as bright as the reference. This fix does not change that.
- With a sky as ambient light, the floor in the capture also gets its ambient light from the sky map. See the page [Compatibility: sky light keeps its HDR values](fix-compat-sky-radiance-hdr.md).
- A light that has the probe's cull mask removed from its layers does not light the probe. That is normal Godot behavior.

## Cost

The probe pass skips the exposure and one tonemap. We did not measure the cost on `master`. We measured one web build of Godot 4.7 (WebGL 2, Chrome) with the multiply, sky and probe fixes together. It had no measurable frame time cost (0 to −3 %).

## Tests

No automated test, because the check needs a rendered image. To check it by hand, use the Compatibility renderer, a camera with exposure 0.5 and colour ambient light. Add a floor, a mirror sphere (metallic 1, roughness 0) and a `ReflectionProbe` over them. On an unchanged build of `master` the floor in the mirror is about half as bright as the floor. On this fork the two match. For the shadows, add a shadowed `DirectionalLight3D` and leave `enable_shadows` off on the probe. An unchanged 4.7.2 build showed black blobs in the reflection. This fork does not.
