import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {luaMeta} from "../scripts/generate-lua-meta.mjs";

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
const valuesHeader = fs.readFileSync(path.join(repositoryRoot, "src", "core", "lua", "visLuaValues.h"), "utf8");
const trackerPrelude = fs.readFileSync(path.join(repositoryRoot, "src", "core", "lua", "visLuaTracker.h"), "utf8");

const sharedFactories = registrationNames(binding, "FACTORY_METHODS");
// The tracker prelude defines Scene methods in Lua, besides those SCENE_METHODS registers.
const preludeMethods = Array.from(trackerPrelude.matchAll(/^function methods\.(\w+)\(/gm), (match) => match[1]);
const sceneMethods = [...sharedFactories, ...registrationNames(binding, "SCENE_METHODS"), ...preludeMethods].sort();
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
assert.deepEqual([...preludeMethods].sort(), ["bind", "track", "tracker"], "The tracker prelude's Scene methods changed");
assert.match(trackerPrelude, /tracker\.get = function/);
assert.deepEqual(Object.keys(api.methods.Tracker), ["get"], "Tracker API schema differs from the tracker prelude");
assert.equal(api.methods.Scene.tracker.returns, "Tracker");
// Each factory config lists the options its binding accepts: its own and, through configBases, the
// style options _options() shares, less the configBaseExcept names.
const sharedStyles = listNames(valuesHeader, "shared");
// Three factories the scene and objects register themselves, since spaces have a cell of their own.
const factoryFunctions = {...registrationTable(binding, "SCENE_METHODS"), ...registrationTable(binding, "FACTORY_METHODS")};
// Factories that check their options in a helper, or refuse some of the shared styles in one.
const OPTION_SOURCES = {
    text: ["_textObject"], code: ["_codeOptions"], spans: ["_rowOptions", "_spanOption"],
    union: ["_region"], intersection: ["_region"], difference: ["_region"], exclusion: ["_region"],
};
for (const [name, factory] of Object.entries(api.factories)) {
    if (factory.module) continue;
    const accepted = factoryOptionNames(binding, OPTION_SOURCES[name] ?? [factoryFunctions[name]], sharedStyles);
    assert.deepEqual(Object.keys(configFields(factory.config)).sort(), accepted,
        `${factory.config} differs from the options ${name} accepts`);
}
assert.equal(api.factories.contour.returns, "Group");
assert.equal(api.configs.ContourConfig.levels.required, true);
// scene:number writes 0 without a value.
assert.equal(api.configs.NumberConfig.value.required, undefined);
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
assert.deepEqual(api.methods.Panel.camera.parameters, [
    {name: "action", type: '"pan"|"orbit"|"zoom"|"reset"|"view2d"|"view3d"'},
    {name: "delta", type: "Vec2|number", optional: true},
]);
assert.equal(api.methods.Panel.camera.returns, "Panel");
assert.doesNotMatch(api.configs.PanelButtonConfig.camera.type, /"move"/);

assert.deepEqual(Object.keys(api.methods.Diagram).sort(), diagramMethods,
    "Diagram API schema differs from the optional Diagram binding");
assert.deepEqual(Object.keys(api.methods.DiagramBuilt).sort(), diagramBuiltMethods,
    "DiagramBuilt API schema differs from the optional Diagram binding");
assert.match(diagramBinding, /lua_setfield\(lua, -2, "diagram"\)/);
assert.deepEqual(api.globals.diagram.parameters,
    [{name: "scene", type: "Scene"}, {name: "config", type: "DiagramConfig"}]);
assert.equal(api.globals.diagram.returns, "Diagram");
for (const [name, type, returns] of [
    ["object", "DiagramNode|DiagramEdge|DiagramZone", "Object"],
    ["body", "DiagramNode|DiagramZone", "Object"],
    ["label", "DiagramNode|DiagramEdge|DiagramZone", "Object|nil"],
    ["detail", "DiagramNode", "Object|nil"],
]) {
    assert.equal(api.methods.DiagramBuilt[name].parameters[0].type, type);
    assert.equal(api.methods.DiagramBuilt[name].returns, returns);
}

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
// A formula takes the group styles of ObjectConfig and none of the shape styles.
assert.equal(api.configBases.TexConfig, "ObjectConfig");
assert.deepEqual([...api.configBaseExcept.TexConfig].sort(), listNames(texBinding, "SHAPE_STYLES"),
    "TexConfig excepts other styles than the tex binding refuses");
assert.deepEqual(Object.keys(configFields("TexConfig")).sort(),
    [...new Set([...listNames(texBinding, "GROUP_STYLES"), ...listNames(texBinding, "FORMULA_OPTIONS")])].sort(),
    "TexConfig differs from the options tex accepts");
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
assert.deepEqual(api.methods.StatesController.transition.parameters, [
    {name: "object", type: "Object"}, {name: "state", type: "string"},
    {name: "options", type: "TimingOptions", optional: true},
]);
assert.deepEqual(api.methods.StatesController.transition.overloads[0], {
    parameters: [{name: "targets", type: "StateTarget[]"}, {name: "options", type: "TimingOptions", optional: true}],
    returns: "StatesController",
});
assert.equal(api.methods.StatesController.transition.overloads.length, 1);
// Timing has one options-table signature; track keeps its required options and its list form.
for (const [owner, methods] of [
    ["Scene", ["tween", "restore", "create", "uncreate", "fill_reveal", "draw_border_then_fill", "write",
        "shift", "transform", "warp", "fade", "grow_from_center", "grow_from_edge", "shrink_to_center",
        "morph", "fade_transform", "match_transform", "edit", "arrange", "transition", "stroke", "fill", "look"]],
    ["View", ["look", "fade"]],
]) {
    for (const name of methods) {
        const method = api.methods[owner][name];
        assert.equal(method.overloads, undefined, `${owner}:${name} has a redundant timing overload`);
        assert.ok(method.parameters.every((parameter) => !["duration", "curve", "lag"].includes(parameter.name)),
            `${owner}:${name} exposes positional timing`);
        assert.equal(method.parameters.at(-1).name, "options");
        assert.equal(method.parameters.at(-1).optional, true);
    }
}
assert.deepEqual(api.methods.Scene.track.parameters, [
    {name: "tracker", type: "Tracker"}, {name: "value", type: "number"}, {name: "options", type: "TimingOptions"},
]);
assert.deepEqual(api.methods.Scene.track.overloads, [{
    parameters: [{name: "moves", type: "{[1]: Tracker, [2]: number}[]"}, {name: "options", type: "TimingOptions"}],
    returns: "Scene",
}]);
for (const [name, base] of Object.entries(api.configBases)) {
    assert.ok(api.configs[name] && api.configs[base], `Unknown config inheritance: ${name}`);
}
for (const [name, fields] of Object.entries(api.configBaseExcept)) {
    const base = api.configBases[name];
    assert.ok(base, `${name} excepts fields of no base`);
    for (const field of fields) {
        assert.ok(api.configs[base][field] && !api.configs[name][field],
            `${name} excepts ${field}, which its base lacks or it declares itself`);
    }
}

// The LuaLS definitions npm run compile writes as api/vis.d.lua.
const meta = luaMeta(api);
// Lua factories require a table even when all its fields have defaults. JavaScript's
// optional options object must not make the LuaLS signature accept circle() or vis.scene().
assert.equal(api.globals.scene.parameters[0].optional, undefined);
assert.match(meta, /---@param config vis\.SceneConfig\n---@return vis\.Scene\nfunction Library\.scene\(config\)/);
for (const owner of ["Scene", "Object", "Group", "Space"]) {
    for (const [name, factory] of Object.entries(api.factories)) {
        if (owner === "Space" && name === "cell") continue;
        assert.match(meta, new RegExp(`---@param config vis\\.${factory.config}\\n---@return [^\\n]+\\nfunction ${owner}:${name}\\(config\\)`),
            `${owner}:${name} must require its Lua configuration table`);
    }
}
// These methods supply defaults themselves instead of requiring their configuration/vector.
assert.equal(api.methods.Scene.scene_transition.parameters[1].optional, true);
assert.match(meta, /---@param config\? vis\.SceneTransitionConfig\nfunction Scene:scene_transition\(scenes, config\)/);
assert.equal(api.methods.Object.arrange.parameters[0].optional, true);
assert.match(meta, /---@param direction\? vis\.Vec2\|vis\.Vec3\n---@param gap\? number\nfunction Group:arrange\(direction, gap\)/);
// Space:cell dispatches between the ordinary Cell factory and a sampled Group. Keep both
// signatures in one LuaLS method, and retain the sampled Group's selection/layout methods.
const staticCell = api.methods.Space.cell.overloads.find((overload) => overload.parameters[0]?.type === api.factories.cell.config);
assert.ok(staticCell, "Space:cell is missing its ordinary Cell configuration overload");
assert.equal(staticCell.parameters[0].optional, undefined);
assert.equal(staticCell.returns, api.factories.cell.returns);
for (const method of ["cell", "voxel"]) {
    assert.equal(api.methods.Space[method].returns, "Group");
    assert.match(meta, new RegExp(`---@return vis\\.Group\\nfunction Space:${method}\\(sample, config\\)`));
}
assert.match(meta, /---@overload fun\(self: vis\.Space, config: vis\.CellConfig\): vis\.Object/);
assert.equal([...meta.matchAll(/function Space:cell\(/g)].length, 1);
const curvePresets = /CURVE_PRESETS\[\]\s*=\s*\{([\s\S]*?)\};/.exec(binding)?.[1];
assert.ok(curvePresets, "Could not find the binding's curve presets");
const curveAliases = meta.match(/^---@alias vis\.Curve(?:Preset)? .+$/gm)?.join("\n") ?? "";
assert.deepEqual(
    [...new Set(Array.from(curveAliases.matchAll(/'([^']+)'/g), (match) => match[1]))].sort(),
    Array.from(curvePresets.matchAll(/\{"([^"]+)",/g), (match) => match[1]).sort(),
    "LuaLS curve presets differ from the Lua binding",
);
for (const name of sceneMethods) assert.match(meta, new RegExp(`function Scene:${name}\\(`));
assert.match(meta, /---@class vis\.Tracker\n/);
assert.match(meta, /function Tracker:get\(\)/);
// A config that takes all of ObjectConfig inherits it; one that takes part of it is written out.
assert.match(meta, /---@class vis\.CircleConfig: vis\.ObjectConfig\n/);
assert.match(meta, /---@class vis\.MatrixConfig\n(?:---@field [^\n]*\n)*---@field opacity\?/);
assert.doesNotMatch(meta, /---@class vis\.MatrixConfig\n(?:---@field [^\n]*\n)*---@field radius\?/);
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
assert.match(meta, /---@overload fun\(self: vis\.StatesController, targets: vis\.StateTarget\[\], options\?: vis\.TimingOptions\): vis\.StatesController/);
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
assert.match(meta, /---@class vis\.TexConfig\n(?:---@field [^\n]*\n)*---@field tex /);
assert.doesNotMatch(meta, /---@class vis\.TexConfig\n(?:---@field [^\n]*\n)*---@field stroke\?/);
assert.match(meta, /---@class vis\.TexPartConfig\n/);
assert.match(meta, /---@class vis\.TexSpanConfig\n/);

console.log("Lua API schema and generated LuaLS metadata are synchronized");

function registrationNames(source, constant) {
    return Object.keys(registrationTable(source, constant)).sort();
}

// The functions a luaL_Reg table registers, by the name each is registered under.
function registrationTable(source, constant) {
    const body = new RegExp(`static const luaL_Reg ${constant}\\[\\] = \\{([\\s\\S]*?)\\};`).exec(source)?.[1];
    assert.ok(body, `Could not find ${constant}`);
    return Object.fromEntries(Array.from(body.matchAll(/\{"([^"]+)",\s*(\w+)\}/g), (match) => [match[1], match[2]]));
}

// The source of a file-level function, up to the next one.
function functionBody(source, functionName) {
    const escaped = functionName.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const start = new RegExp(`static [^\\n]+ ${escaped}\\(`).exec(source)?.index;
    assert.notEqual(start, undefined, `Could not find ${functionName}`);
    const end = source.indexOf("\nstatic ", start + 1);
    return source.slice(start, end < 0 ? source.length : end);
}

// The option names the core's _factory(), _options() and _option() accept in the functions, with
// the shared styles unless the style flag is false, less the names a `for (auto shape : {...})`
// loop refuses.
function factoryOptionNames(source, functionNames, shared) {
    const names = new Set();
    const refused = new Set();
    for (const functionName of functionNames) {
        const body = functionBody(source, functionName);
        const calls = body.matchAll(/(?:_factory\(lua,\s*|_options\(lua,\s*(?:2|index),\s*)\{([^}]*)\}(?:,\s*(true|false))?\)|_option\(name,\s*\{([^}]*)\},\s*(true|false)\)/g);
        for (const match of calls) {
            for (const name of (match[1] ?? match[3]).matchAll(/"([^"]+)"/g)) names.add(name[1]);
            if ((match[2] ?? match[4]) !== "false") for (const name of shared) names.add(name);
        }
        for (const match of body.matchAll(/for \(auto shape : \{([^}]*)\}\)/g)) {
            for (const name of match[1].matchAll(/"([^"]+)"/g)) refused.add(name[1]);
        }
    }
    assert.ok(names.size, `No options in ${functionNames.join(", ")}`);
    return [...names].filter((name) => !refused.has(name)).sort();
}

// The fields a config accepts: its own over those of its bases, less the names it excepts.
function configFields(name) {
    const parent = api.configBases?.[name];
    if (!parent) return api.configs[name];
    const fields = {...configFields(parent)};
    for (const fieldName of api.configBaseExcept?.[name] ?? []) delete fields[fieldName];
    return {...fields, ...api.configs[name]};
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
