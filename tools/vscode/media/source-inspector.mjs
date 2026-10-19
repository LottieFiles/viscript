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

// The extension gives each source context an ID unique within the Scene, which keys its open state.
export function reconcileInspectorOpenState(contexts, previous, activeIndex, preserve) {
    if (!preserve) {
        // A new Scene opens with its notes and its active source link, so the explanation is visible.
        const active = contexts[resolveActiveSourceIndex(contexts, activeIndex)];
        return {
            notesOpen: true,
            sourceKeys: active ? [active.id] : [],
        };
    }

    const previousKeys = new Set(Array.isArray(previous?.sourceKeys) ? previous.sourceKeys : []);
    return {
        notesOpen: previous?.notesOpen === true,
        sourceKeys: contexts.map((context) => context.id).filter((key) => previousKeys.has(key)),
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
