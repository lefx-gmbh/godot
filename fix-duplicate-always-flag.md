# Duplicating a node keeps every property marked ALWAYS_DUPLICATE

Branch: [`fix/duplicate-always-flag`](https://github.com/lefx-gmbh/godot/tree/fix/duplicate-always-flag), on top of Godot `master`. It is in `fixes/master`.

## Problem

A script adds properties through `_get_property_list()` with `PROPERTY_USAGE_ALWAYS_DUPLICATE`, for example strings or vectors. You duplicate the node with Ctrl+D, save it with **Save Branch as Scene**, or call `Node.duplicate()` in a game. The copy has the default values. Your values are gone.

## Cause and fix

The flag means: if the property holds a Resource, give the copy its own duplicate of it. `Node::_duplicate_properties()` set a flagged property only when it held a Resource. Any other value was skipped completely.

Now only a Resource value takes the duplicate path. Every other value is set the normal way, including the existing remapping of Node references inside the duplicated branch. File: `scene/main/node.cpp`.

## Measured

- [#82819](https://github.com/godotengine/godot/issues/82819), with the script from the report, in a headless editor run. On an unchanged build of the same Godot `master` commit, Ctrl+D gives `"default"` instead of `"one"`, and the scene saved by Save Branch as Scene lacks the value. On this fork both keep `"one"`.

## Cost

The values that were skipped are now set the normal way. Each of them costs one normal property assignment during the duplication, as any other property does. No other work is added.

## Tests

`tests/scene/test_node.cpp`, case "Duplicating node keeps PROPERTY_USAGE_ALWAYS_DUPLICATE properties". It checks an int, a String and a Resource on the node itself and on a child of a duplicated parent. The Resource must be a separate copy. Without the fix, the int and the String fail.
