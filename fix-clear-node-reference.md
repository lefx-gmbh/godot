# A cleared Node reference stays cleared

Branch: [`fix/clear-node-reference`](https://github.com/lefx-gmbh/godot/tree/fix/clear-node-reference). It builds on `fix/stale-base-scene-state`, because its tests use the same test helpers. It is in `fixes/master` and `fixes/4.7`.

## Problem

A base scene sets a property of type Node, for example `@export var weapon: Node` points to a child `Sword`. A scene that inherits or instances the base clears the property. The Inspector shows it as empty. After you reopen the scene or run the game, the base's `Sword` is back.

## Cause and fix

During a save, Godot skipped every empty Node reference as "never set", before it compared the value with the base. So the clear was never written. The fix lets an empty reference in an instanced or inherited scene go through that comparison, and it saves `null` when the base sets a reference. Only a real null counts as a clear. Other values are skipped as before. File: `scene/resources/packed_scene.cpp`.

## Files from Godot 4.2

Godot 4.2 saved such a clear as `weapon = NodePath("")`, an empty path without the node-path marker. That form does not load as a clear either. In the editor, a script without `@tool` keeps the empty path, and each save writes it back, so the file stays wrong.

This fork saves an empty path in a Node property as `null`, so such a file heals on its next save. It does not change files when they load. A check on load would add work to every scene load in every game, for a form that only old files contain. A scene that nobody saves again, for example in an imported asset, keeps the old form. To heal it, open the scene and save it.

## Measured

- [#92879](https://github.com/godotengine/godot/issues/92879): we saved a clear in an inherited scene and in an instance. Both fail on an unchanged build of the same Godot `master` commit. Both pass on this fork. The reporter's own 4.2 file, opened and saved without changes, keeps `NodePath("")` on Godot and becomes `null` on this fork.

[#106990](https://github.com/godotengine/godot/issues/106990) describes the same defect for an instanced scene. We did not rebuild its project, so we make no claim about it.

An open Godot pull request, [#107032](https://github.com/godotengine/godot/pull/107032), changes the same code. It saves the clear as an empty path with the node-path marker. That form and `null` both load as an empty reference on Godot, on that pull request and on this fork.

## Cost

None at run time. The change runs only when a scene is saved.

## Tests

`tests/scene/test_packed_scene.cpp`, case "Clearing a Node reference set by the base".
