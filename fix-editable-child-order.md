# A node added under an editable instance keeps its place

Branch: [`fix/editable-child-order`](https://github.com/lefx-gmbh/godot/tree/fix/editable-child-order), on top of [`fix/clear-node-reference`](fix-clear-node-reference.md). It is in `fixes/master`.

## Problem

An instanced scene has **Editable Children** on. You add a node directly under the instance and move it between two of the instance's own children. You save and reopen. The node is now behind all of the instance's children.

## Cause and fix

A scene saves the position of a node only where it is needed. `SceneState::_parse_node()` skipped it when the node's parent belongs to the edited scene, because then all siblings come from the same file. An instance root also belongs to the edited scene, but its other children come from the instanced scene. So the added node lost its position.

The position is now saved when the parent is an instance root. Other nodes save as before. File: `scene/resources/packed_scene.cpp`.

## Measured

- [#99452](https://github.com/godotengine/godot/issues/99452), the reporter's layout, in a headless editor run. `Node2D_xxxx` is moved between the instance's `Node2D3` and `Node2D4`, then the scene is saved and reloaded. On an unchanged build of the same Godot `master` commit, the order after the reload is `Node2D3, Node2D4, Node2D_xxxx`. On this fork it is `Node2D3, Node2D_xxxx, Node2D4`.

## Limits

Every node added under an editable instance root now stores its position, also one simply added at the end. If the instanced scene later gains children, such a node keeps its stored position instead of staying last. Inherited scenes already behave this way. Scenes saved before the fix get the new `index` lines on their next save.

## Cost

One more condition while packing a scene. Affected nodes store one more number.

## Tests

`tests/scene/test_packed_scene.cpp`, case "Added child keeps its order among instance children". It has the editable instance and, as a control, a plain parent node. Without the fix the instance case fails.
