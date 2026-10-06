# Detach Script keeps the values of a custom type

Branch: [`fix/detach-script-values`](https://github.com/lefx-gmbh/godot/tree/fix/detach-script-values), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

A script with `class_name MyBase` exports some variables. You add a `MyBase` node from the Add Node dialog, extend its script, and set the exported values in the Inspector. Then you use **Detach Script**. The node falls back to `MyBase`, as intended, but all of `MyBase`'s exported values are back at their defaults.

## Cause and fix

For a node made from a custom type, Detach Script does not remove the script. It puts back the custom type's base script. That script gets a new instance, which starts from its defaults.

Attach Script already saves the script properties before such a change and applies them afterwards. Detach Script now does the same. Values for properties that both scripts share are kept. File: `editor/docks/scene_tree_dock.cpp`.

## Measured

- [#120143](https://github.com/godotengine/godot/issues/120143), in a headless editor run with the real Detach Script action. The node runs a script that extends `MyBase`, with `speed = 5` and `label = "mine"`. On an unchanged build of the same Godot `master` commit, it has `speed = 1` and `label = "base"` after the detach. On this fork it has `speed = 5` and `label = "mine"`.

The report also says Undo did not restore the values. In our runs Undo restores them on both builds, so we make no claim about that part.

## Cost

None. Two calls per detached node, only in this editor action.

## Tests

None automated. `SceneTreeDock` needs the running editor, which the unit tests do not start. To check it by hand, follow the steps under Problem. The values stay on the fork.
