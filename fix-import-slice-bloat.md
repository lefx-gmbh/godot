# `.import` files of 3D scenes stay small

Branch: [`fix/import-slice-bloat`](https://github.com/lefx-gmbh/godot/tree/fix/import-slice-bloat), on top of Godot `master`. It is in `fixes/master`.

## Problem

You import a glTF with animations and change one option of one animation, for example the loop mode. The `.import` file then gains 256 empty slices, 8 lines each, for that animation. With a few dozen animations the file grows to megabytes, and every reimport shows up in version control.

## Cause and fix

Before the scene importer reads an animation's settings, it fills in every option that is not set with its default, the 256 slices included. The dictionaries it filled were the ones in the import options themselves, not copies. The editor writes those options to the `.import` file after the import, so every default ended up in the file. The advanced import dialog itself stores only the option you change.

The importer now takes one deep copy of the animation settings before it uses them. This covers the animation options only. The material and mesh options are filled the same way; the material code also stores fallback paths on purpose through the same dictionaries, so they need their own fix. Everything inside the import works exactly as before, on the copy. The intended changes to the options still happen on the original, earlier in the import: save paths are turned into UIDs there. File: `editor/import/3d/resource_importer_scene.cpp`.

## Measured

- [#68936](https://github.com/godotengine/godot/issues/68936): a generated glTF with one animation, the loop mode set in `_subresources`, then a headless `--import`. On an unchanged build of the same Godot `master` commit, the `.import` file goes from 41 to 2100 lines, 2048 of them slice entries. On this fork it has 47 lines and no slice entries. On both builds the imported animation has the loop mode set.

## Limits

Only animation options. Defaults of material and mesh options can still reach the `.import` file once one of their options is set.

A post-import plugin that changes animation options in `_internal_process()` no longer has those changes saved, because it now works on the copy.

A `.import` file that is already bloated stays as it is. Its extra entries equal the defaults, so they change nothing, and you can delete them by hand.

## Cost

One deep copy of the animation settings per import. Its time and temporary memory grow with the size of those settings. In the measured case, the `.import` file has 2048 fewer slice entries. We did not measure the import time or the peak memory.

## Tests

None automated. The import needs the editor's file system, which the unit tests do not start. To check it by hand: import a glTF with an animation, change its loop mode in the advanced import dialog, and count the `slice_` lines in its `.import` file. The fork writes none.
