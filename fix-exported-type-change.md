# An exported value survives a change of its type

Branch: [`fix/exported-type-change`](https://github.com/lefx-gmbh/godot/tree/fix/exported-type-change), on top of Godot `master`. It is in `fixes/master`.

## Problem

A script has `@export var speed: int = 10`, and you set `speed` to 42 on a node. Then you change the script to `@export var speed: float = 10.0`. The node now shows 10.0. Your 42 is gone, and the next save writes the loss to disk.

## Cause and fix

In the editor, a script without `@tool` runs as a placeholder that stores the values you set. When the script changes the type of a property, the placeholder replaced a stored value of the old type with the new default. The fix converts the value instead when two conditions are true:

- Godot's strict conversion rules allow it (`Variant::can_convert_strict()`).
- The conversion back gives the same value, so nothing is lost.

So 42 becomes 42.0. A conversion that loses information still takes the new default, for example 1.5 to an int. An unrelated type also takes it, for example a String to an int. File: `core/object/script_language.cpp`.

## Measured

- [#46103](https://github.com/godotengine/godot/issues/46103): we set 42 on a single node in the editor and changed the type to float. On an unchanged build of the same Godot `master` commit, the value becomes 10.0. On this fork it becomes 42.0. We did not rebuild the inherited scene from the report.

## Cost

None at run time. The change runs only when the editor updates a script's exported properties.

## Tests

`tests/core/object/test_placeholder_script_instance.cpp`, case "Updating the scripts default values."
