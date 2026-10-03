# Pasted nodes keep their children inside an editable instance

Branch: [`fix/paste-editable-owner`](https://github.com/lefx-gmbh/godot/tree/fix/paste-editable-owner), on top of Godot `master`. It is in `fixes/master`.

## Problem

An instanced scene has **Editable Children** on. You copy a node that has children and paste it onto one of the instance's nodes. Everything looks right. After saving and reopening the scene, only the pasted node is left. Its children and grandchildren are gone.

## Cause and fix

`SceneTreeDock::paste_nodes()` gave the pasted node the edited scene as its owner. Its descendants got the owner of the node you pasted onto. Inside an editable instance, that owner is the instance's root, not the edited scene. A scene saves only the nodes it owns, so the descendants were dropped.

The descendants now get the edited scene as their owner, like the pasted node. For a paste onto an ordinary node nothing changes, because there the two owners were already the same. File: `editor/docks/scene_tree_dock.cpp`.

## Measured

- [#115894](https://github.com/godotengine/godot/issues/115894), in a headless editor run with the real Copy and Paste actions. A node with a child and a grandchild is pasted onto a node inside an editable instance, then the scene is saved and reloaded. On an unchanged build of the same Godot `master` commit, only the pasted node remains. On this fork all three levels remain. An ordinary paste, as a control, works on both builds.

Not rebuilt, same mechanism by their description: [#116484](https://github.com/godotengine/godot/issues/116484) and [#66020](https://github.com/godotengine/godot/issues/66020).

## Cost

None.

## Tests

None automated. `SceneTreeDock` needs the running editor, which the unit tests do not start. To check it by hand: turn on Editable Children on an instance, copy a node with a child, paste it onto a node of the instance, save, and reopen the scene. The child is still there on the fork.
