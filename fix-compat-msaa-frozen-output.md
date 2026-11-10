# Compatibility: the image does not freeze when post effects turn off with MSAA

Branch: [`fix/compat-msaa-frozen-output`](https://github.com/lefx-gmbh/godot/tree/fix/compat-msaa-frozen-output), on top of Godot `master`. It is in `fixes/master`. It is not in `fixes/4.7`, because Godot 4.7.2 does not have the change that causes the problem.

## Problem

You use the Compatibility renderer on Android or in WebXR, with 3D MSAA. A post effect that needs the internal buffer is on, for example glow, SSAO or the color adjustments. During play, you turn it off. The image then freezes. In XR, the headset keeps showing the last frame with the post effect. The scene still runs, but nothing new reaches the screen.

## Cause and fix

On these platforms, MSAA can render to a texture and resolve on the GPU tile. With the internal buffer on, the MSAA framebuffer has the internal color and depth textures attached. Since 81a533bb9e, `_check_render_buffers()` returns early when this framebuffer exists. When the post effects turn off, Godot frees the internal textures, but the MSAA framebuffer stays. The setup then does not run again. The scene renders into the deleted textures and never reaches the render target.

The fix frees the MSAA framebuffer together with the internal buffer. The setup then runs again in the same call and renders into the render target. An MSAA buffer that resolves has storage of its own, so it stays.

File: `drivers/gles3/storage/render_scene_buffers_gles3.cpp`.

## Measured

Meta Quest 3S (Adreno 740), OpenXR, Compatibility, MSAA 4x, 3D scale 1.0. A quad in front of the camera changes its color on each frame. On each frame, the test reads the center pixel of the left eye. A frame is "repeated" if its color is the same as on the frame before. The readback repeats about half of the frames also when the image is live. Compare each value with the share when the effect is on.

Each phase runs 90 frames with the effect on, then 90 frames with the effect off. The adjustments phase runs after the glow phase in the same session.

| Phase | Effect on: repeated frames | Unchanged `master`, effect off | This fork, effect off |
| --- | ---: | ---: | ---: |
| Glow | 41 of 81 | 82 of 82 (frozen) | 40 of 82 |
| Color adjustments | 41 of 81 | no valid readback in the whole phase | 40 of 82 |

We ran each build two times, with the same result.

## Limits

- Only Android and WebXR builds use render-to-texture MSAA, so only these platforms have the problem. A desktop build shows the same result with and without the fix.
- We measured on a Quest only. We did not test WebXR, or an Android phone without XR.
- A change of the 3D scale does not cause the problem. That change frees all buffers.

## Cost

The fix adds no work per frame. It runs only when the last post effect that needs the internal buffer turns off. Then Godot sets up the MSAA framebuffer again, as on the first frame.

## Tests

No automated test, because the problem shows only with OpenGL ES and render-to-texture MSAA. To check it by hand, export an XR project for a Quest with the Compatibility renderer and MSAA 4x. Turn on glow in the environment, then turn it off during play. On an unchanged build of `master`, the headset keeps the last frame with glow. On this fork, the image stays live.
