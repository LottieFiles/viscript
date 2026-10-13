export function normalizeSourceContexts(value) {
    if (!Array.isArray(value)) return [];
    const contexts = [];
    for (const item of value) {
        if (!item || typeof item !== "object" || !Array.isArray(item.lines)
            || item.lines.some((line) => typeof line !== "string")) return [];
        const kind = item.kind === "diff" ? "diff" : "source";
        const diffRows = kind === "diff" ? normalizedDiffRows(item.diffRows) : undefined;
        if (kind === "diff" && !diffRows) return [];
        const lines = [...item.lines];
        const startLine = positiveInteger(item.startLine, 1);
        const fallbackEndLine = startLine + Math.max(0, lines.length - 1);
        const endLine = Math.max(startLine, positiveInteger(item.endLine, fallbackEndLine));
        const focusStartLine = positiveInteger(item.focusStartLine, startLine);
        const focusEndLine = Math.max(focusStartLine, positiveInteger(item.focusEndLine, endLine));
        const baseStartLine = Math.min(
            endLine,
            Math.max(startLine, positiveInteger(item.baseStartLine, focusStartLine)),
        );
        const baseEndLine = Math.min(
            endLine,
            Math.max(baseStartLine, positiveInteger(item.baseEndLine, Math.min(focusEndLine, endLine))),
        );
        contexts.push({
            kind,
            id: typeof item.id === "string" && item.id ? item.id : undefined,
            label: stringValue(item.label),
            path: stringValue(item.path),
            language: stringValue(item.language),
            startLine,
            endLine,
            focusStartLine,
            focusEndLine,
            totalLineCount: Math.max(endLine, positiveInteger(item.totalLineCount, endLine)),
            hasMoreAbove: item.hasMoreAbove === true,
            hasMoreBelow: item.hasMoreBelow === true,
            baseStartLine,
            baseEndLine,
            expanded: item.expanded === true,
            lines,
            truncated: item.truncated === true,
            unavailable: item.unavailable === true,
            oldPath: kind === "diff" ? stringValue(item.oldPath) : "",
            diffRows,
        });
    }
    return contexts;
}

export function sourceContextExpansionCount(context, direction, step = 5) {
    if (!Number.isInteger(step) || step <= 0) return 0;
    if (direction === "above") return Math.min(step, Math.max(0, context.startLine - 1));
    if (direction === "below") {
        return Math.min(step, Math.max(0, context.totalLineCount - context.endLine));
    }
    return 0;
}

export function resolveActiveSourceIndex(contexts, value) {
    if (!contexts.length) return -1;
    return Number.isInteger(value) && value >= 0 && value < contexts.length ? value : 0;
}

export function sourceContextKeys(contexts) {
    const occurrences = new Map();
    return contexts.map((context) => {
        const base = context.id
            ? JSON.stringify(["id", context.id])
            : JSON.stringify(["legacy", context.path, context.label, context.language]);
        const occurrence = occurrences.get(base) ?? 0;
        occurrences.set(base, occurrence + 1);
        return `${base}:${occurrence}`;
    });
}

export function reconcileInspectorOpenState(contexts, previous, activeIndex, preserve) {
    const contextKeys = sourceContextKeys(contexts);
    if (!preserve) {
        const activeKey = contextKeys[resolveActiveSourceIndex(contexts, activeIndex)];
        return {
            notesOpen: contextKeys.length === 0,
            sourceKeys: activeKey === undefined ? [] : [activeKey],
        };
    }

    const previousKeys = new Set(Array.isArray(previous?.sourceKeys) ? previous.sourceKeys : []);
    return {
        notesOpen: previous?.notesOpen === true,
        sourceKeys: contextKeys.filter((key) => previousKeys.has(key)),
    };
}

export function sourceContextRows(context) {
    if (context.kind === "diff") {
        return context.diffRows.map((row) => ({
            ...row,
            lineNumber: row.newLineNumber ?? row.oldLineNumber,
        }));
    }
    return context.lines.map((text, index) => ({lineNumber: context.startLine + index, text}));
}

export function sourceContextRowIsFocused(context, lineNumber) {
    return context.kind !== "diff"
        && context.expanded === true
        && Number.isInteger(lineNumber)
        && lineNumber >= context.focusStartLine
        && lineNumber <= context.focusEndLine;
}

export function resetSourceContextView(context) {
    if (context.kind === "diff") return context;
    const baseStartLine = Math.max(context.startLine, context.baseStartLine);
    const baseEndLine = Math.min(context.endLine, Math.max(baseStartLine, context.baseEndLine));
    const offset = baseStartLine - context.startLine;
    return {
        ...context,
        startLine: baseStartLine,
        endLine: baseEndLine,
        hasMoreAbove: !context.unavailable && baseStartLine > 1,
        hasMoreBelow: !context.unavailable && baseEndLine < context.totalLineCount,
        expanded: false,
        lines: context.lines.slice(offset, offset + baseEndLine - baseStartLine + 1),
        truncated: context.focusEndLine > baseEndLine,
    };
}

function positiveInteger(value, fallback) {
    return Number.isInteger(value) && value > 0 ? value : fallback;
}

function stringValue(value) {
    return typeof value === "string" ? value : "";
}

function normalizedDiffRows(value) {
    if (!Array.isArray(value)) return undefined;
    const rows = [];
    for (const item of value) {
        if (!item || typeof item !== "object" || typeof item.text !== "string") return undefined;
        const kind = item.kind;
        const oldLineNumber = optionalPositiveInteger(item.oldLineNumber);
        const newLineNumber = optionalPositiveInteger(item.newLineNumber);
        if ((kind === "context" && (oldLineNumber === undefined || newLineNumber === undefined))
            || (kind === "inserted" && newLineNumber === undefined)
            || (kind === "removed" && oldLineNumber === undefined)
            || (kind !== "context" && kind !== "inserted" && kind !== "removed")) return undefined;
        rows.push({kind, oldLineNumber, newLineNumber, text: item.text});
    }
    return rows;
}

function optionalPositiveInteger(value) {
    return Number.isInteger(value) && value > 0 ? value : undefined;
}
