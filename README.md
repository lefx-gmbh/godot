# LEFX Godot Fixes Fork

This repository is a fork of the [Godot Engine](https://github.com/godotengine/godot) that holds bug fixes. Each fix removes one cause that is behind a group of open Godot issues. The fork follows Godot `master` and the current stable release, and adds only these fixes.

This fork is not official. The Godot Foundation and the Godot maintainers do not make it, review it or support it. For the engine itself, use [godotengine/godot](https://github.com/godotengine/godot).

## AI use

AI substantially contributes to most fixes/changes in this fork. We use several AI agents and models, from frontier providers and on our own hardware. They do the analysis, write the fixes and write the tests. Other agents and models then review that work and check it a second time.

A person at LEFX chooses each problem, directs the work, draws up the concepts and makes the decisions. Afterwards we review the code and check the results in the editor.

Every fix comes with tests, or with steps to check it by hand where the code needs the running editor. Every claim below has a measurement behind it. Read the code and the tests before you rely on a fix.

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
| `fix/<topic>` | One fix, on top of Godot `master` or of the fix branch it needs. Its page says which. | Available |
| `master`, `4.7` | Unchanged copies of Godot `master` and `4.7` | Available |

Branches without a prefix hold Godot as it is. Our releases have tags such as `4.7.2-stable-fixes.1`. We do not change the Godot tags, such as `4.7.2-stable`.

We bring the `fixes/*` branches up to date with Godot about once a week, and after each stable release. A fix has a place on a branch only when its tests pass there.

## Fixes

Each fix has a page with the problem, the cause, the Godot issues we measured, and its limits.

| Fix | Godot issues measured | Branch |
| --- | --- | --- |
| [Inherited scenes keep up with their base scene](fix-inherited-scenes.md) | #41492, #43032, #57089, #28090, #94912 | `fix/stale-base-scene-state`, `fix/editor-reload-base-chain` |
| [A cleared Node reference stays cleared](fix-clear-node-reference.md) | #92879 | `fix/clear-node-reference` |
| [An exported value survives a change of its type](fix-exported-type-change.md) | #46103 | `fix/exported-type-change` |
| [Node references survive Save Branch as Scene](fix-save-branch-references.md) | #84016 | `fix/save-branch-references` |

We list a Godot issue only when we rebuilt it and it fails on an unchanged build of the same commit. Other reports can describe the same symptoms. We make no claim about them.

## Build

Use the official [compiling instructions](https://docs.godotengine.org/en/latest/contributing/development/compiling/index.html). Before you build, check out the branch you want:

    git clone https://github.com/lefx-gmbh/godot.git
    cd godot
    git checkout fixes/master

## Report a problem

We will open the issue tracker on this repository soon. When it is open, tell us the branch, the commit and the steps that cause the problem. If you can, attach a small project that shows it.

## License

Godot is available under the MIT license. See [LICENSE.txt](LICENSE.txt) and [COPYRIGHT.txt](COPYRIGHT.txt). The changes in this fork use the same license. LEFX is not affiliated with or endorsed by the Godot Foundation. LEFX uses the GODOT® name under a permissive license, only to say which engine this fork changes. This fork does not use the Godot logo.
