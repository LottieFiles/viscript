export type SourceRange = {
    startLine: number;
    endLine: number;
    startColumn?: number;
    endColumn?: number;
};

export type RelatedSource = {
    label: string;
    source: string;
    line?: number;
    column?: number;
    symbol?: string;
    range?: SourceRange;
    diff?: SourceDiff;
};

export type SourceDiff = {
    base: string;
    head: string;
    oldPath?: string;
};

export type VisualizationEntry = {
    source: string;
    symbol: string;
    animation: string;
    series: string;
    title?: string;
    description?: string;
    line?: number;
    links: RelatedSource[];
};

export type ParsedVisualizationManifest = {
    entries: VisualizationEntry[];
    errors: string[];
};

export type VisualizationLocation = {
    entry: VisualizationEntry;
    symbol: string;
    line?: number;
    column?: number;
    range?: SourceRange;
    relatedLabel?: string;
};

export type VisualizationSeriesGroup = {
    label: string;
    entries: VisualizationEntry[];
};

export function matchingVisualizationAnimation(
    entries: VisualizationEntry[],
    animationPath: string,
): VisualizationEntry | undefined {
    const matches = entries.filter((entry) => entry.animation === animationPath);
    return matches.length === 1 ? matches[0] : undefined;
}

export function matchingVisualizationLocations(
    entries: VisualizationEntry[],
    workspacePath: string,
): VisualizationLocation[] {
    const matches: VisualizationLocation[] = [];
    for (const entry of entries) {
        if (entry.source === workspacePath) matches.push({entry, symbol: entry.symbol, line: entry.line});
        for (const link of entry.links) {
            if (link.source !== workspacePath || (!link.symbol && link.line === undefined && !link.range)) continue;
            matches.push({
                entry,
                symbol: link.symbol ?? entry.symbol,
                line: link.line,
                column: link.column,
                range: link.range,
                relatedLabel: link.label,
            });
        }
    }
    return matches;
}

export function uniqueVisualizationLocation(
    locations: readonly VisualizationLocation[],
): VisualizationLocation | undefined {
    let unique: VisualizationLocation | undefined;
    let uniqueKey: string | undefined;
    for (const location of locations) {
        const entry = location.entry;
        const key = `${entry.source}\0${entry.symbol}\0${entry.line ?? ""}\0${entry.animation}`;
        if (key === uniqueKey) continue;
        if (unique) return undefined;
        unique = location;
        uniqueKey = key;
    }
    return unique;
}

export function matchingVisualizationUsage(
    entries: VisualizationEntry[],
    word: string,
    definitionPaths: string[],
): VisualizationEntry | undefined {
    const candidates = entries.filter((entry) => symbolLeaf(entry.symbol) === word);
    if (!candidates.length) return undefined;
    if (!definitionPaths.length) return candidates.length === 1 ? candidates[0] : undefined;
    const definitions = new Set(definitionPaths);
    const matched = candidates.filter((entry) => definitions.has(entry.source)
        || entry.links.some((link) => definitions.has(link.source)));
    if (matched.length === 1) return matched[0];
    if (matched.length > 1) return undefined;
    return candidates.length === 1 ? candidates[0] : undefined;
}

export function visualizationSeriesEntries(
    entries: VisualizationEntry[],
    current: VisualizationEntry,
): VisualizationEntry[] {
    return entries.filter((entry) => entry.series === current.series);
}

export function visualizationSeriesGroups(entries: VisualizationEntry[]): VisualizationSeriesGroup[] {
    const groups = new Map<string, VisualizationEntry[]>();
    for (const entry of entries) {
        const items = groups.get(entry.series);
        if (items) items.push(entry);
        else groups.set(entry.series, [entry]);
    }
    return [...groups].map(([label, groupEntries]) => ({label, entries: groupEntries}));
}

export function parseVisualizationManifest(value: unknown): ParsedVisualizationManifest {
    const errors: string[] = [];
    if (!isRecord(value)) return {entries: [], errors: ["The manifest root must be an object."]};
    if (value.version !== 2) errors.push('"version" must be 2.');
    if (!Array.isArray(value.visualizations)) {
        return {entries: [], errors: [...errors, '"visualizations" must be an array.']};
    }

    const entries: VisualizationEntry[] = [];
    value.visualizations.forEach((item, index) => {
        const prefix = `visualizations[${index}]`;
        if (!isRecord(item)) {
            errors.push(`${prefix} must be an object.`);
            return;
        }
        const source = workspacePath(item.source, `${prefix}.source`, errors);
        const symbol = nonEmptyString(item.symbol, `${prefix}.symbol`, errors);
        const series = seriesName(item.series, `${prefix}.series`, errors);
        const animation = seriesAnimationPath(item.animation, series, `${prefix}.animation`, errors);
        const title = optionalString(item.title, `${prefix}.title`, errors);
        const description = optionalString(item.description, `${prefix}.description`, errors);
        const line = optionalPositiveInteger(item.line, `${prefix}.line`, errors);
        const links = relatedSources(item.links, prefix, errors);
        if (source && animation && symbol && series) {
            entries.push({source, symbol, animation, series, title, description, line, links});
        }
    });
    return {entries, errors};
}

function seriesAnimationPath(
    value: unknown,
    series: string | undefined,
    field: string,
    errors: string[],
): string | undefined {
    const animation = workspacePath(value, field, errors);
    if (!animation || !series) return undefined;
    if (typeof value === "string" && value.includes("\\")) {
        errors.push(`${field} must use forward slashes.`);
        return undefined;
    }
    const parts = animation.split("/");
    if (parts.length !== 5
        || parts[0] !== ".vscode"
        || parts[1] !== "vis"
        || parts[2] !== series
        || parts[3] !== "animations"
        || parts[4].length <= 4
        || !parts[4].endsWith(".lua")) {
        errors.push(`${field} must match .vscode/vis/${series}/animations/<scene>.lua.`);
        return undefined;
    }
    return animation;
}

function seriesName(value: unknown, field: string, errors: string[]): string | undefined {
    const series = nonEmptyString(value, field, errors);
    if (!series) return undefined;
    if (series === "." || series === ".." || series.includes("/") || series.includes("\\")) {
        errors.push(`${field} must be one directory name without path separators.`);
        return undefined;
    }
    return series;
}

export function normalizeWorkspacePath(value: string): string | undefined {
    const normalized = value.replaceAll("\\", "/").replace(/^\.\//, "");
    if (!normalized || normalized.startsWith("/") || /^[A-Za-z]:\//.test(normalized)) return undefined;
    const parts = normalized.split("/");
    if (parts.some((part) => !part || part === "." || part === "..")) return undefined;
    return parts.join("/");
}

function relatedSources(value: unknown, prefix: string, errors: string[]): RelatedSource[] {
    if (value === undefined) return [];
    if (!Array.isArray(value)) {
        errors.push(`${prefix}.links must be an array.`);
        return [];
    }
    const links: RelatedSource[] = [];
    value.forEach((item, index) => {
        const itemPrefix = `${prefix}.links[${index}]`;
        if (!isRecord(item)) {
            errors.push(`${itemPrefix} must be an object.`);
            return;
        }
        const label = nonEmptyString(item.label, `${itemPrefix}.label`, errors);
        const source = workspacePath(item.source, `${itemPrefix}.source`, errors);
        const line = optionalPositiveInteger(item.line, `${itemPrefix}.line`, errors);
        const column = optionalPositiveInteger(item.column, `${itemPrefix}.column`, errors);
        const symbol = optionalString(item.symbol, `${itemPrefix}.symbol`, errors);
        const range = optionalSourceRange(item.range, `${itemPrefix}.range`, errors);
        const diff = optionalSourceDiff(item.diff, `${itemPrefix}.diff`, errors);
        if (item.diff !== undefined && item.range === undefined) {
            errors.push(`${itemPrefix}.range is required when diff is present.`);
        }
        if (label
            && source
            && (item.range === undefined || range)
            && (item.diff === undefined || (diff && range))) {
            links.push({label, source, line, column, symbol, range, diff});
        }
    });
    return links;
}

function optionalSourceDiff(value: unknown, field: string, errors: string[]): SourceDiff | undefined {
    if (value === undefined) return undefined;
    if (!isRecord(value)) {
        errors.push(`${field} must be an object.`);
        return undefined;
    }
    const errorCount = errors.length;
    const base = gitCommit(value.base, `${field}.base`, errors);
    const head = gitCommit(value.head, `${field}.head`, errors);
    const oldPath = value.oldPath === undefined
        ? undefined
        : workspacePath(value.oldPath, `${field}.oldPath`, errors);
    if (errors.length !== errorCount || !base || !head) return undefined;
    return {base, head, oldPath};
}

function optionalSourceRange(value: unknown, field: string, errors: string[]): SourceRange | undefined {
    if (value === undefined) return undefined;
    if (!isRecord(value)) {
        errors.push(`${field} must be an object.`);
        return undefined;
    }

    const errorCount = errors.length;
    const startLine = positiveInteger(value.startLine, `${field}.startLine`, errors);
    const endLine = positiveInteger(value.endLine, `${field}.endLine`, errors);
    const startColumn = optionalPositiveInteger(value.startColumn, `${field}.startColumn`, errors);
    const endColumn = optionalPositiveInteger(value.endColumn, `${field}.endColumn`, errors);
    const hasStartColumn = value.startColumn !== undefined;
    const hasEndColumn = value.endColumn !== undefined;

    if (hasStartColumn !== hasEndColumn) {
        errors.push(`${field}.startColumn and ${field}.endColumn must be provided together.`);
    }
    if (startLine !== undefined && endLine !== undefined && endLine < startLine) {
        errors.push(`${field}.endLine must not be before ${field}.startLine.`);
    }
    if (startLine === endLine && startColumn !== undefined && endColumn !== undefined && endColumn <= startColumn) {
        errors.push(`${field}.endColumn must be greater than ${field}.startColumn on the same line.`);
    }
    if (errors.length !== errorCount
        || startLine === undefined
        || endLine === undefined
        || hasStartColumn !== hasEndColumn) {
        return undefined;
    }
    if (startColumn === undefined || endColumn === undefined) return {startLine, endLine};
    return {startLine, endLine, startColumn, endColumn};
}

function workspacePath(value: unknown, field: string, errors: string[]): string | undefined {
    const text = nonEmptyString(value, field, errors);
    if (!text) return undefined;
    const normalized = normalizeWorkspacePath(text);
    if (!normalized) errors.push(`${field} must be a path inside the workspace.`);
    return normalized;
}

function nonEmptyString(value: unknown, field: string, errors: string[]): string | undefined {
    if (typeof value !== "string" || !value.trim()) {
        errors.push(`${field} must be a non-empty string.`);
        return undefined;
    }
    return value.trim();
}

function optionalString(value: unknown, field: string, errors: string[]): string | undefined {
    if (value === undefined) return undefined;
    return nonEmptyString(value, field, errors);
}

function optionalPositiveInteger(value: unknown, field: string, errors: string[]): number | undefined {
    if (value === undefined) return undefined;
    if (!Number.isInteger(value) || (value as number) < 1) {
        errors.push(`${field} must be a positive integer.`);
        return undefined;
    }
    return value as number;
}

function positiveInteger(value: unknown, field: string, errors: string[]): number | undefined {
    if (!Number.isInteger(value) || (value as number) < 1) {
        errors.push(`${field} must be a positive integer.`);
        return undefined;
    }
    return value as number;
}

function gitCommit(value: unknown, field: string, errors: string[]): string | undefined {
    const commit = nonEmptyString(value, field, errors);
    if (!commit) return undefined;
    if (!/^[0-9a-f]{40}$/i.test(commit)) {
        errors.push(`${field} must be a full 40-character Git commit SHA.`);
        return undefined;
    }
    return commit.toLowerCase();
}

function isRecord(value: unknown): value is Record<string, unknown> {
    return typeof value === "object" && value !== null && !Array.isArray(value);
}

function symbolLeaf(symbol: string): string {
    return symbol.split(/::|[.:]/).filter(Boolean).at(-1) ?? symbol;
}
