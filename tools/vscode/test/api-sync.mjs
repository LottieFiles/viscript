import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const api = JSON.parse(fs.readFileSync(path.join(extensionRoot, "api", "vis-api.json"), "utf8"));
const binding = ["visLua.cpp", "visLuaApi.cpp", "visLuaValues.h", "visLuaStyle.h"]
    .map((name) => fs.readFileSync(path.join(repositoryRoot, "src", "core", "lua", name), "utf8"))
    .join("\n");
const uiBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "ui", "visUiLua.cpp"), "utf8");
const inputBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "input", "visInputLua.cpp"), "utf8");
const runtimeBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "lua_runtime", "visLuaRuntime.cpp"), "utf8");
const statesBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "states", "visStatesLua.cpp"), "utf8");
const diagramBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "diagram", "visDiagramLua.cpp"), "utf8");
const texBinding = fs.readFileSync(path.join(repositoryRoot, "src", "core", "tex", "visTexLua.cpp"), "utf8");

const sharedFactories = registrationNames(binding, "FACTORY_METHODS");
const sceneMethods = [...sharedFactories, ...registrationNames(binding, "SCENE_METHODS")].sort();
const objectMethods = [...sharedFactories, ...registrationNames(binding, "OBJECT_METHODS")].sort();
const groupMethods = registrationNames(binding, "GROUP_METHODS");
const viewMethods = registrationNames(binding, "VIEW_METHODS");
const panelMethods = registrationNames(uiBinding, "methods");
const inputMethods = registrationNames(inputBinding, "methods");
const runtimeMethods = registrationNames(runtimeBinding, "methods");
const statesMethods = registrationNames(statesBinding, "methods");
const diagramMethods = registrationNames(diagramBinding, "diagramMethods");
const diagramBuiltMethods = registrationNames(diagramBinding, "builtMethods");
// A module that adds methods to the handles marks them with its name; the core binding lacks them.
const factories = Object.keys(api.factories).filter((name) => !api.factories[name].module);
const coreGroupMethods = Object.keys(api.methods.Group).filter((name) => !api.methods.Group[name].module);

assert.deepEqual(sceneMethods, [...factories, ...Object.keys(api.methods.Scene)].sort(), "Scene API schema differs from SCENE_METHODS");
assert.deepEqual(objectMethods, [...factories, ...Object.keys(api.methods.Object)].sort(), "Object API schema differs from OBJECT_METHODS");
assert.deepEqual(coreGroupMethods.sort(), groupMethods, "Group API schema differs from GROUP_METHODS");
assert.deepEqual(Object.keys(api.methods.View).sort(), viewMethods, "View API schema differs from VIEW_METHODS");
assert.equal(api.methods.Scene.view.returns, "View");
assert.deepEqual(Object.keys(api.methods.Space).sort(), ["cell", "labels", "voxel"], "Space-only methods changed");
assert.deepEqual(Object.keys(api.methods.Panel).sort(), panelMethods, "Panel API schema differs from the optional UI binding");
assert.equal(api.namespaces.ui.type, "UI");
assert.equal(api.namespaces.ui.functions.panel.returns, "Panel");
assert.deepEqual(api.namespaces.ui.functions.panel.parameters, [{name: "scene", type: "Scene"}]);
assert.deepEqual(Object.keys(api.methods.InputController).sort(), inputMethods,
    "InputController API schema differs from the optional Input binding");
assert.equal(api.namespaces.input.type, "Input");
assert.equal(api.namespaces.input.functions.controller.returns, "InputController");
assert.deepEqual(api.namespaces.input.functions.controller.parameters, [{name: "scene", type: "Scene"}]);
assert.deepEqual(Object.keys(api.methods.Runtime).sort(), runtimeMethods,
    "Runtime API schema differs from the optional retained Lua binding");
assert.deepEqual(api.globals.runtime.parameters,
    [{name: "scene", type: "Scene"}, {name: "config", type: "RuntimeConfig"}]);
assert.equal(api.globals.runtime.returns, "Runtime");
assert.deepEqual(Object.keys(api.methods.StatesController).sort(), statesMethods,
    "StatesController API schema differs from the optional States binding");
assert.match(statesBinding, /lua_setfield\(lua, -2, "states"\)/);
assert.deepEqual(api.globals.states.parameters, [{name: "scene", type: "Scene"}]);
assert.equal(api.globals.states.returns, "StatesController");
for (const factory of Object.values(api.factories)) assert.ok(api.configs[factory.config], `Missing ${factory.config}`);
assert.equal(api.factories.contour.returns, "Group");
assert.equal(api.configs.ContourConfig.levels.required, true);
assert.equal(api.globals.spring_response.returns, "number");
assert.deepEqual(api.globals.spring_response.parameters.map((parameter) => parameter.type), ["number", "SpringResponseConfig"]);
for (const name of ["mass", "stiffness", "damping", "displacement"]) {
    assert.equal(api.configs.SpringResponseConfig[name].type, "number");
    assert.equal(api.configs.SpringResponseConfig[name].required, true);
}
assert.equal(api.configs.SpringResponseConfig.velocity.required, undefined);
assert.equal(api.configs.ContourConfig.values.type, "number[]");
assert.equal(api.configs.ContourConfig.labels.type, "boolean|ContourLabelConfig");
assert.equal(api.configs.ContourLabelConfig.upright.type, "boolean");
for (const method of ["button", "toggle_button", "slider", "sample_area"]) {
    const config = api.methods.Panel[method].parameters[0]?.type;
    assert.ok(api.configs[config], `Missing Panel method config ${config}`);
}
for (const config of ["RegionConfig", "UITransformConfig", "SliderBindingConfig", "PanelButtonConfig", "PanelToggleButtonConfig", "PanelSliderConfig", "PanelSampleAreaConfig"]) {
    assert.ok(api.configs[config], `Missing experimental UI config ${config}`);
}
for (const method of ["pointer_follow", "key_move"]) {
    const config = api.methods.InputController[method].parameters[0]?.type;
    assert.ok(api.configs[config], `Missing InputController method config ${config}`);
}
assert.equal(api.configs.PanelButtonConfig.target.type, "Object");
assert.equal(api.configs.PanelButtonConfig.transform.type, "UITransformConfig");
assert.equal(api.configs.PanelButtonConfig.duration.type, "number");
assert.equal(api.configs.PanelButtonConfig.hover_visual.type, "Object");
assert.equal(api.configs.PanelButtonConfig.pressed_visual.type, "Object");
assert.equal(api.configs.PanelToggleButtonConfig.target.type, "Object");
assert.equal(api.configs.PanelToggleButtonConfig.off.type, "UITransformConfig");
assert.equal(api.configs.PanelToggleButtonConfig.on.type, "UITransformConfig");
assert.equal(api.configs.PanelToggleButtonConfig.value.type, "boolean");
assert.equal(api.configs.PanelSliderConfig.target.type, "Object");
assert.equal(api.configs.PanelSliderConfig.bindings.type, "SliderBindingConfig[]");
assert.equal(api.configs.SliderBindingConfig.target.required, true);
assert.equal(api.configs.PanelSampleAreaConfig.region.type, "RegionConfig");
assert.equal(api.configs.PanelSampleAreaConfig.targets.type, "Object[]");
assert.equal(api.configs.PanelSampleAreaConfig.marker.type, "Object");
assert.equal(api.configs.PanelSampleAreaConfig.swatch.type, "Object");
assert.equal(api.methods.Panel.toggle, undefined);

assert.deepEqual(Object.keys(api.methods.Diagram).sort(), diagramMethods,
    "Diagram API schema differs from the optional Diagram binding");
assert.deepEqual(Object.keys(api.methods.DiagramBuilt).sort(), diagramBuiltMethods,
    "DiagramBuilt API schema differs from the optional Diagram binding");
assert.match(diagramBinding, /lua_setfield\(lua, -2, "diagram"\)/);
assert.deepEqual(api.globals.diagram.parameters,
    [{name: "scene", type: "Scene"}, {name: "config", type: "DiagramConfig"}]);
assert.equal(api.globals.diagram.returns, "Diagram");

// The tex module adds its factory to the Scene and Object handles and part to Group handles, and
// wraps match_transform; without it, the core stops a script that calls the factory.
const texMethods = moduleMethodNames(texBinding);
const texFactories = Object.keys(api.factories).filter((name) => api.factories[name].module === "tex").sort();
assert.deepEqual(texMethods.Object, texFactories, "tex factories differ from the tex binding");
assert.deepEqual(texMethods.Scene.filter((name) => texFactories.includes(name)), texFactories,
    "tex factories differ from the tex binding");
assert.deepEqual(Object.keys(api.methods.Group).filter((name) => api.methods.Group[name].module === "tex").sort(),
    texMethods.Group, "Group API schema differs from the tex binding");
for (const name of texMethods.Scene.filter((name) => !texFactories.includes(name))) {
    assert.ok(api.methods.Scene[name], `tex wraps a Scene method the schema lacks: ${name}`);
}
assert.deepEqual(moduleFactories(binding),
    Object.entries(api.factories).filter(([, factory]) => factory.module).map(([name, factory]) => `${name}:${factory.module}`).sort(),
    "Module factories differ from MODULE_METHODS");
assertConfig("TexConfig", listNames(texBinding, "FORMULA_OPTIONS"), ["tex"]);
assertConfig("TexPartConfig", [...listNames(texBinding, "GROUP_STYLES"), ...listNames(texBinding, "PART_OPTIONS")].sort(), []);
for (const name of listNames(texBinding, "GROUP_STYLES")) {
    assert.deepEqual(api.configs.TexPartConfig[name], api.configs.ObjectConfig[name], `TexPartConfig ${name} differs from ObjectConfig`);
}
// The tex module makes the {tex = ...} parts of spans; without it, the core names the module.
assertConfig("TexSpanConfig", [...listNames(texBinding, "GROUP_STYLES"), ...listNames(texBinding, "SPAN_OPTIONS")].sort(),
    ["tex"]);
for (const name of listNames(texBinding, "GROUP_STYLES")) {
    assert.deepEqual(api.configs.TexSpanConfig[name], api.configs.ObjectConfig[name], `TexSpanConfig ${name} differs from ObjectConfig`);
}
assert.deepEqual(spanPartKeys(texBinding), moduleParts(binding), "tex spans parts differ from MODULE_PARTS");

assertConfig("DiagramConfig", optionNames(diagramBinding, "_newDiagram"), []);
assertConfig("DiagramLayersConfig", optionNames(diagramBinding, "_fieldLayers"), []);
assertConfig("DiagramNodeConfig", optionNames(diagramBinding, "_addNode"), ["id"]);
assertConfig("DiagramEdgeConfig", optionNames(diagramBinding, "_addEdge"), ["from", "id", "to"]);
assertConfig("DiagramZoneConfig", optionNames(diagramBinding, "_addZone"), ["id", "members"]);
assertConfig("PanelToggleButtonConfig", optionNames(uiBinding, "_toggleButton"),
    ["off", "on", "region", "target", "visual"]);
assertConfig("PanelSliderConfig", optionNames(uiBinding, "_slider"),
    ["region", "visual"]);
assertConfig("InputPointerFollowConfig", optionNames(inputBinding, "_pointerFollow"),
    ["map_origin", "map_x", "map_y", "region", "target"]);
assertConfig("InputKeyMoveConfig", optionNames(inputBinding, "_keyMove"),
    ["key", "shift", "target"]);
assertConfig("RuntimeConfig", optionNames(runtimeBinding, "_newRuntime"), ["update"]);
assertConfig("RuntimeObjectState", optionNames(runtimeBinding, "_objectState"), []);
assertConfig("RuntimeSoundConfig", optionNames(runtimeBinding, "_sound"), []);
assertConfig("StateOverlay", optionNames(statesBinding, "_state"), []);
assertConfig("StateTarget", optionNames(statesBinding, "_transition"), ["object", "state"]);
assert.deepEqual(api.methods.StatesController.transition.overloads[0], {
    parameters: [{name: "targets", type: "StateTarget[]"}, {name: "duration", type: "number", optional: true}, {name: "curve", type: "Curve", optional: true}],
    returns: "StatesController",
});
assert.deepEqual(api.methods.StatesController.transition.overloads.slice(1).map((overload) => overload.parameters), [
    [{name: "object", type: "Object"}, {name: "state", type: "string"}, {name: "options", type: "TimingOptions"}],
    [{name: "targets", type: "StateTarget[]"}, {name: "options", type: "TimingOptions"}],
]);
for (const [name, base] of Object.entries(api.configBases)) {
    assert.ok(api.configs[name] && api.configs[base], `Unknown timing config inheritance: ${name}`);
    for (const key of Object.keys(api.configs[base])) assert.ok(api.configs[name][key], `Missing ${name}.${key}`);
}

const meta = fs.readFileSync(path.join(extensionRoot, "api", "vis.d.lua"), "utf8");
for (const name of sceneMethods) assert.match(meta, new RegExp(`function Scene:${name}\\(`));
for (const name of groupMethods) assert.match(meta, new RegExp(`function Group:${name}\\(`));
for (const name of viewMethods) assert.match(meta, new RegExp(`function View:${name}\\(`));
assert.match(meta, /function Library\.scene\(/);
assert.match(meta, /---@field ui\? vis\.UI Experimental interactive UI module/);
assert.match(meta, /function UI\.panel\(scene\)/);
for (const name of panelMethods) assert.match(meta, new RegExp(`function Panel:${name}\\(`));
for (const config of ["RegionConfig", "UITransformConfig", "SliderBindingConfig", "PanelButtonConfig", "PanelToggleButtonConfig", "PanelSliderConfig", "PanelSampleAreaConfig"]) {
    assert.match(meta, new RegExp(`---@class vis\\.${config}`));
}
assert.match(meta, /---@field input\? vis\.Input Experimental keyboard, pointer, and camera Input module/);
assert.match(meta, /function Input\.controller\(scene\)/);
for (const name of inputMethods) assert.match(meta, new RegExp(`function InputController:${name}\\(`));
for (const config of ["InputPointerFollowConfig", "InputKeyMoveConfig"]) {
    assert.match(meta, new RegExp(`---@class vis\\.${config}`));
}
assert.match(meta, /function Library\.runtime\(scene, config\)/);
for (const name of runtimeMethods) assert.match(meta, new RegExp(`function Runtime:${name}\\(`));
for (const config of [
    "RuntimeConfig", "RuntimeObjectState", "RuntimeActionState", "RuntimeKeyState",
    "RuntimePointerState", "RuntimeSoundConfig",
]) assert.match(meta, new RegExp(`---@class vis\\.${config}`));
assert.match(meta, /function Library\.states\(scene\)/);
for (const name of statesMethods) assert.match(meta, new RegExp(`function StatesController:${name}\\(`));
assert.match(meta, /---@overload fun\(self: vis\.StatesController, targets: vis\.StateTarget\[\], duration\?: number, curve\?: vis\.Curve\): vis\.StatesController/);
for (const config of ["StateOverlay", "StateTarget", "StateCurveRecord", "StateEvent"]) {
    assert.match(meta, new RegExp(`---@class vis\\.${config}`));
}
assert.match(meta, /function Library\.diagram\(scene, config\)/);
for (const [owner, methods] of [
    ["Diagram", diagramMethods], ["DiagramBuilt", diagramBuiltMethods],
]) {
    assert.match(meta, new RegExp(`---@class vis\\.${owner}`));
    for (const name of methods) assert.match(meta, new RegExp(`function ${owner}:${name}\\(`));
}
for (const name of ["DiagramNode", "DiagramEdge", "DiagramZone"]) {
    assert.match(meta, new RegExp(`---@class vis\\.${name}`));
}
for (const config of [
    "DiagramConfig", "DiagramLayersConfig", "DiagramNodeConfig", "DiagramEdgeConfig",
    "DiagramZoneConfig",
]) assert.match(meta, new RegExp(`---@class vis\\.${config}`));

for (const owner of ["Scene", "Object", "Group", "Space"]) {
    for (const name of texFactories) assert.match(meta, new RegExp(`function ${owner}:${name}\\(config\\)`));
}
for (const name of texMethods.Group) assert.match(meta, new RegExp(`function Group:${name}\\(`));
assert.match(meta, /---@class vis\.TexConfig: vis\.ObjectConfig/);
assert.match(meta, /---@class vis\.TexPartConfig\n/);
assert.match(meta, /---@class vis\.TexSpanConfig\n/);

console.log("Lua API schema and generated LuaLS metadata are synchronized");

function registrationNames(source, constant) {
    const body = new RegExp(`static const luaL_Reg ${constant}\\[\\] = \\{([\\s\\S]*?)\\};`).exec(source)?.[1];
    assert.ok(body, `Could not find ${constant}`);
    return Array.from(body.matchAll(/\{"([^"]+)",/g), (match) => match[1]).sort();
}

// The methods a module adds to the handles, by the kind of handle.
function moduleMethodNames(source) {
    const names = {Scene: [], Object: [], Group: []};
    for (const match of source.matchAll(/luaSetMethod\(lua, core::LuaHandle::(\w+), "([^"]+)"\)/g)) names[match[1]].push(match[2]);
    for (const list of Object.values(names)) list.sort();
    return names;
}

// The factories the core stops a script for until their module adds them, as name:module.
function moduleFactories(source) {
    const body = /static constexpr LuaModuleMethod MODULE_METHODS\[\] = \{([\s\S]*?)\n\};/.exec(source)?.[1];
    assert.ok(body, "Could not find MODULE_METHODS");
    return Array.from(body.matchAll(/\{"([^"]+)", "([^"]+)"\}/g), (match) => `${match[1]}:${match[2]}`).sort();
}

// The keys of the spans parts a module gives a maker.
function spanPartKeys(source) {
    return Array.from(source.matchAll(/luaSetSpanPart\(lua, "([^"]+)"\)/g), (match) => match[1]).sort();
}

// The keys of the spans parts the core stops a script for until their module makes them.
function moduleParts(source) {
    const body = /static constexpr LuaModulePart MODULE_PARTS\[\] = \{([\s\S]*?)\n\};/.exec(source)?.[1];
    assert.ok(body, "Could not find MODULE_PARTS");
    return Array.from(body.matchAll(/\{"([^"]+)", "([^"]+)"\}/g), (match) => match[1]).sort();
}

function listNames(source, constant) {
    const body = new RegExp(`static constexpr const char\\* ${constant}\\[\\] = \\{([\\s\\S]*?)\\};`).exec(source)?.[1];
    assert.ok(body, `Could not find ${constant}`);
    return Array.from(body.matchAll(/"([^"]+)"/g), (match) => match[1]).sort();
}

function optionNames(source, functionName) {
    const escaped = functionName.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const start = new RegExp(`static [^\\n]+ ${escaped}\\(`).exec(source)?.index;
    assert.notEqual(start, undefined, `Could not find ${functionName}`);
    const end = source.indexOf("\nstatic ", start + 1);
    const body = source.slice(start, end < 0 ? source.length : end);
    const initializer = /static const char\* const options\[\]\s*=\s*\{([\s\S]*?)\};/.exec(body)?.[1];
    assert.ok(initializer, `Could not find options in ${functionName}`);
    return Array.from(initializer.matchAll(/"([^"]+)"/g), (match) => match[1]).sort();
}

function assertConfig(name, fields, required) {
    assert.ok(api.configs[name], `Missing ${name}`);
    assert.deepEqual(Object.keys(api.configs[name]).sort(), fields,
        `${name} differs from its binding options`);
    assert.deepEqual(Object.entries(api.configs[name])
        .filter(([, field]) => field.required)
        .map(([field]) => field)
        .sort(), required.sort(), `${name} required fields differ from the binding contract`);
}
