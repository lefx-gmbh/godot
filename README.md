# LEFX Godot Fixes Fork

This repository is a fork of the [Godot Engine](https://github.com/godotengine/godot) that holds bug fixes. Each fix removes one cause that is behind a group of open Godot issues. The fork follows Godot `master` and the current stable release, and adds only these fixes.

This fork is not official. The Godot Foundation and the Godot maintainers do not make it, review it or support it. For the engine itself, use [godotengine/godot](https://github.com/godotengine/godot).

## AI use

AI substantially contributes to most fixes/changes in this fork. We use several AI agents and models, from frontier providers and on our own hardware. They do the analysis, write the fixes and write the tests. Other agents and models then review that work and check it a second time.

A person at LEFX chooses each problem, directs the work, draws up the concepts and makes the decisions. Afterwards we review the code and check the results in the editor.

Every fix comes with tests, and every claim below has a measurement behind it. Read the code and the tests before you rely on a fix.

## Not for upstream

The Godot [contribution rules](https://contributing.godotengine.org/en/latest/development/contribution_rules.html) do not accept AI-written code. We respect that rule. For that reason:

- Do not open pull requests to Godot with code from this fork.
- Do not report a bug to Godot that comes from a build of this fork. First do a check with an official build.

Discussion about this fork belongs here, not on the Godot issue tracker.

## Branches

| Branch | Content | Status |
| --- | --- | --- |
| `fixes/master` | Godot `master` with all fixes | Available |
| `fixes/4.7` | Godot `4.7.x-stable` with all fixes | Planned |
| `fix/<topic>` | One fix, on top of Godot `master` | Available |
| `master`, `4.7` | Unchanged copies of Godot `master` and `4.7` | Available |

Branches without a prefix hold Godot as it is. Our releases have tags such as `4.7.2-stable-fixes.1`. We do not change the Godot tags, such as `4.7.2-stable`.

We bring the `fixes/*` branches up to date with Godot about once a week, and after each stable release. A fix has a place on a branch only when its tests pass there.

## Fixes

### Inherited scenes keep up with their base scene

Branches: [`fix/stale-base-scene-state`](https://github.com/lefx-gmbh/godot/tree/fix/stale-base-scene-state) and [`fix/editor-reload-base-chain`](https://github.com/lefx-gmbh/godot/tree/fix/editor-reload-base-chain)

Godot issue: [#7984](https://github.com/godotengine/godot/issues/7984), open since 2016.

When you change a base scene, the scenes that inherit from it can lose the change, or show an old version. There are two causes:

1. An inherited scene reads its base scene live during a save. If the base scene changed before that save, the inherited scene stores the wrong data. The fix keeps a copy of the base state that the inherited scene knows. Files: `scene/resources/packed_scene.{cpp,h}`.
2. The editor looks only at the direct base scene to find scenes that need a reload. A change two or more levels down the chain does not cause a reload. The fix examines the full chain. Files: `editor/editor_data.{cpp,h}`.

We rebuilt these Godot issues on Godot `master` (4.8 in development). Each one fails on an unchanged build of the same commit and passes on this fork:

- [#41492](https://github.com/godotengine/godot/issues/41492)
- [#43032](https://github.com/godotengine/godot/issues/43032)
- [#57089](https://github.com/godotengine/godot/issues/57089)
- [#28090](https://github.com/godotengine/godot/issues/28090)

Other reports describe the same symptoms. We did not rebuild them, so we make no claim about them.

## Build

Use the official [compiling instructions](https://docs.godotengine.org/en/latest/contributing/development/compiling/index.html). Before you build, check out the branch you want:

    git clone https://github.com/lefx-gmbh/godot.git
    cd godot
    git checkout fixes/master

## Report a problem

We will open the issue tracker on this repository soon. When it is open, tell us the branch, the commit and the steps that cause the problem. If you can, attach a small project that shows it.

## License

Godot is available under the MIT license. See [LICENSE.txt](LICENSE.txt) and [COPYRIGHT.txt](COPYRIGHT.txt). The changes in this fork use the same license. LEFX is not affiliated with or endorsed by the Godot Foundation. LEFX uses the GODOT® name under a permissive license, only to say which engine this fork changes. This fork does not use the Godot logo.
