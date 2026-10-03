# Make Unique (Recursive) makes nested resources unique

Branch: [`fix/make-unique-recursive`](https://github.com/lefx-gmbh/godot/tree/fix/make-unique-recursive), on top of Godot `master`. It is in `fixes/master`.

## Problem

You use **Make Unique (Recursive)** on an AnimationLibrary, an AnimationNodeBlendTree or an AnimationNodeStateMachine, with every sub-resource selected in the dialog. The library or tree itself becomes a copy. Its animations, nodes or states are still shared with the original. Editing them in the copy changes the original too.

## Cause and fix

The command duplicates each selected sub-resource and puts the copy back into its parent. Two cases failed:

- **Arrays and dictionaries.** The parent got a copy of the container first, and the copies of the sub-resources were put into it afterwards. AnimationLibrary stores its animations through a setter that keeps its own copy of the container, so the later changes never reached it. The command now sets the container again once it is filled. File: `editor/inspector/editor_resource_picker.cpp`.
- **Nodes of blend trees and state machines.** The copy went back through the storage properties `nodes/<name>/node` and `states/<name>/node`. Those only added nodes and refused a name that already existed. They now replace an existing node in place, so its position, connections and transitions stay. A state machine uses its existing `replace_node()`. A blend tree still refuses to replace its output node. Files: `scene/animation/animation_blend_tree.cpp`, `scene/animation/animation_node_state_machine.cpp`.

## Measured

In a headless editor run, through the real picker action: menu entry, dialog, confirm.

- [#111130](https://github.com/godotengine/godot/issues/111130), AnimationLibrary with one animation. On an unchanged build of the same Godot `master` commit, the copy's animation is the original's. On this fork it is a separate copy.
- [#94646](https://github.com/godotengine/godot/issues/94646), BlendTree with one node and StateMachine with one state. On the unchanged build, both stay shared and the engine logs `Condition "nodes.has(p_name)" is true` (or `states`). On this fork both are separate copies.

## Cost

None outside this command. Setting a storage property for a node name that already exists used to fail with an error and now replaces the node.

## Tests

`tests/scene/test_animation_blend_tree.cpp`, case "Replace existing child node through storage property". It checks the node replacement and that position, connections and transitions stay. The picker part needs the running editor, which the unit tests do not start. It was measured as described above.
