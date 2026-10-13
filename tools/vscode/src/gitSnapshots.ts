import * as path from "node:path";
import * as vscode from "vscode";
import {
    addedFileRowsForRange,
    normalizedGitSource,
    unifiedDiffRowsForRange,
    type UnifiedDiffRow,
} from "./unifiedDiff";
import type {SourceRange} from "./visualizationManifest";

export type GitDiffReference = {
    base: string;
    head: string;
    oldUri: vscode.Uri;
};

export type VerifiedGitDiff = {
    oldPath: string;
    rows: UnifiedDiffRow[];
};

type GitRepository = {
    rootUri: vscode.Uri;
    state: {HEAD?: {commit?: string}};
    getCommit(ref: string): Promise<{hash: string}>;
    getObjectDetails(ref: string, filePath: string): Promise<{object: string}>;
    show(ref: string, filePath: string): Promise<string>;
    diffBlobs(previous: string, current: string): Promise<string>;
};

type GitApi = {
    getRepository(uri: vscode.Uri): GitRepository | null;
    toGitUri(uri: vscode.Uri, ref: string): vscode.Uri;
};

type GitExtension = {
    getAPI(version: 1): GitApi;
};

export async function verifiedGitDiff(
    document: vscode.TextDocument,
    diff: GitDiffReference,
    range: SourceRange,
): Promise<VerifiedGitDiff | undefined> {
    const resolved = await verifiedSnapshot(document, diff);
    if (!resolved) return undefined;
    const rows = resolved.added
        ? addedFileRowsForRange(resolved.headSource, range.startLine, range.endLine)
        : unifiedDiffRowsForRange(
            resolved.patch,
            resolved.headSource,
            range.startLine,
            range.endLine,
        );
    if (!rows.length) return undefined;
    return {
        oldPath: vscode.workspace.asRelativePath(diff.oldUri, false),
        rows,
    };
}

export async function openVerifiedGitDiff(
    document: vscode.TextDocument,
    diff: GitDiffReference,
    range: SourceRange,
    title: string,
    viewColumn: vscode.ViewColumn,
    current: () => boolean,
): Promise<boolean> {
    const resolved = await verifiedSnapshot(document, diff);
    if (!resolved || !current()) return false;
    const rows = resolved.added
        ? addedFileRowsForRange(resolved.headSource, range.startLine, range.endLine)
        : unifiedDiffRowsForRange(
            resolved.patch,
            resolved.headSource,
            range.startLine,
            range.endLine,
        );
    if (!rows.length) return false;

    const left = resolved.added
        ? (await vscode.workspace.openTextDocument({content: "", language: document.languageId})).uri
        : resolved.api.toGitUri(diff.oldUri, diff.base);
    await vscode.commands.executeCommand(
        "vscode.diff",
        left,
        document.uri,
        `${title} (${diff.base.slice(0, 7)} → ${diff.head.slice(0, 7)})`,
        {preview: false, viewColumn},
    );
    if (!current()) return false;
    const focus = new vscode.Position(range.startLine - 1, Math.max(0, (range.startColumn ?? 1) - 1));
    const editor = vscode.window.visibleTextEditors.find(
        (candidate) => candidate.document.uri.toString() === document.uri.toString()
            && candidate.viewColumn === viewColumn,
    ) ?? vscode.window.visibleTextEditors.find(
        (candidate) => candidate.document.uri.toString() === document.uri.toString(),
    );
    if (editor) {
        editor.selection = new vscode.Selection(focus, focus);
        const endLine = Math.min(document.lineCount - 1, range.endLine - 1);
        editor.revealRange(new vscode.Range(focus, document.lineAt(endLine).range.end), vscode.TextEditorRevealType.InCenter);
    }
    return true;
}

async function verifiedSnapshot(
    document: vscode.TextDocument,
    diff: GitDiffReference,
): Promise<{api: GitApi; headSource: string; patch: string; added: boolean} | undefined> {
    try {
        const api = await gitApi();
        const repository = api?.getRepository(document.uri);
        if (!api || !repository || repository.state.HEAD?.commit?.toLowerCase() !== diff.head) return undefined;
        const currentPath = repositoryPath(repository.rootUri, document.uri);
        const oldPath = repositoryPath(repository.rootUri, diff.oldUri);
        if (!currentPath || !oldPath) return undefined;
        const [baseCommit, headSource] = await Promise.all([
            repository.getCommit(diff.base),
            repository.show(diff.head, document.uri.fsPath),
        ]);
        if (baseCommit.hash.toLowerCase() !== diff.base
            || normalizedGitSource(document.getText()) !== normalizedGitSource(headSource)) return undefined;
        try {
            await repository.getObjectDetails(diff.base, diff.oldUri.fsPath);
            const patch = await repository.diffBlobs(
                `${diff.base}:${oldPath}`,
                `${diff.head}:${currentPath}`,
            );
            return {api, headSource, patch, added: false};
        } catch (error) {
            if (!isUnknownGitPath(error)
                || diff.oldUri.toString() !== document.uri.toString()) return undefined;
            return {api, headSource, patch: "", added: true};
        }
    } catch {
        return undefined;
    }
}

function isUnknownGitPath(error: unknown): boolean {
    return typeof error === "object"
        && error !== null
        && (error as {gitErrorCode?: unknown}).gitErrorCode === "UnknownPath";
}

async function gitApi(): Promise<GitApi | undefined> {
    const extension = vscode.extensions.getExtension<GitExtension>("vscode.git");
    if (!extension) return undefined;
    const exports = extension.isActive ? extension.exports : await extension.activate();
    return exports?.getAPI(1);
}

function repositoryPath(root: vscode.Uri, uri: vscode.Uri): string | undefined {
    if (root.scheme !== "file" || uri.scheme !== "file") return undefined;
    const relative = path.relative(root.fsPath, uri.fsPath).replaceAll("\\", "/");
    if (!relative || relative === ".." || relative.startsWith("../") || path.isAbsolute(relative)) return undefined;
    return relative;
}
