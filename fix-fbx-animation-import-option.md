# FBX import respects `animation/import=false`

Branch: [`fix/fbx-animation-import-option`](https://github.com/lefx-gmbh/godot/tree/fix/fbx-animation-import-option), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

You import an FBX file and turn off the import option **Animation > Import**. The imported scene still gets an `AnimationPlayer`. Most exporters write a default animation take, also for a model without animation. So a static model gets an `AnimationPlayer` with one empty animation. A glTF file with the same option gets no `AnimationPlayer`.

## Cause and fix

The scene importer gives the option to each format importer. The glTF importer reads it and tells its state not to create animations. The two FBX importers, ufbx and FBX2glTF, did not read the option. Their state kept the default, which creates animations.

Both FBX importers now read the option in the same way as the glTF importer. Files: `modules/fbx/editor/editor_scene_importer_ufbx.cpp`, `modules/fbx/editor/editor_scene_importer_fbx2gltf.cpp`.

## Measured

No Godot issue found. We measured the behavior itself: an FBX file with one node and one empty animation take, imported headless with the option on and off. On an unchanged build of Godot `master`, both imports give an `AnimationPlayer`. On this fork, the import with the option off gives none. The import with the option on still gives one.

## Limits

With the option on, an empty default take still gives an `AnimationPlayer` with an empty animation. This fix does not change that default behavior.

The FBX2glTF path has no automated test, because it needs the external FBX2glTF program. Its change is the same as the one in the ufbx path.

## Cost

None measurable. The importer reads one more option per import.

## Tests

`modules/fbx/tests/test_fbx.h` imports `modules/fbx/tests/data/empty_take.fbx` through the ufbx importer with the option on and off. It fails on unchanged Godot `master` and passes on this fork.
