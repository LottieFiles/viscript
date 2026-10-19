import * as fs from "node:fs";
import * as vscode from "vscode";

type Parameter = {name: string; type: string; optional?: boolean};
type Signature = {parameters: Parameter[]; returns?: string};
type Callable = Signature & {description: string; overloads?: Signature[]};
type Factory = {config: string; returns: string; description: string};
type Field = {type: string; description: string; required?: boolean};
type Namespace = {type: string; description: string; functions: Record<string, Callable>};
type Api = {
    globals: Record<string, Callable>;
    namespaces?: Record<string, Namespace>;
    factories: Record<string, Factory>;
    methods: Record<string, Record<string, Callable>>;
    configs: Record<string, Record<string, Field>>;
    // The config each config inherits, and the inherited fields it does not take.
    configBases?: Record<string, string>;
    configBaseExcept?: Record<string, string[]>;
};

const IDENTIFIER = "[A-Za-z_][A-Za-z0-9_]*";

export function registerLanguageFeatures(
    context: vscode.ExtensionContext,
    enabled: (document: vscode.TextDocument) => boolean,
): vscode.Disposable[] {
    const apiPath = vscode.Uri.joinPath(context.extensionUri, "api", "vis-api.json").fsPath;
    const api = JSON.parse(fs.readFileSync(apiPath, "utf8")) as Api;
    const selector: vscode.DocumentSelector = {language: "lua", scheme: "file"};
    return [
        vscode.languages.registerCompletionItemProvider(
            selector,
            {provideCompletionItems: (document, position) => enabled(document) ? completions(api, document, position) : undefined},
            ".", ":", "{",
        ),
        vscode.languages.registerHoverProvider(selector, {
            provideHover: (document, position) => enabled(document) ? hover(api, document, position) : undefined,
        }),
        vscode.languages.registerSignatureHelpProvider(selector, {
            provideSignatureHelp: (document, position) => enabled(document) ? signatureHelp(api, document, position) : undefined,
        }, "(", ","),
    ];
}

function completions(api: Api, document: vscode.TextDocument, position: vscode.Position): vscode.CompletionItem[] | undefined {
    const source = document.getText(new vscode.Range(new vscode.Position(0, 0), position));
    const line = document.lineAt(position.line).text.slice(0, position.character);
    const namespace = new RegExp(`\\bvis\\.(${IDENTIFIER})\\.[A-Za-z_]*$`).exec(line)?.[1];
    if (namespace) {
        const functions = api.namespaces?.[namespace]?.functions;
        return functions ? Object.entries(functions).map(([name, callable]) => callableCompletion(name, callable)) : undefined;
    }
    if (/\bvis\.[A-Za-z_]*$/.test(line)) {
        return [
            ...Object.entries(api.globals).map(([name, callable]) => callableCompletion(name, callable)),
            ...Object.entries(api.namespaces ?? {}).map(([name, value]) => namespaceCompletion(name, value)),
        ];
    }
    const receiver = new RegExp(`(${IDENTIFIER}):[A-Za-z_]*$`).exec(line)?.[1];
    if (receiver) {
        const type = inferVariables(api, source).get(receiver);
        return type ? methodCompletions(api, type) : undefined;
    }
    const table = tableContext(api, source);
    if (!table) return undefined;
    const existing = new Set(Array.from(table.source.matchAll(/\b([A-Za-z_][A-Za-z0-9_]*)\s*=/g), (match) => match[1]));
    return Object.entries(configFields(api, table.config))
        .filter(([name]) => !existing.has(name))
        .map(([name, field]) => fieldCompletion(name, field));
}

function hover(api: Api, document: vscode.TextDocument, position: vscode.Position): vscode.Hover | undefined {
    const range = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
    if (!range) return undefined;
    const name = document.getText(range);
    const prefix = document.lineAt(position.line).text.slice(0, range.start.character);
    const documentPrefix = document.getText(new vscode.Range(new vscode.Position(0, 0), range.start));
    const namespaceName = new RegExp(`\\bvis\\.(${IDENTIFIER})\\.$`).exec(prefix)?.[1];
    if (namespaceName) {
        const callable = api.namespaces?.[namespaceName]?.functions[name];
        if (callable) return new vscode.Hover(callableMarkdown(`vis.${namespaceName}.${name}`, callable), range);
    }
    if (/\bvis\.$/.test(prefix) && api.namespaces?.[name]) {
        return new vscode.Hover(namespaceMarkdown(name, api.namespaces[name]), range);
    }
    if (/\bvis\.$/.test(prefix) && api.globals[name]) return new vscode.Hover(callableMarkdown(`vis.${name}`, api.globals[name]), range);
    const receiver = new RegExp(`(${IDENTIFIER}):\\s*$`).exec(prefix)?.[1];
    if (receiver) {
        const type = inferVariables(api, documentPrefix).get(receiver);
        const callable = type && lookupMethod(api, type, name);
        if (callable) return new vscode.Hover(callableMarkdown(`${type}:${name}`, callable), range);
    }
    const table = tableContext(api, documentPrefix);
    const field = table && configFields(api, table.config)[name];
    if (field) return new vscode.Hover(fieldMarkdown(name, field), range);
    if (api.factories[name]) return new vscode.Hover(factoryMarkdown(name, api.factories[name]), range);
    for (const methods of Object.values(api.methods)) {
        if (methods[name]) return new vscode.Hover(callableMarkdown(name, methods[name]), range);
    }
    for (const fields of Object.values(api.configs)) {
        if (fields[name]) return new vscode.Hover(fieldMarkdown(name, fields[name]), range);
    }
    return undefined;
}

function signatureHelp(api: Api, document: vscode.TextDocument, position: vscode.Position): vscode.SignatureHelp | undefined {
    const source = document.getText(new vscode.Range(new vscode.Position(0, 0), position));
    const call = new RegExp(`(?:(${IDENTIFIER}):|vis(?:\\.(${IDENTIFIER}))?\\.)([A-Za-z_][A-Za-z0-9_]*)\\s*\\(([^()]*)$`).exec(source);
    if (!call) return undefined;
    const type = call[1] ? inferVariables(api, source.slice(0, call.index)).get(call[1]) : undefined;
    const callable = call[1]
        ? type && lookupMethod(api, type, call[3])
        : call[2]
            ? api.namespaces?.[call[2]]?.functions[call[3]]
            : api.globals[call[3]];
    if (!callable) return undefined;
    const label = call[1]
        ? `${call[1]}:${call[3]}`
        : `vis.${call[2] ? `${call[2]}.` : ""}${call[3]}`;
    const result = new vscode.SignatureHelp();
    const signatures = [callable, ...(callable.overloads ?? [])];
    result.signatures = signatures.map((overload) => {
        const signature = new vscode.SignatureInformation(callableLabel(label, overload), new vscode.MarkdownString(callable.description));
        signature.parameters = overload.parameters.map((parameter) => new vscode.ParameterInformation(
            `${parameter.name}${parameter.optional ? "?" : ""}: ${parameter.type}`,
        ));
        return signature;
    });
    const argument = commaCount(call[4]);
    const named = unmatchedBraces(call[4]).length
        ? signatures.findIndex((signature) => api.configs[signature.parameters[argument]?.type]) : -1;
    result.activeSignature = Math.max(0, named);
    result.activeParameter = Math.min(argument, Math.max(0, result.signatures[result.activeSignature].parameters.length - 1));
    return result;
}

function inferVariables(api: Api, source: string): Map<string, string> {
    const variables = new Map<string, string>();
    const assignment = new RegExp(
        `(?:local\\s+)?(${IDENTIFIER})\\s*=\\s*(?:vis\\.(${IDENTIFIER})(?:\\.(${IDENTIFIER}))?\\b|(${IDENTIFIER}):(${IDENTIFIER})\\b)`,
        "g",
    );
    for (const match of source.matchAll(assignment)) {
        if (match[2]) {
            const callable = match[3]
                ? api.namespaces?.[match[2]]?.functions[match[3]]
                : api.globals[match[2]];
            if (callable?.returns) variables.set(match[1], callable.returns);
        } else {
            const owner = variables.get(match[4]);
            const callable = owner ? lookupMethod(api, owner, match[5]) : undefined;
            if (callable?.returns) variables.set(match[1], callable.returns);
        }
    }
    return variables;
}

function methodCompletions(api: Api, type: string): vscode.CompletionItem[] {
    const items = new Map<string, vscode.CompletionItem>();
    if (supportsFactories(type)) {
        for (const [name, factory] of Object.entries(api.factories)) items.set(name, factoryCompletion(name, factory));
    }
    for (const className of methodClasses(type)) {
        for (const [name, callable] of Object.entries(api.methods[className] ?? {})) items.set(name, callableCompletion(name, callable));
    }
    return [...items.values()];
}

function lookupMethod(api: Api, type: string, name: string): Callable | undefined {
    if (supportsFactories(type) && api.factories[name]) return factoryCallable(api.factories[name]);
    for (const className of methodClasses(type)) {
        const callable = api.methods[className]?.[name];
        if (callable) return callable;
    }
    return undefined;
}

function methodClasses(type: string): string[] {
    if (type === "Scene") return ["Scene"];
    if (type === "Panel") return ["Panel"];
    if (type === "Space") return ["Space", "Object"];
    if (type === "Group") return ["Group", "Object"];
    return ["Object"];
}

function supportsFactories(type: string): boolean {
    return type === "Scene" || type === "Object" || type === "Group" || type === "Space";
}

function callableCompletion(name: string, callable: Callable): vscode.CompletionItem {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
    item.detail = callableLabel(name, callable);
    item.documentation = callableMarkdown(name, callable);
    if (callable.parameters.length === 1 && callable.parameters[0].type.endsWith("Config")) {
        item.insertText = new vscode.SnippetString(`${name} {\n\t\${1}\n}`);
        return item;
    }
    const parameters = callable.parameters.map((parameter, index) => `\${${index + 1}:${parameter.name}}`).join(", ");
    item.insertText = new vscode.SnippetString(`${name}(${parameters})`);
    item.command = {command: "editor.action.triggerParameterHints", title: "Trigger parameter hints"};
    return item;
}

function namespaceCompletion(name: string, namespace: Namespace): vscode.CompletionItem {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Module);
    item.detail = `vis.${name}`;
    item.documentation = new vscode.MarkdownString(namespace.description);
    item.insertText = name;
    return item;
}

function factoryCompletion(name: string, factory: Factory): vscode.CompletionItem {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
    item.detail = `${name}(config?: ${factory.config}): ${factory.returns}`;
    item.documentation = factoryMarkdown(name, factory);
    item.insertText = new vscode.SnippetString(`${name} {\n\t\${1}\n}`);
    return item;
}

function fieldCompletion(name: string, field: Field): vscode.CompletionItem {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Field);
    item.detail = `${name}${field.required ? "" : "?"}: ${field.type}`;
    item.documentation = fieldMarkdown(name, field);
    item.insertText = new vscode.SnippetString(`${name} = ${valueSnippet(field.type)}`);
    return item;
}

function valueSnippet(type: string): string {
    if (type === "boolean") return "${1|true,false|}";
    const values = Array.from(type.matchAll(/\"([^\"]+)\"/g), (match) => match[1]);
    if (values.length) return `"\${1|${values.join(",")}|}"`;
    if (/Color|string/.test(type)) return '"${1}"';
    if (/Vec2|Range/.test(type)) return "{${1:0}, ${2:0}}";
    if (/Vec3/.test(type)) return "{${1:0}, ${2:0}, ${3:0}}";
    if (type.endsWith("Config")) return "{${1}}";
    return "${1}";
}

function tableContext(api: Api, source: string): {config: string; source: string} | undefined {
    const braces = unmatchedBraces(source);
    let config: string | undefined;
    for (let index = 0; index < braces.length; index++) {
        const open = braces[index];
        const prefix = source.slice(0, open);
        const method = new RegExp(`(${IDENTIFIER}):(${IDENTIFIER})\\s*(?:\\(([^()]*)|\\s*)$`).exec(prefix);
        if (/vis\.scene\s*\(?\s*$/.test(prefix)) {
            config = "SceneConfig";
        } else if (method) {
            const owner = inferVariables(api, prefix.slice(0, method.index)).get(method[1]);
            const callable = owner ? lookupMethod(api, owner, method[2]) : undefined;
            const argument = commaCount(method[3] ?? "");
            const signatures = callable ? [callable, ...(callable.overloads ?? [])] : [];
            config = signatures.map((signature) => signature.parameters[argument]?.type)
                .find((type) => type && api.configs[type]) ?? api.factories[method[2]]?.config;
        } else if (config) {
            const fieldName = /([A-Za-z_][A-Za-z0-9_]*)\s*=\s*$/.exec(prefix)?.[1];
            const type = fieldName ? configFields(api, config)[fieldName]?.type : undefined;
            config = type && api.configs[type] ? type : undefined;
        }
        if (config && index === braces.length - 1) return {config, source: source.slice(open + 1)};
    }
    return undefined;
}

function unmatchedBraces(source: string): number[] {
    const stack: number[] = [];
    let quote = "";
    let comment = false;
    for (let index = 0; index < source.length; index++) {
        const char = source[index];
        if (comment) {
            if (char === "\n") comment = false;
        } else if (quote) {
            if (char === "\\") index++;
            else if (char === quote) quote = "";
        } else if (char === "-" && source[index + 1] === "-") {
            comment = true;
            index++;
        } else if (char === '"' || char === "'") quote = char;
        else if (char === "{") stack.push(index);
        else if (char === "}") stack.pop();
    }
    return stack;
}

// The fields a config accepts: its own over those of its bases, less the names it excepts.
function configFields(api: Api, config: string): Record<string, Field> {
    const own = api.configs[config] ?? {};
    const base = api.configBases?.[config];
    if (!base) return own;
    const fields = {...configFields(api, base)};
    for (const name of api.configBaseExcept?.[config] ?? []) delete fields[name];
    return {...fields, ...own};
}

function factoryCallable(factory: Factory): Callable {
    return {description: factory.description, parameters: [{name: "config", type: factory.config, optional: true}], returns: factory.returns};
}

function callableLabel(name: string, callable: Signature): string {
    const parameters = callable.parameters.map((parameter) => `${parameter.name}${parameter.optional ? "?" : ""}: ${parameter.type}`).join(", ");
    return `${name}(${parameters})${callable.returns ? `: ${callable.returns}` : ""}`;
}

function callableMarkdown(name: string, callable: Callable): vscode.MarkdownString {
    const markdown = new vscode.MarkdownString();
    for (const overload of [callable, ...(callable.overloads ?? [])]) {
        markdown.appendCodeblock(callableLabel(name, overload), "lua");
    }
    markdown.appendMarkdown(callable.description);
    return markdown;
}

function namespaceMarkdown(name: string, namespace: Namespace): vscode.MarkdownString {
    const markdown = new vscode.MarkdownString();
    markdown.appendCodeblock(`vis.${name}`, "lua");
    markdown.appendMarkdown(namespace.description);
    return markdown;
}

function factoryMarkdown(name: string, factory: Factory): vscode.MarkdownString {
    return callableMarkdown(name, factoryCallable(factory));
}

function fieldMarkdown(name: string, field: Field): vscode.MarkdownString {
    const markdown = new vscode.MarkdownString();
    markdown.appendCodeblock(`${name}${field.required ? "" : "?"}: ${field.type}`, "lua");
    markdown.appendMarkdown(field.description);
    return markdown;
}

function commaCount(source: string): number {
    let depth = 0;
    let count = 0;
    for (const char of source) {
        if ("({[".includes(char)) depth++;
        else if (")}]".includes(char)) depth--;
        else if (char === "," && depth === 0) count++;
    }
    return count;
}
