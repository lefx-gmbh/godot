# New resources get new IDs after a save

Branch: [`fix/unique-id-reseed`](https://github.com/lefx-gmbh/godot/tree/fix/unique-id-reseed), on top of Godot `master`. It is in `fixes/master` and `fixes/4.7`.

## Problem

In a scene, every built-in resource has an ID such as `BoxMesh_heep2`. Since Godot 4.5, the editor can give two new resources the same ID. Then it reports `Another resource is loaded from path '::heep2' (possible cyclic resource inclusion)`. It happens more often after the editor has saved something, for example its settings.

## Cause and fix

A save seeds the ID generator from the file path, so that a file gets the same IDs each time it is saved. Importers rely on that. But the seed stayed in place after the save. The next resource created anywhere continued from that fixed point. Every later save to the same path rewound the generator to the same point again, so the next new resource got the same ID again.

`ResourceSaver::save()` now keeps a copy of the complete generator before the format saver runs, and restores it when the saver returns. After an outer save, the generator is unseeded or random again, so new IDs are random, as they were before 4.5. Inside a file, the IDs stay deterministic as before. This also applies when a resource getter saves another resource during a save: the inner save gives the outer save its sequence back. Files: `core/io/resource_saver.cpp`, `core/io/resource.cpp`, `core/io/resource.h`.

A caller that runs a format saver directly, without `ResourceSaver::save()`, still leaves the seed in place, as on an unchanged build.

## Measured

- [#112332](https://github.com/godotengine/godot/issues/112332): a script saves a resource three times to one `.tres` path and three times to one `.res` path, and calls `Resource.generate_scene_unique_id()` after each save. On an unchanged build of the same Godot `master` commit, it gets one ID three times per path. On this fork all six IDs differ.
- A save from a resource getter during another save. A script saves a resource five times to one `.res` path. A getter of that resource saves a second resource. On the unchanged build and on this fork, the sub-resource of the first resource gets the same ID from the second save on.

We did not rebuild the editor error itself. The editor retries an ID while its path is taken, so the error needs a narrow timing window.

## Cost

One copy of the generator state before each save, and one copy back after it. The state is a few bytes.

## Tests

`tests/core/io/test_resource.cpp`, case "Scene unique IDs and saving". It checks that IDs stay fresh after saves to the same `.tres` and `.res` path, and that saving the same content twice writes the same sub-resource IDs. It also saves a resource whose getter saves another resource, to a `.tres` and a `.res` path. The sub-resource IDs of the outer file must stay the same, and later IDs must stay fresh. Each check requires that the save succeeds. Without the fix the first check fails.
