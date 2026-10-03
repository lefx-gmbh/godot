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

- [#46103](https://github.com/godotengine/godot/issues/46103), rebuilt as reported: two inherited scenes override `life_span = 42`, and an instance in a third scene sets 7. The script changes `:= 10` to `:= 10.0` while the scenes are open. On an unchanged build of the same Godot `master` commit, both show 10.0. A save then drops the 42 from the inherited scene and writes 10.0 over the 7. On this fork they stay 42.0 and 7.0, and save that way. A scene that is closed during the change is not affected on either build.

## Cost

None at run time. The change runs only when the editor updates a script's exported properties.

## Tests

`tests/core/object/test_placeholder_script_instance.cpp`, case "Updating the scripts default values."
