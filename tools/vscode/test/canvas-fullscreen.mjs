import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const htmlSource = fs.readFileSync(path.join(extensionRoot, "src", "previewPanel.ts"), "utf8");
const script = fs.readFileSync(path.join(extensionRoot, "media", "main.js"), "utf8");
const css = fs.readFileSync(path.join(extensionRoot, "media", "main.css"), "utf8");

assert.doesNotMatch(htmlSource, /id="canvas-fullscreen"|id="canvas-windowed"/);
assert.match(script, /classList\.toggle\("canvas-fullscreen"/);
assert.match(script, /event\.key === "Escape"/);
assert.match(css, /body\.canvas-fullscreen \.viewer\s*\{[^}]*height:\s*100vh/s);
assert.match(css, /body\.canvas-fullscreen \.viewer\s*\{[^}]*grid-template-columns:\s*minmax\(0, 1fr\)/s);
assert.match(css, /body\.canvas-fullscreen \.viewer\s*\{[^}]*grid-template-rows:\s*minmax\(0, 1fr\)/s);
assert.match(css, /body\.canvas-fullscreen \.stage-shell\s*\{[^}]*max-height:\s*none/s);
assert.match(css, /body\.canvas-fullscreen \.player\s*\{[^}]*grid-template-rows:\s*minmax\(0, 1fr\) auto auto/s);
assert.match(css, /body\.canvas-fullscreen \.player\s*\{[^}]*border-right:\s*0/s);
assert.match(css, /body\[data-layout="wide"\]\.canvas-fullscreen \.player\s*\{[^}]*grid-template-rows:\s*minmax\(0, 1fr\)/s);
assert.match(css, /body\[data-layout="wide"\]\.canvas-fullscreen \.transport\s*\{[^}]*position:\s*absolute[^}]*width:\s*0[^}]*height:\s*0[^}]*pointer-events:\s*none/s);
assert.match(css, /body\[data-layout="wide"\]\.canvas-fullscreen \.seek-row > :not\(\.player-menu\)[\s\S]*\.player-menu > summary\s*\{[^}]*display:\s*none/s);
assert.match(css, /body\[data-layout="wide"\]\.canvas-fullscreen \.player-menu > \.menu-panel\s*\{[^}]*pointer-events:\s*auto/s);
assert.doesNotMatch(css, /body\[data-layout="wide"\]\.canvas-fullscreen \.transport\s*\{[^}]*display:\s*none/s);
assert.match(css, /body\.canvas-fullscreen \.document-body\s*\{[^}]*display:\s*none/s);
assert.match(css, /body\.canvas-fullscreen \.wide-resizer\s*\{[^}]*display:\s*none/s);
assert.match(css, /body\.canvas-fullscreen \.wide-series-slot\s*\{[^}]*display:\s*none/s);
assert.doesNotMatch(htmlSource, /id="source-links"/);

console.log("canvas fullscreen layout passed");
