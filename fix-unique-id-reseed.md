# New resources get new IDs after a save

Branch: [`fix/unique-id-reseed`](https://github.com/lefx-gmbh/godot/tree/fix/unique-id-reseed), on top of Godot `master`. It is in `fixes/master`.

## Problem

In a scene, every built-in resource has an ID such as `BoxMesh_heep2`. Since Godot 4.5, the editor can give two new resources the same ID. Then it reports `Another resource is loaded from path '::heep2' (possible cyclic resource inclusion)`. It happens more often after the editor has saved something, for example its settings.

## Cause and fix

A save seeds the ID generator from the file path, so that a file gets the same IDs each time it is saved. Importers rely on that. But the seed stayed in place after the save. The next resource created anywhere continued from that fixed point. Every later save to the same path rewound the generator to the same point again, so the next new resource got the same ID again.

`ResourceSaver::save()` now unseeds the generator after the format saver returns. Inside a file, the IDs stay deterministic as before. Outside a save, IDs are random again, as they were before 4.5. File: `core/io/resource_saver.cpp`.

## Measured

- [#112332](https://github.com/godotengine/godot/issues/112332): a script saves a resource three times to one `.tres` path and three times to one `.res` path, and calls `Resource.generate_scene_unique_id()` after each save. On an unchanged build of the same Godot `master` commit, it gets one ID three times per path. On this fork all six IDs differ.

We did not rebuild the editor error itself. The editor retries an ID while its path is taken, so the error needs a narrow timing window.

## Cost

One call that resets a seed, once per save.

## Tests

`tests/core/io/test_resource.cpp`, case "Scene unique IDs and saving". It checks that IDs stay fresh after saves to the same `.tres` and `.res` path, and that saving the same content twice writes the same sub-resource IDs. Without the fix the first check fails.
