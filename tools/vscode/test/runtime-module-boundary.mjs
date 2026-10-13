import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const expectedUi = process.env.VIS_EXPECT_UI !== "0";
const expectedInput = process.env.VIS_EXPECT_INPUT !== "0";
const expectedLuaRuntime = process.env.VIS_EXPECT_LUA_RUNTIME !== "0";
const expectedMotion = process.env.VIS_EXPECT_MOTION !== "0";
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const source = `
local expected_ui = ${expectedUi}
local expected_input = ${expectedInput}
local expected_lua_runtime = ${expectedLuaRuntime}
local expected_motion = ${expectedMotion}
if (vis.ui ~= nil) ~= expected_ui then error("unexpected UI namespace") end
if (vis.input ~= nil) ~= expected_input then error("unexpected Input namespace") end
if (vis.runtime ~= nil) ~= expected_lua_runtime then error("unexpected Lua Runtime namespace") end
if (vis.motion ~= nil) ~= expected_motion then error("unexpected Motion namespace") end
return vis.scene {
    width = 32,
    height = 18,
    camera = {mode = "interactive", view = "2d", target = {0, 0}, height = 4},
}
`;

const runtime = await createVis(source, "runtime-module-boundary.lua", {wasmBinary});
try {
    assert.equal(typeof runtime.input === "function", expectedUi || expectedInput);
    for (const name of ["beginInputFrame", "releaseInput", "keyState", "pointerState", "actionMap"]) {
        assert.equal(typeof runtime[name] === "function", expectedInput);
    }
    assert.equal(typeof runtime.advanceRuntime === "function", expectedLuaRuntime);
    assert.equal(runtime.retainedLua, false);
    assert.equal(runtime.runtimeTime, 0);
    assert.equal(runtime.cameraInput("pan", 0.01, 0), expectedUi || expectedInput);
    assert.doesNotThrow(() => runtime.render(0));
} finally {
    runtime.destroy();
}

console.log(`runtime module boundary passed (ui=${expectedUi}, input=${expectedInput}, luaRuntime=${expectedLuaRuntime}, motion=${expectedMotion})`);
