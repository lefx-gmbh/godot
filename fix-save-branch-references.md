# Node references survive Save Branch as Scene

Branch: [`fix/save-branch-references`](https://github.com/lefx-gmbh/godot/tree/fix/save-branch-references), on top of Godot `master`. It is in `fixes/master`.

## Problem

Node `A` has `@export var target: Node`, set to its child `B`. You right-click `B` and choose **Save Branch as Scene**. `A`'s Inspector still shows `B`, but the next save writes `target = NodePath("")`. The reference is gone. The same happens to a reference to a child of `B`, and to `Node` entries in an `Array[Node]`.

## Cause and fix

Save Branch as Scene saves the branch, then replaces it with a new instance of the saved scene. Node references elsewhere in the scene still pointed at the old nodes, which are no longer in the tree. A `NodePath` property is unaffected, because the new nodes have the same names.

Change Type has the same problem and already solves it in `SceneTreeDock::perform_node_replace()`, with undo. Save Branch as Scene now calls that function too, for the branch root and every node below it. The function now takes a map from old node to new node, so it walks the scene once for the whole branch. Change Type passes a map with one entry and behaves as before. File: `editor/docks/scene_tree_dock.cpp`.

## Measured

- [#84016](https://github.com/godotengine/godot/issues/84016): in a headless editor run, we used the real Save Branch action on `B`. We checked references to `B`, to `B/C` and in an `Array[Node]`. On an unchanged build of the same Godot `master` commit, all three are lost and saved as `NodePath("")`. On this fork all three point to the new instance and save as `NodePath("B")` and `NodePath("B/C")`. Undo restores the old state on both builds.

Not reproduced, so not claimed: [#44526](https://github.com/godotengine/godot/issues/44526). We rebuilt four Save Branch variants with instance overrides and editable children, and all of them pass on the unchanged build.

## Cost

None at run time. It is editor code, and it runs once per Save Branch as Scene: one walk over the edited scene.

## Tests

None automated. `SceneTreeDock` needs the running editor, which the unit tests do not start. To check it by hand:

1. Attach a script with `@export var target: Node` to a root node `A`. Add a child `B`, and set `target` to `B`.
2. Right-click `B`, then **Save Branch as Scene**.
3. Save the scene and open the `.tscn` in a text editor. The fork writes `target = NodePath("B")`. An unchanged build writes `NodePath("")`.
