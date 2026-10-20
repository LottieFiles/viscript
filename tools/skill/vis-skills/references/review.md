# Probes and hosts

The review round, the audit and a contact sheet in one command, is in [SKILL.md](../SKILL.md#review-in-rounds). This page adds probes for a number in question and the checks for particular hosts.

`inspect` prints the duration, the objects, and the marks as JSON; run it when an ID or a time is in question. `layout --time T --padding 4` prints every object's bounds as JSON. Run it only when a number decides a fix, such as a gap or a containment margin, and filter it for the IDs in question instead of reading the whole report. A successful command is not proof of a correct frame; a visible object with null bounds needs a look. `viscript audit --help` lists the audit's other allowances.

Renders, probes, variant copies, and helper scripts go in `$REVIEW_DIR`, a scratch directory outside the user's project or vault (the session scratchpad, or one from `mktemp -d`), never beside the Scene: an Obsidian vault lists every image and `.lua` file in it, and `.vscode/vis` holds only Scenes and their assets. The delivered folder ends with the Scenes and nothing else.

A Scene for the VS Code Viewer also gets the vis-vscode host review: the Viewer sets headings and body text in Source Serif 4 and takes the editor's colors. For JS/WASM, write the Lua from `compileScene` to a file and review that file.
