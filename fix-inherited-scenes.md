# Inherited scenes keep up with their base scene

Branches: [`fix/stale-base-scene-state`](https://github.com/lefx-gmbh/godot/tree/fix/stale-base-scene-state) (cause 1) and [`fix/editor-reload-base-chain`](https://github.com/lefx-gmbh/godot/tree/fix/editor-reload-base-chain) (causes 1 and 2). Both are in `fixes/master` and `fixes/4.7`.

## Problem

You change a base scene. A scene that inherits from it, or instances it, loses the change or shows an old version. Sometimes the inherited scene stores the old value as its own override, and from then on it no longer follows the base scene.

## Causes and fixes

1. **A save compares against the wrong base.** Some scenes are two or more levels away from their base. Such a scene read the base live during a save. If the base changed first, the save compared against the new base and stored the difference as overrides. The fix keeps a copy of the base state that the scene was built from. Trees that the editor builds after a base save get a copy that knows the newer base. Files: `scene/resources/packed_scene.{cpp,h}`.
2. **The editor does not see a change further down the chain.** To decide if an open scene needs a reload, the editor looked only at its direct base scene. The fix examines the full chain. Files: `editor/editor_data.{cpp,h}`.

## Measured

We rebuilt these Godot issues on Godot `master` (4.8 in development). Each one fails on an unchanged build of the same commit and passes on this fork:

- [#41492](https://github.com/godotengine/godot/issues/41492)
- [#43032](https://github.com/godotengine/godot/issues/43032)
- [#57089](https://github.com/godotengine/godot/issues/57089)
- [#28090](https://github.com/godotengine/godot/issues/28090)
- [#94912](https://github.com/godotengine/godot/issues/94912): an instance of an inherited Control scene, with `mouse_filter` changed three times in the base. Godot keeps the old value. This fork follows each change and stores no override.

[#7984](https://github.com/godotengine/godot/issues/7984), open since 2016, collects many reports of this kind. Other reports describe the same symptoms. We did not rebuild them, so we make no claim about them.

## Cost

The editor walks the base chain once per scene check and once per instantiation in edit state. It copies a scene state only when a base in the chain was saved. A game that runs without the editor carries none of this.

## Tests

`tests/scene/test_packed_scene.cpp` (cases "Stale base state") and `tests/editor/` (scene reload and repeated saves).
