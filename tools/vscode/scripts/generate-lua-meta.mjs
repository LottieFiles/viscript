// Writes api/vis.d.lua, the LuaLS definitions, from the API schema api/vis-api.json. The file is
// not tracked: npm run compile writes it, and api-sync checks the text luaMeta gives.
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

/** The LuaLS definitions for the API schema `api`. */
export function luaMeta(api) {
    const output = [
        "---@meta vis",
        "",
        "-- Generated from api/vis-api.json. Do not edit directly.",
        "",
        "---@alias vis.Color string",
        "---@alias vis.ThemeColor 'background'|'foreground'|'muted'|'accent'|'secondary'|'success'|'warning'|'danger'|'info'|'surface'|'border'|'result'|'focus'",
        "---@alias vis.Vec2 {[1]: number, [2]: number}",
        "---@alias vis.Vec3 {[1]: number, [2]: number, [3]: number}",
        "---@alias vis.Range {[1]: number, [2]: number, [3]?: number}",
        "---@alias vis.Mat4 number[]",
        "---@alias vis.Curve 'linear'|'smooth'|'ease_in'|'ease_out'|'ease_in_out'|'gentle'|'snappy'|{preset: string, strength?: number, reverse?: boolean}|{bezier: number[], strength?: number}",
        "---@alias vis.CreateDirection 'forward'|'reverse'|'clockwise'|'counterclockwise'",
        "---@alias vis.SceneTransitionStyleName 'match'|'fade'|'slide'|'zoom_in'|'zoom_out'",
        "---@alias vis.PathCommand table",
        "",
    ];

    // A config inherits the base configBases names in LuaLS, which cannot leave fields out, so one
    // that takes only part of its base (configBaseExcept) is written out with the part it takes.
    for (const [name, fields] of Object.entries(api.configs)) {
        const parent = api.configBases?.[name];
        const except = api.configBaseExcept?.[name];
        output.push(`---@class vis.${name}${parent && !except ? `: vis.${parent}` : ""}`);
        const emitted = new Map(except ? Object.entries(configFields(parent)).filter(([fieldName]) => !except.includes(fieldName)) : []);
        for (const [fieldName, field] of Object.entries(fields)) {
            if (parent && !except && JSON.stringify(api.configs[parent][fieldName]) === JSON.stringify(field)) continue;
            emitted.set(fieldName, field);
        }
        for (const [fieldName, field] of emitted) {
            output.push(`---@field ${fieldName}${field.required ? "" : "?"} ${qualify(field.type)} ${field.description}`);
        }
        output.push("");
    }

    output.push("---@class vis.Object", "local Object = {}", "");
    emitFactories(output, "Object", api.factories);
    emitMethods(output, "Object", api.methods.Object);
    output.push("---@class vis.Group: vis.Object", "local Group = {}", "");
    emitFactories(output, "Group", api.factories);
    emitMethods(output, "Group", api.methods.Object);
    emitMethods(output, "Group", api.methods.Group ?? {});
    output.push("---@class vis.Space: vis.Object", "local Space = {}", "");
    emitFactories(output, "Space", Object.fromEntries(Object.entries(api.factories).filter(([name]) => name !== "cell")));
    emitMethods(output, "Space", api.methods.Object);
    emitMethods(output, "Space", api.methods.Space);
    output.push("---@class vis.Scene", "local Scene = {}", "");
    emitFactories(output, "Scene", api.factories);
    emitMethods(output, "Scene", api.methods.Scene);
    if (api.methods.Panel) {
        output.push("---Experimental Scene-owned interaction panel; available only with optional UI support.",
            "---@class vis.Panel", "local Panel = {}", "");
        emitMethods(output, "Panel", api.methods.Panel);
    }
    for (const [name, type] of Object.entries(api.types ?? {})) {
        output.push(`---${type.description}`, `---@class vis.${name}`, `local ${name} = {}`, "");
        if (api.methods[name]) emitMethods(output, name, api.methods[name]);
    }
    for (const namespace of Object.values(api.namespaces ?? {})) {
        output.push(`---${namespace.description}`, `---@class vis.${namespace.type}`,
            `local ${namespace.type} = {}`, "");
        for (const [name, callable] of Object.entries(namespace.functions)) {
            emitCallable(output, namespace.type, name, callable, false);
        }
    }
    output.push("---@class vis.Library");
    for (const [name, namespace] of Object.entries(api.namespaces ?? {})) {
        output.push(`---@field ${name}? vis.${namespace.type} ${namespace.description}`);
    }
    output.push("local Library = {}", "");
    for (const [name, callable] of Object.entries(api.globals)) emitCallable(output, "Library", name, callable, false);
    output.push("---@type vis.Library", "vis = {}", "");

    return `${output.join("\n").trimEnd()}\n`;

    function emitFactories(lines, owner, factories) {
        for (const [name, factory] of Object.entries(factories)) {
            emitCallable(lines, owner, name, {
                description: factory.description,
                parameters: [{name: "config", type: factory.config, optional: true}],
                returns: factory.returns,
            }, true);
        }
    }

    function emitMethods(lines, owner, methods) {
        for (const [name, callable] of Object.entries(methods)) emitCallable(lines, owner, name, callable, true);
    }

    function emitCallable(lines, owner, name, callable, method) {
        lines.push(`---${callable.description}`);
        for (const overload of callable.overloads ?? []) {
            const parameters = [
                ...(method ? [`self: vis.${owner}`] : []),
                ...overload.parameters.map((parameter) => `${parameter.name}${parameter.optional ? "?" : ""}: ${qualify(parameter.type)}`),
            ].join(", ");
            lines.push(`---@overload fun(${parameters})${overload.returns ? `: ${qualify(overload.returns)}` : ""}`);
        }
        for (const parameter of callable.parameters) {
            lines.push(`---@param ${parameter.name}${parameter.optional ? "?" : ""} ${qualify(parameter.type)}`);
        }
        if (callable.returns) lines.push(`---@return ${qualify(callable.returns)}`);
        const parameters = callable.parameters.map((parameter) => parameter.name).join(", ");
        lines.push(`function ${owner}${method ? ":" : "."}${name}(${parameters}) end`, "");
    }

    // The fields a config accepts: its own over those of its bases, less the names it excepts.
    function configFields(name) {
        const parent = api.configBases?.[name];
        if (!parent) return api.configs[name];
        const fields = {...configFields(parent)};
        for (const fieldName of api.configBaseExcept?.[name] ?? []) delete fields[fieldName];
        return {...fields, ...api.configs[name]};
    }

    function qualify(type) {
        const names = [...new Set([
            "Color", "CreateDirection", "Curve", "Group", "Mat4", "Object", "Panel",
            "PathCommand", "Range", "Scene", "SceneTransitionStyleName", "Space", "ThemeColor",
            ...Object.values(api.namespaces ?? {}).map((namespace) => namespace.type),
            "Vec2", "Vec3", ...Object.keys(api.configs), ...Object.keys(api.types ?? {}),
        ])].sort((left, right) => right.length - left.length);
        let result = type;
        for (const name of names) result = result.replace(new RegExp(`\\b${name}\\b`, "g"), `vis.${name}`);
        return result;
    }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
    const api = JSON.parse(fs.readFileSync(path.join(root, "api", "vis-api.json"), "utf8"));
    fs.writeFileSync(path.join(root, "api", "vis.d.lua"), luaMeta(api));
}
