import * as vscode from "vscode";
import type {SourceRange} from "./visualizationManifest";

export type LocatedSymbolRanges = {
    range: vscode.Range;
    selectionRange: vscode.Range;
};

export function explicitSourceRange(
    document: vscode.TextDocument,
    sourceRange: SourceRange,
): vscode.Range | undefined {
    const startLine = sourceRange.startLine - 1;
    const endLine = sourceRange.endLine - 1;
    if (startLine < 0 || endLine < startLine || endLine >= document.lineCount) return undefined;
    const startText = document.lineAt(startLine).text;
    const endText = document.lineAt(endLine).text;
    const startCharacter = sourceRange.startColumn === undefined ? 0 : sourceRange.startColumn - 1;
    const endCharacter = sourceRange.endColumn === undefined ? endText.length : sourceRange.endColumn - 1;
    if (startCharacter < 0 || startCharacter > startText.length
        || endCharacter < 0 || endCharacter > endText.length) return undefined;
    if (startLine === endLine && endCharacter <= startCharacter) return undefined;
    return new vscode.Range(startLine, startCharacter, endLine, endCharacter);
}

export async function findSymbolRange(
    document: vscode.TextDocument,
    symbol: string,
    line?: number,
    column?: number,
): Promise<vscode.Range | undefined> {
    return (await findSymbolRanges(document, symbol, line, column))?.selectionRange;
}

export async function findSymbolRanges(
    document: vscode.TextDocument,
    symbol: string,
    line?: number,
    column?: number,
): Promise<LocatedSymbolRanges | undefined> {
    if (line !== undefined) return duplicateRange(rangeOnLine(document, line, symbol, column));

    let symbols: Array<vscode.DocumentSymbol | vscode.SymbolInformation> | undefined;
    try {
        symbols = await vscode.commands.executeCommand<Array<vscode.DocumentSymbol | vscode.SymbolInformation>>(
            "vscode.executeDocumentSymbolProvider",
            document.uri,
        );
    } catch {
        symbols = undefined;
    }
    const matches = flattenSymbols(symbols ?? [], document.uri)
        .filter((candidate) => candidate.uri.toString() === document.uri.toString() && namesMatch(candidate.name, symbol));
    if (matches.length) return {range: matches[0].range, selectionRange: matches[0].selectionRange};
    return duplicateRange(fallbackRange(document, symbol));
}

function flattenSymbols(symbols: Array<vscode.DocumentSymbol | vscode.SymbolInformation>, documentUri: vscode.Uri): Array<{
    name: string;
    range: vscode.Range;
    selectionRange: vscode.Range;
    uri: vscode.Uri;
}> {
    const flattened: Array<{name: string; range: vscode.Range; selectionRange: vscode.Range; uri: vscode.Uri}> = [];
    const visit = (symbol: vscode.DocumentSymbol | vscode.SymbolInformation): void => {
        if ("location" in symbol) {
            flattened.push({
                name: symbol.name,
                range: symbol.location.range,
                selectionRange: symbol.location.range,
                uri: symbol.location.uri,
            });
            return;
        }
        flattened.push({
            name: symbol.name,
            range: symbol.range,
            selectionRange: symbol.selectionRange,
            uri: documentUri,
        });
        for (const child of symbol.children) visit(child);
    };
    for (const symbol of symbols) visit(symbol);
    return flattened;
}

function duplicateRange(range: vscode.Range | undefined): LocatedSymbolRanges | undefined {
    return range ? {range, selectionRange: range} : undefined;
}

function rangeOnLine(document: vscode.TextDocument, oneBasedLine: number, symbol: string, column?: number): vscode.Range | undefined {
    const lineNumber = oneBasedLine - 1;
    if (lineNumber < 0 || lineNumber >= document.lineCount) return undefined;
    const textLine = document.lineAt(lineNumber);
    if (column !== undefined) {
        const character = Math.max(0, Math.min(textLine.text.length, column - 1));
        return new vscode.Range(lineNumber, character, lineNumber, character);
    }
    const leaf = symbolLeaf(symbol);
    const index = textLine.text.indexOf(leaf);
    if (index >= 0) return new vscode.Range(lineNumber, index, lineNumber, index + leaf.length);
    return new vscode.Range(lineNumber, textLine.firstNonWhitespaceCharacterIndex, lineNumber, textLine.text.length);
}

function fallbackRange(document: vscode.TextDocument, symbol: string): vscode.Range | undefined {
    const leaf = symbolLeaf(symbol);
    const pattern = new RegExp(`\\b${escapeRegExp(leaf)}\\b`);
    let best: {range: vscode.Range; score: number} | undefined;
    for (let lineNumber = 0; lineNumber < document.lineCount; lineNumber++) {
        const text = document.lineAt(lineNumber).text;
        const match = pattern.exec(text);
        if (!match) continue;
        const after = text.slice(match.index + leaf.length);
        const before = text.slice(0, match.index);
        if (!/^\s*[(:]/.test(after) && !/\bfunction\s+$/.test(before)) continue;
        let score = 0;
        if (/\bfunction\s+/.test(before)) score += 5;
        if (before.trimEnd().endsWith("::")) score += 4;
        if (text.includes("{")) score += 2;
        if (/;\s*(?:\/\/.*)?$/.test(text)) score -= 5;
        if (/(?:\.|->)\s*$/.test(before) || /\b(?:if|for|while|switch|return)\s*$/.test(before)) score -= 6;
        const range = new vscode.Range(lineNumber, match.index, lineNumber, match.index + leaf.length);
        if (!best || score > best.score) best = {range, score};
    }
    return best?.range;
}

function namesMatch(candidate: string, requested: string): boolean {
    const normalizedCandidate = stripSignature(candidate);
    const normalizedRequested = stripSignature(requested);
    if (normalizedCandidate === normalizedRequested) return true;
    return symbolLeaf(normalizedCandidate) === symbolLeaf(normalizedRequested);
}

function stripSignature(value: string): string {
    return value.trim().replace(/\s*\(.*$/, "").replace(/\s+/g, "");
}

function symbolLeaf(value: string): string {
    return stripSignature(value).split(/::|[.:]/).pop() ?? value;
}

function escapeRegExp(value: string): string {
    return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
