# A duplicated node's connections are not locked

Branch: [`fix/duplicate-inherited-flag`](https://github.com/lefx-gmbh/godot/tree/fix/duplicate-inherited-flag), on top of [`fix/duplicate-always-flag`](fix-duplicate-always-flag.md). It is in `fixes/master`.

## Problem

In an inherited scene, or under Editable Children, a node has a signal connection that comes from the base scene. The editor shows such a connection as locked, because you edit it in the base scene. You duplicate the node with Ctrl+D. The copy's connection is locked too, and you cannot disconnect it until you reopen the scene. The copy is a new node of your scene, so nothing about it comes from the base scene.

## Cause and fix

`Node::_duplicate_signals()` copied each connection with all its flags, including `CONNECT_INHERITED`, which marks a connection from a base scene. Saving already strips that flag, so the file was always correct. Only the editor's live state was wrong.

The copy's connections no longer get that flag. File: `scene/main/node.cpp`.

## Measured

- [#117415](https://github.com/godotengine/godot/issues/117415), in a headless editor run with the real Duplicate action. The base scene connects `A.renamed` to `B`, and Ctrl+D runs on `A` in the inherited scene. On an unchanged build of the same Godot `master` commit, the copy's connection is marked inherited. On this fork it is not. The original keeps its mark on both builds.

## Cost

None.

## Tests

`tests/scene/test_node.cpp`, case "Duplicating node clears CONNECT_INHERITED on copied connections". It duplicates a whole branch and a single node. Without the fix both fail.
