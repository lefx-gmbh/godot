# A post-import script's changes reach animations saved to file

Branch: [`fix/post-import-animation-save`](https://github.com/lefx-gmbh/godot/tree/fix/post-import-animation-save), on top of Godot `master`. It is in `fixes/master`.

## Problem

An imported model has an animation set to **Save to File**, and a post-import script (`EditorScenePostImport`) changes that animation, for example by renaming track paths. The editor shows the change. After a reload of the project it is gone, and the running game never had it.

## Cause and fix

The importer wrote the animation file while it was still processing the scene, before the post-import script and the post-import plugins ran. The script then changed the animation in memory only. The scene refers to the file, so it loads the old version.

The importer now queues these saves and writes them after the script and the plugins, right before the scene itself is saved. Each file is still written once, and "Keep Custom Tracks" works as before. The queue belongs to one import. When an import stops early, for example because the script returns no scene, the importer releases the queue and its animations. File: `editor/import/3d/resource_importer_scene.cpp`.

## Measured

- [#85738](https://github.com/godotengine/godot/issues/85738), in a headless `--import`: a glTF animation saved to `walk.res`, and a post-import script that renames track 0 to `Modified`. On an unchanged build of the same Godot `master` commit, `walk.res` and the imported scene keep the old track path `Box`. On this fork both have `Modified`. Without the script, both builds write `Box`.
- A post-import script that frees the scene and returns `null`. Ten seconds later, the editor no longer holds the animation, on the unchanged build and on this fork.

## Limits

If a post-import script puts a new animation object in place of the imported one, the file gets the imported object that was queued. We did not measure this case.

## Cost

The same files are written, only later in the import. Until then, the import keeps a reference to each animation that it saves to a file.

## Tests

None automated. The import needs the editor's file system, which the unit tests do not start. To check it by hand: set an imported animation to **Save to File**, add a post-import script that changes it, reimport, and reload the project. The change stays on the fork.
