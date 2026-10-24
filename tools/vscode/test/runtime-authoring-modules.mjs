import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis, vis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const codeFont = fs.readFileSync(path.resolve(extensionRoot, "../../src/examples/assets/JetBrainsMono.ttf"));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));
const font = fs.readFileSync(path.join(extensionRoot, "runtime", "Pretendard.ttf"));
const diagramTemplatesRoot = path.join(
    extensionRoot, "..", "skill", "vis-skills", "templates", "diagram",
);
const diagramTemplates = [
    {file: "native-diagram.lua", root: "request-flow:root"},
    {
        file: "causal-loop.lua", root: "process-lifecycle:root",
        required: [
            "process-lifecycle:header-rule",
            "process-lifecycle:state:waiting:body",
            "process-lifecycle:transition:running-ready:route",
            "process-lifecycle:transition:waiting-ready:route",
        ],
        initiallyHidden: ["process-lifecycle:transition:waiting-ready:label"],
    },
    {
        file: "swimlane.lua", root: "release-handoff:root",
        required: [
            "release-handoff:header-rule",
            "release-handoff:handoff:submit-triage:route",
            "release-handoff:handoff:deploy-notify:route",
        ],
    },
].map((template) => ({
    ...template,
    source: fs.readFileSync(path.join(diagramTemplatesRoot, template.file), "utf8"),
}));
const nativeDiagram = diagramTemplates[0].source;

const runtime = await createVis({wasmBinary});

try {
    runtime.font("Pretendard", font, "ttf");
    runtime.font("JetBrainsMono", codeFont, "ttf");
    runtime.load(`
local scene = vis.scene {
    width = 320, height = 180, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 10},
}

local diagram = vis.diagram(scene, {
    id = "wasm-diagram", direction = "lr", origin = {-3.2, 2.2},
    node_size = {2, 0.8}, rank_gap = 1,
})
local input = diagram:node {id = "input", label = "Input"}
local output = diagram:node {id = "output", label = "Output"}
local edge = diagram:connect {id = "flow", from = input, to = output, flow = true}
local built_diagram = diagram:build()
built_diagram:body(input):label {
    text = "child", point = {-3.2, 2.2}, size = 12,
    role = "text", fill = "focus", id = "wasm-object-label",
}

scene:tween({target = built_diagram:object(edge), dash_offset = -9}, {duration = 0.4, curve = "linear"})
return scene
`, "runtime-authoring-modules.lua");

    assert.ok(runtime.duration > 0);
    const settled = runtime.duration;
    assert.ok(runtime.bounds("wasm-diagram:root", settled).width > 0);
    assert.ok(runtime.bounds("wasm-diagram:edge:flow", settled).width > 0);
    assert.ok(runtime.bounds("wasm-object-label", settled).width > 0);
    const pixels = runtime.render(settled);
    assert.equal(pixels.length, 320 * 180 * 4);
    assert.ok(pixels.some((value, index) => index % 4 !== 3 && value !== 255));
    for (const template of diagramTemplates) {
        runtime.load(template.source, `diagram-template-${template.file}`);
        const templateTime = runtime.duration;
        assert.ok(
            runtime.bounds(template.root, templateTime).width > 0,
            `${template.file} should expose its semantic root`,
        );
        for (const objectId of template.required ?? []) {
            const bounds = runtime.bounds(objectId, templateTime);
            assert.ok(
                bounds.width > 0 && bounds.height > 0,
                `${template.file} should retain required grammar object ${objectId}`,
            );
        }
        for (const objectId of template.initiallyHidden ?? []) {
            assert.throws(
                () => runtime.bounds(objectId, 0),
                /InsufficientCondition/,
                `${template.file} should not reveal ${objectId} before its evidence mark`,
            );
            assert.ok(
                runtime.bounds(objectId, templateTime).width > 0,
                `${template.file} should reveal ${objectId} by the settled frame`,
            );
        }
        assert.equal(
            runtime.render(templateTime).length,
            960 * 540 * 4,
            `${template.file} should render a complete 960x540 frame`,
        );
    }
    for (let index = 0; index < 256; index += 1) {
        runtime.load(nativeDiagram, `native-diagram-reload-${index}.lua`);
    }
} finally {
    runtime.destroy();
}

const definition = vis.scene({
    width: 160,
    height: 90,
    camera: {mode: "fixed", view: "2d", target: [0, 0], height: 4},
});
const subject = definition.rectangle({center: [0, 0], size: [2, 1], id: "js-label-parent"});
subject.label({
    text: "Object label",
    point: [0, 0],
    size: 14,
    role: "text",
    fill: "focus",
    id: "js-object-label",
});
const jsRuntime = await createVis(definition, "runtime-object-label.js", {wasmBinary});
try {
    jsRuntime.font("Pretendard", font, "ttf");
    jsRuntime.font("JetBrainsMono", codeFont, "ttf");
    assert.ok(jsRuntime.bounds("js-label-parent", 0).width > 0);
    assert.ok(jsRuntime.bounds("js-object-label", 0).width > 0);
} finally {
    jsRuntime.destroy();
}

console.log("runtime authoring modules, diagram family templates, edge growth, and Object labels passed");
