export type UnifiedDiffRowKind = "context" | "inserted" | "removed";

export type UnifiedDiffRow = {
    kind: UnifiedDiffRowKind;
    oldLineNumber?: number;
    newLineNumber?: number;
    text: string;
};

type ParsedHunk = {
    newStart: number;
    newEndCursor: number;
    oldEndCursor: number;
    rows: Array<UnifiedDiffRow & {anchor: number}>;
};

const HUNK_HEADER = /^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/;

export function unifiedDiffRowsForRange(
    patch: string,
    headSource: string,
    startLine: number,
    endLine: number,
): UnifiedDiffRow[] {
    const headLines = sourceLines(headSource);
    if (!Number.isInteger(startLine)
        || !Number.isInteger(endLine)
        || startLine < 1
        || endLine < startLine
        || endLine > headLines.length) return [];

    const hunks = parseHunks(patch);
    if (!hunks.length) return [];
    const currentRows = new Map<number, UnifiedDiffRow>();
    const removedRows = new Map<number, UnifiedDiffRow[]>();
    for (const hunk of hunks) {
        for (const row of hunk.rows) {
            if (row.kind === "removed") {
                const entries = removedRows.get(row.anchor);
                if (entries) entries.push(row);
                else removedRows.set(row.anchor, [row]);
            } else if (row.newLineNumber !== undefined) {
                currentRows.set(row.newLineNumber, row);
            }
        }
    }

    const rows: UnifiedDiffRow[] = [];
    let changed = false;
    for (let lineNumber = startLine; lineNumber <= endLine; lineNumber += 1) {
        const removed = removedRows.get(lineNumber) ?? [];
        if (removed.length) changed = true;
        rows.push(...removed.map(stripAnchor));

        const parsed = currentRows.get(lineNumber);
        if (parsed) {
            if (parsed.kind === "inserted") changed = true;
            rows.push(stripAnchor(parsed));
            continue;
        }
        rows.push({
            kind: "context",
            oldLineNumber: oldLineForCurrentLine(hunks, lineNumber),
            newLineNumber: lineNumber,
            text: headLines[lineNumber - 1],
        });
    }

    if (!changed) {
        const trailingRemoval = removedRows.get(endLine + 1) ?? [];
        if (trailingRemoval.length) {
            rows.push(...trailingRemoval.map(stripAnchor));
            changed = true;
        }
    }
    return changed ? rows : [];
}

export function addedFileRowsForRange(
    headSource: string,
    startLine: number,
    endLine: number,
): UnifiedDiffRow[] {
    const headLines = sourceLines(headSource);
    if (!Number.isInteger(startLine)
        || !Number.isInteger(endLine)
        || startLine < 1
        || endLine < startLine
        || endLine > headLines.length) return [];

    return headLines.slice(startLine - 1, endLine).map((text, index) => ({
        kind: "inserted",
        oldLineNumber: undefined,
        newLineNumber: startLine + index,
        text,
    }));
}

export function normalizedGitSource(value: string): string {
    return value.replace(/\r\n?/g, "\n");
}

function sourceLines(value: string): string[] {
    return normalizedGitSource(value).split("\n");
}

function parseHunks(patch: string): ParsedHunk[] {
    const lines = normalizedGitSource(patch).split("\n");
    const hunks: ParsedHunk[] = [];
    let hunk: ParsedHunk | undefined;
    let oldLine = 0;
    let newLine = 0;
    for (const line of lines) {
        const header = HUNK_HEADER.exec(line);
        if (header) {
            oldLine = Number(header[1]);
            newLine = Number(header[3]);
            hunk = {newStart: newLine, newEndCursor: newLine, oldEndCursor: oldLine, rows: []};
            hunks.push(hunk);
            continue;
        }
        if (!hunk || line.startsWith("\\ No newline at end of file")) continue;
        const marker = line[0];
        const text = line.slice(1);
        if (marker === " ") {
            hunk.rows.push({
                kind: "context",
                oldLineNumber: oldLine,
                newLineNumber: newLine,
                text,
                anchor: newLine,
            });
            oldLine += 1;
            newLine += 1;
        } else if (marker === "-") {
            hunk.rows.push({kind: "removed", oldLineNumber: oldLine, text, anchor: newLine});
            oldLine += 1;
        } else if (marker === "+") {
            hunk.rows.push({kind: "inserted", newLineNumber: newLine, text, anchor: newLine});
            newLine += 1;
        }
        hunk.oldEndCursor = oldLine;
        hunk.newEndCursor = newLine;
    }
    return hunks;
}

function oldLineForCurrentLine(hunks: readonly ParsedHunk[], lineNumber: number): number {
    let offset = 0;
    for (const hunk of hunks) {
        if (lineNumber < hunk.newStart) break;
        if (lineNumber < hunk.newEndCursor) {
            const row = hunk.rows.find((candidate) => candidate.newLineNumber === lineNumber);
            if (row?.oldLineNumber !== undefined) return row.oldLineNumber;
        }
        offset = hunk.oldEndCursor - hunk.newEndCursor;
    }
    return lineNumber + offset;
}

function stripAnchor(row: UnifiedDiffRow & {anchor?: number}): UnifiedDiffRow {
    return {
        kind: row.kind,
        oldLineNumber: row.oldLineNumber,
        newLineNumber: row.newLineNumber,
        text: row.text,
    };
}
