# Compatibility: multiply-blended materials are not scaled by exposure

Branch: [`fix/compat-multiply-exposure`](https://github.com/lefx-gmbh/godot/tree/fix/compat-multiply-exposure), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

You use the Compatibility renderer and a material with the blend mode **Multiply**, for example tinted glass. You set a camera exposure below 1.0. The glass gets darker than its colour says. Blue glass with the colour (0.1, 0.4, 1.0) should pass 100 % of the blue light and 40 % of the green light. At exposure 0.5 it passes only half of that. Forward+ and Mobile do not have this problem.

## Cause and fix

A multiply-blended material does not output light. Its colour is a factor that says how much of the light behind it passes through. The scene shader still multiplied that factor by the exposure, as it does for light. With an exposure below 1.0, the factor became too small.

With glow on, the shader also multiplied the factor by the luminance multiplier. That is the second half of the same problem. Godot `master` fixed it in b90ae496cc: the shader skips that multiplication for multiply blending (the define `BLEND_MUL`). `fixes/4.7` carries a backport of that commit. This fix adds the same guard around the exposure line. File: `drivers/gles3/shaders/scene.glsl`.

## Measured

An unchanged build of Godot `master`, this fork, and a path-traced reference (Blender Cycles) rendered the same scenes. Values are linear, the mean of the RGB channels over a region, with exposure applied. The glass tile lies over a floor. We compare the blue channel of the tile over the floor next to it:

| Scene | Unchanged `master` | This fork | Reference |
| --- | ---: | ---: | ---: |
| Scene with colour ambient, exposure 0.5 | 0.528 | 1.000 | 0.970 |
| Scene with a key light, shadow and colour ambient, exposure 0.5 | 0.505 | 1.000 | 0.994 |

The mean over the whole tile goes from 0.023 to 0.043 at exposure 0.5 (reference 0.037). Regions that have no multiply material do not change: the floor, the spheres and the glass pane have the same values on both builds.

## Limits

- The blend itself still runs on 8-bit sRGB values. The green channel at exposure 0.5 is 0.169 on this fork, against 0.129 in the reference. This fix does not change that.
- A tonemapper other than Linear still bends the factor.
- Fog on a multiply material is not handled. It fades toward the fog colour, not toward 1.0.
- A shader that reads the screen texture to make glass is not changed.
- A scene that made its glass tint brighter to make up for the fault looks brighter with this fix.

## Cost

The shader skips one multiplication for multiply-blended materials. We did not measure the cost on `master`. We measured one web build of Godot 4.7 (WebGL 2, Chrome) with the multiply, sky and probe fixes together. It had no measurable frame time cost (0 to −3 %).

## Tests

No automated test, because the check needs a rendered image. To check it by hand, use the Compatibility renderer and a camera with exposure 0.5 (`CameraAttributesPractical` or `CameraAttributesPhysical`). Look at a plane in front of a floor. Give the plane a `StandardMaterial3D` with the blend mode **Multiply**, the colour (0.1, 0.4, 1.0) and **Shading mode** unshaded. On an unchanged build of `master` the blue channel of the floor seen through the glass is about half of the floor's blue channel. On this fork it is the same as the floor's.
