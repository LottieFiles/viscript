import {createBundledHighlighter, createSingletonShorthands} from "shiki/core";
import {createJavaScriptRegexEngine} from "shiki/engine/javascript";
import {bundledLanguages, bundledLanguagesInfo} from "shiki/langs";

const themes = {
    "dark-plus": () => import("@shikijs/themes/dark-plus"),
    "light-plus": () => import("@shikijs/themes/light-plus"),
    "github-dark-high-contrast": () => import("@shikijs/themes/github-dark-high-contrast"),
    "github-light-high-contrast": () => import("@shikijs/themes/github-light-high-contrast"),
};

const createHighlighter = createBundledHighlighter({
    langs: bundledLanguages,
    themes,
    engine: () => createJavaScriptRegexEngine(),
});
const {codeToTokens} = createSingletonShorthands(createHighlighter);

const languageAliases = new Map([
    ["bat", "batch"],
    ["cuda-cpp", "cuda"],
    ["dockerfile", "docker"],
    ["javascriptreact", "jsx"],
    ["makefile", "make"],
    ["objective-c", "objective-c"],
    ["objective-cpp", "objective-cpp"],
    ["plaintext", "text"],
    ["typescriptreact", "tsx"],
]);
for (const info of bundledLanguagesInfo) {
    languageAliases.set(info.id, info.id);
    for (const alias of info.aliases || []) languageAliases.set(alias, info.id);
}

export function shikiLanguage(language) {
    const normalized = typeof language === "string" ? language.trim().toLowerCase() : "";
    return languageAliases.get(normalized) || "text";
}

export function shikiTheme(classNames) {
    const names = classNames instanceof Set ? classNames : new Set(classNames || []);
    if (names.has("vscode-high-contrast-light")) return "github-light-high-contrast";
    if (names.has("vscode-high-contrast")) return "github-dark-high-contrast";
    if (names.has("vscode-light")) return "light-plus";
    return "dark-plus";
}

export async function highlightSourceLines(lines, language, theme) {
    if (!Array.isArray(lines) || lines.some((line) => typeof line !== "string")) return [];
    const result = await codeToTokens(lines.join("\n"), {
        lang: shikiLanguage(language),
        theme: themes[theme] ? theme : "dark-plus",
    });
    return lines.map((line, index) => result.tokens[index] || [{content: line}]);
}
