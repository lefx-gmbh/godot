# Sub-resource IDs stay the same from the first save on

Branch: [`fix/sub-id-stable`](https://github.com/lefx-gmbh/godot/tree/fix/sub-id-stable), on top of [`fix/unique-id-reseed`](fix-unique-id-reseed.md). It is in `fixes/master` and `fixes/4.7`.

## Problem

You create a resource file (`.tres`) that uses a script class and holds built-in sub-resources, then save it again. The second save changes the IDs of the sub-resources, although nothing changed. After that they stay stable. Every new resource file therefore shows one extra change in version control.

## Cause and fix

The text saver takes IDs from a generator seeded with the file path, so that a file gets the same IDs each time. On the first save it also needs IDs for the external resources it refers to, such as the scripts, and draws those first. On later saves those IDs come from a cache. So the sub-resources got different draws on the first save than on all later saves.

The saver now seeds the generator from the path again, right before it creates sub-resource IDs. They no longer depend on how many external IDs that save needed. File: `scene/resources/resource_format_text.cpp`.

## Measured

- [#120131](https://github.com/godotengine/godot/issues/120131), with the `Fruit` and `Basket` classes from the report, saved three times in separate runs. On an unchanged build of the same Godot `master` commit, the ID goes `Resource_u555v`, then `Resource_fhklh`, then `Resource_fhklh`. On this fork it is `Resource_6fa2i` all three times.

## Limits

A file saved before this fix has the IDs of its first save. Its next save moves them once to the stable IDs.

## Cost

One seed call per text save.

## Tests

`tests/core/io/test_resource.cpp`, case "Scene unique IDs and saving", subcase "First save and later saves write the same sub-resource ID with an external resource present". Without the fix it fails.
