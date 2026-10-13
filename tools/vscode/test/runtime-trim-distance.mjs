import assert from "node:assert/strict";
import fs from "node:fs";
import {createVis, vis} from "../runtime/client.js";

const wasmBinary = fs.readFileSync(new URL("../runtime/vis-wasm.wasm", import.meta.url));
const font = fs.readFileSync(new URL("../runtime/Pretendard.ttf", import.meta.url));
const config = {width: 200, height: 100, fps: 10, camera: {view: "2d", height: 10}};
const authored = vis.scene(config);
const line = authored.line({from: [-4, 0], to: [4, 0], trimDistance: [1, 2], id: "line"});
authored.arrow({from: [-4, 2], to: [4, 2], trimDistance: 1, id: "arrow"});
const vector = authored.vector({origin: [-4, -2], value: [8, 0], trimDistance: [2, 1], id: "vector"});
authored.brace({from: [-4, 4], to: [4, 4], offset: 0.5, depth: 0.5, size: 20, id: "brace"});
authored.set({target: vector, trimDistance: 1});
const tracker = authored.tracker(0);
authored.bind(line, tracker, (t) => ({to: [4, 6 * t], trimDistance: [1 + t, 2 - t]}));
authored.track(tracker, 1, {duration: 1, curve: "linear"});
authored.play({target: line, trimDistance: 0.5}, {duration: 1, curve: "linear"});

const source = `
local s = vis.scene {width = 200, height = 100, fps = 10, camera = {view = "2d", height = 10}}
local line = s:line {from = {-4, 0}, to = {4, 0}, trim_distance = {1, 2}, id = "line"}
s:arrow {from = {-4, 2}, to = {4, 2}, trim_distance = 1, id = "arrow"}
local vector = s:vector {origin = {-4, -2}, value = {8, 0}, trim_distance = {2, 1}, id = "vector"}
s:brace {from = {-4, 4}, to = {4, 4}, offset = 0.5, depth = 0.5, size = 20, id = "brace"}
local bounds = s:bounds(line)
assert(bounds.left == -3 and bounds.right == 2)
s:set {target = vector, trim_distance = 1}
local tracker = s:tracker(0)
s:bind(line, tracker, function(t) return {to = {4, 6 * t}, trim_distance = {1 + t, 2 - t}} end)
s:track(tracker, 1, {duration = 1, curve = "linear"})
s:play({target = line, trim_distance = 0.5}, {duration = 1, curve = "linear"})
return s
`;
const runtimes = [];
try {
    for (const language of ["lua", "js"]) {
        const runtime = await createVis("return vis.scene {}", "bootstrap.lua", {wasmBinary});
        runtimes.push(runtime);
        runtime.font("Pretendard", font, "ttf");
        if (language === "lua") runtime.loadLua(source, "trim-distance.lua");
        else runtime.loadScene(authored, "trim-distance.js");
        assert.equal(runtime.duration, 2);
    }
    for (const time of [0, 0.5, 1, 1.5, 2]) {
        assert.deepEqual(runtimes[0].render(time, true), runtimes[1].render(time, true));
        for (const id of ["line", "arrow", "vector", "brace"]) {
            assert.deepEqual(runtimes[0].bounds(id, time), runtimes[1].bounds(id, time));
        }
    }
    for (const body of [
        "s:line {from={0,0}, to={1,0}, buff=1}",
        "s:arrow {from={0,0}, to={1,0}, buff=1}",
        "s:vector {value={1,0}, buff=1}",
        "s:brace {from={0,0}, to={1,0}, buff=1}",
        "s:rectangle {size={1,1}, buff=1}",
        "local l=s:line {from={0,0}, to={1,0}} s:play({target=l, buff=1}, 1)",
        "local l=s:line {from={0,0}, to={1,0}} local t=s:tracker(0) s:bind(l,t,function(v) return {buff=v} end)",
    ]) {
        assert.throws(() => runtimes[0].loadLua(`local s=vis.scene{} ${body} return s`),
            /unknown option 'buff'|a bound state cannot set 'buff'/);
    }
} finally {
    for (const runtime of runtimes) runtime.destroy();
}

console.log("WASM endpoint trimming, Brace offset and Lua/JS parity passed");
