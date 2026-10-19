import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const {commands, menus} = JSON.parse(fs.readFileSync(path.join(extensionRoot, "package.json"), "utf8")).contributes;

// The Command Palette opens the Viewer one way; Open to Side serves the editor title only.
const hidden = new Set(menus.commandPalette.filter((item) => item.when === "false").map((item) => item.command));
const visible = commands.filter((command) => !hidden.has(command.command));
assert.ok(commands.some((command) => command.command === "visPreview.openToSide"));
assert.deepEqual(
    visible.map((command) => command.command).filter((name) => /^visPreview\.open(?:Viewer|ToSide)?$/.test(name)),
    ["visPreview.openViewer"],
);

console.log("viewer commands passed");
