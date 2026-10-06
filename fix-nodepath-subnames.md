# A NodePath to a property survives moving its node

Branch: [`fix/nodepath-subnames`](https://github.com/lefx-gmbh/godot/tree/fix/nodepath-subnames), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

A node holds a `NodePath` that points to a property, such as `../Target:position`. It can be a direct export or a path inside a resource in an `Array[Resource]`. You drag the node somewhere else in the scene tree. The path becomes `../../Target:position:position`, and every further move adds another `:position`.

## Cause and fix

When nodes move or are renamed, the editor rewrites the NodePaths that refer to them. `SceneTreeDock::_update_node_path()` handles the case where the node holding the path moves. It built the old absolute path, property part included, and took the relative path to it with `NodePath::rel_path_to()`. That function keeps the property part. Then the code appended the property part a second time.

The result of `rel_path_to()` is now used as it is. File: `editor/docks/scene_tree_dock.cpp`.

## Measured

- [#122468](https://github.com/godotengine/godot/issues/122468), in a headless editor run with the scene tree's real drag and drop. `Holder` has `../Target:position` as a direct export and inside a resource in an `Array[Binding]`, and is moved into a sibling node.
  - On an unchanged build of the same Godot `master` commit, both paths become `../../Target:position:position`. After a second move, of the target this time, they stay doubled.
  - On this fork they become `../../Target:position`, then `../Target:position`.
  - A path without a property part is unchanged on both builds.

## Cost

Less work than before: one string concatenation and one parse fewer per rewritten path.

## Tests

None automated. `SceneTreeDock` needs the running editor, which the unit tests do not start. To check it by hand: set a NodePath export to `../Target:position`, drag its node under another node, and look at the path. The fork shows `:position` once.
