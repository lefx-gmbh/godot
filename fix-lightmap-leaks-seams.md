# Baked lightmaps keep light out of closed corners

Branch: [`fix/lightmap-leaks-seams`](https://github.com/lefx-gmbh/godot/tree/fix/lightmap-leaks-seams), on top of Godot `master`. It is in `fixes/master`. It is not in `fixes/4.7` yet.

## Problem

You bake a `LightmapGI` with the GPU lightmapper. Light from the sky or from a baked light leaks through closed corners. A bright line appears where two walls meet, or where a wall meets the floor. A one-sided room that is closed on all sides still gets light along its edges. The baked lightmap is also half a texel away from the UV2 of the mesh, so baked shadow edges sit in the wrong place. Some UV seams show a step in the light.

## Causes and fixes

The lightmapper had several errors at triangle edges and chart edges. All changes are in `modules/lightmapper_rd/`.

1. **Samples on a triangle edge.** The raster pass moved the geometry by half a texel, so that no sample lay on a triangle edge. That moved the whole lightmap by half a texel. A bounce ray starts a small distance (the bias) away from its sample, and it ignores hits nearer than the bias. So a sample near an edge can send rays past the wall that meets that edge. The raster pass now samples at the texel centers. It keeps each sample 2.1 times the bias away from the triangle edges, in world units. Files: `lm_raster.glsl`, `lightmapper_rd.cpp`.
2. **Antialiasing samples behind a wall.** The direct light pass moves each antialiasing sample by up to half a texel along the surface. Near a wall, the moved sample can go through the wall and see the light. Now a ray checks the path from the texel to the moved sample. If it hits something, the sample stays at the texel. File: `lm_compute.glsl`.
3. **A broken tangent frame.** For a surface or a light direction that points straight down, the direct light pass got an invalid tangent. The shadow rays then hit nothing. Ceilings under omni lights and the umbra of a sun that points straight down were lit. File: `lm_compute.glsl`.
4. **Samples moved through one-sided planes.** The unocclude pass moves a sample when it finds a back face within one texel. It treats that face as the inside of an object. But a one-sided plane also shows its back face. For example, a one-sided lip at the edge of a ledge shows its back face to the underside of the ledge. The pass moved those samples through the lip, where the lights under the ledge cannot reach them. Now the pass moves a sample only in two cases. The surface of the sample continues at the new position, or the sample is inside a closed object. A face flush against a wall still moves out of the wall. File: `lm_compute.glsl`.
5. **Dilate wrote over valid texels.** The dilate pass now fills only empty texels. It marks the texels that it fills, so the seam pass can tell them apart. File: `lm_compute.glsl`.
6. **OIDN and the shadowmask.** After the OIDN denoiser, the code copied the alpha back in 16-bit steps. The shadowmask uses 8 bits per channel, so the copy wrote the wrong bytes. It did not restore the alpha of every second pixel, and it wrote over a color channel. The copy now uses the pixel size of the format. File: `lightmapper_rd.cpp`.
7. **The seam pass.** The pass now draws the texels outside each chart edge with the value of the first texel inside the other chart. It reads that texel at its center, also when the two charts have a different UV winding. Files: `lm_blendseams.glsl`, `lightmapper_rd.cpp`, `lightmapper_rd.h`, `lm_common_inc.glsl`.
8. **Seams on charts thinner than a texel.** On such a chart, the first texel inside is often a texel that the dilate pass filled. That texel can come from another chart. A copy then writes a wrong value. So the seam pass first checks that it can read both sides of the seam. If it cannot, it averages the two sides on the edge and in the gutter. A texel that the dilate pass filled takes the value of the other side. File: `lm_blendseams.glsl`.

## Measured

- [#117468](https://github.com/godotengine/godot/issues/117468), with the project attached to the issue. We baked it with the editor's **Bake Lightmaps** button and read the baked lightmap back. A leak shows as a corner texel that is much brighter than the wall next to it. We compare each texel along a concave corner with the median of the same wall 1.5 to 4 texels away. We leave out every texel that the baked sun can reach on any part of its area. Such a texel can be bright without a leak. 936 corner texels remain.
  - Unchanged build of the same Godot `master` commit: 132 texels are more than 3 times as bright as their wall. 343 are more than 1.5 times as bright.
  - `fix/lightmap-leaks-seams`: 3 texels are more than 3 times as bright, and 62 more than 1.5 times. `fixes/master`: 4 and 62.
- [#90929](https://github.com/godotengine/godot/issues/90929), with the project attached to the issue. The mean step across UV seams is 0.096 of the local light on the unchanged build. It is 0.048 on `fixes/master` and 0.053 on `fix/lightmap-leaks-seams`. We do not list #90929 as fixed. We did not set a pass limit for a seam step before we measured it.

We also measured the fix with our own test scenes, on a build with the same lightmapper code on an earlier Godot `master` commit. Results:

- A closed one-sided room under a bright sky: the brightest texel goes from 4.18 to 0.02.
- Corners of one-sided rooms with a window: the mean light along the corners drops to about a third.
- Shadow edges: the mean distance from the true edge goes from 0.50 to 0.15 texel.
- An authored UV seam on a cylinder: no step at the seam, also when one chart is mirrored. A seam that runs diagonally through the lightmap keeps a step of 1 to 2 %.
- A floor under a sun that points straight down, with soft shadows: the lit texels in the umbra go from 324 to 0.
- With OIDN and a shadowmask: the dark gutter texels go from 201 to 0.
- Bake time: within 5 % of the unchanged build.

## Limits

- In the project of [#69126](https://github.com/godotengine/godot/issues/69126), the mean step across UV seams goes up. It goes from 0.189 to 0.235 of the local light. That map has automatic UV2 with many charts smaller than a texel. The fix samples at the texel centers. On this map, 68 % of the triangles contain no texel center. Their charts then have no texel of their own. Each texel near such a seam is a copy from the dilate pass, often of another chart. So no seam pass can match the two sides. Larger charts do not have this problem. Godot closed #69126, and its leak does not show on the unchanged build. We do not list it.
- On triangles smaller than about 6 times the bias, many texels move to the same small area. The light inside such a triangle then becomes flat. This is visible only with a large bias.
- With supersampling factor 3, the bake downscales the lightmap with a filter that skips texels. This fix does not change that.
- At a low texel density, the built-in denoiser adds about 7 % light against a bake at a high texel density. It does this also on the unchanged build. This fix does not change the denoiser.

## Cost

One extra ray per antialiasing sample and light in the direct light pass. The unocclude pass traces up to six more rays for a sample that it can move. It runs once per bake. In our test scenes the bake time changed by less than 5 %. No cost at run time.

## Tests

None automated. The lightmapper needs a GPU and the editor, which the unit tests do not start. To check it by hand: bake the project of #117468 and look at the corners of the inner walls. The fork shows no bright lines there.
