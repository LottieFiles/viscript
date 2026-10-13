import assert from "node:assert/strict";
import {
    highlightSourceLines,
    shikiLanguage,
    shikiTheme,
} from "../webview/syntax-highlighter.js";

assert.equal(shikiLanguage("cpp"), "cpp");
assert.equal(shikiLanguage("typescriptreact"), "tsx");
assert.equal(shikiLanguage("unknown-language"), "text");
assert.equal(shikiTheme(new Set(["vscode-light"])), "light-plus");
assert.equal(shikiTheme(new Set(["vscode-dark"])), "dark-plus");
assert.equal(shikiTheme(new Set(["vscode-high-contrast"])), "github-dark-high-contrast");
assert.equal(
    shikiTheme(new Set(["vscode-high-contrast-light"])),
    "github-light-high-contrast",
);

const highlighted = await highlightSourceLines(
    ["local value = 42", "return value"],
    "lua",
    "dark-plus",
);
assert.equal(highlighted.length, 2);
assert.equal(highlighted[0].map((token) => token.content).join(""), "local value = 42");
assert.ok(highlighted[0].some((token) => token.color));
assert.equal(highlighted[1].map((token) => token.content).join(""), "return value");

const plain = await highlightSourceLines(["plain <text>"], "unknown-language", "light-plus");
assert.equal(plain[0].map((token) => token.content).join(""), "plain <text>");

console.log("syntax highlighter passed");
