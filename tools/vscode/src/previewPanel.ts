import * as path from "node:path";
import * as vscode from "vscode";
import {openVerifiedGitDiff, verifiedGitDiff, type GitDiffReference} from "./gitSnapshots";
import {explicitSourceRange, findSymbolRanges, type LocatedSymbolRanges} from "./symbolLocator";
import {previewUriCandidate} from "./previewTarget";
import type {SourceRange} from "./visualizationManifest";
import type {UnifiedDiffRow} from "./unifiedDiff";

type AssetPayload = {
    name: string;
    mime: string;
    base64: string;
};

type ScenePayload = {
    type: "scene";
    sceneId: number;
    fileName: string;
    displayName: string;
    source: string;
    version: number;
    assets: AssetPayload[];
    assetErrors: string[];
    relatedSources: string[];
    sourceContexts: SourceContextPayload[];
    activeSourceIndex: number;
    series?: {
        activeGroupIndex: number;
        groups: Array<{
            label: string;
            activeIndex: number;
            items: string[];
        }>;
    };
    description: string;
};

type SourceContextPayload = {
    kind: "source" | "diff";
    id: string;
    label: string;
    path: string;
    language: string;
    startLine: number;
    endLine: number;
    focusStartLine: number;
    focusEndLine: number;
    totalLineCount: number;
    hasMoreAbove: boolean;
    hasMoreBelow: boolean;
    baseStartLine: number;
    baseEndLine: number;
    expanded: boolean;
    lines: string[];
    truncated: boolean;
    unavailable: boolean;
    oldPath?: string;
    diffRows?: UnifiedDiffRow[];
};

export type PreviewSourceReference = {
    label: string;
    uri: vscode.Uri;
    line?: number;
    column?: number;
    symbol?: string;
    range?: SourceRange;
    diff?: GitDiffReference;
};

export type PreviewContextBase = {
    title: string;
    primarySource: PreviewSourceReference;
    relatedSources: PreviewSourceReference[];
    sourceColumn?: vscode.ViewColumn;
    description?: string;
};

export type PreviewSeriesItem = {
    label: string;
    uri: vscode.Uri;
    context: PreviewContextBase;
};

export type PreviewSeriesGroup = {
    label: string;
    activeIndex: number;
    items: PreviewSeriesItem[];
};

export type PreviewSeries = {
    activeGroupIndex: number;
    groups: PreviewSeriesGroup[];
};

export type PreviewContext = PreviewContextBase & {
    series?: PreviewSeries;
};

export type PreviewContextResolver = (
    document: vscode.TextDocument,
    sourceColumn?: vscode.ViewColumn,
) => Promise<PreviewContext | undefined>;

export type PreviewExportFormat = "png" | "gif" | "mp4";

export type PreviewSample = Readonly<{
    position: Readonly<{x: number; y: number}>;
    value: Readonly<{x: number; y: number}>;
    rgba: readonly [number, number, number, number];
    object: Readonly<{handle: number; id: string | null; type: string}>;
}>;

type SourceOpenOptions = {
    current?: () => boolean;
    preservePreview?: boolean;
};

const ASSET_PATTERN = /\b(?:asset|texture)\s*=\s*(["'])(.*?)\1/g;
const MAX_ASSET_BYTES = 32 * 1024 * 1024;
const MAX_SOURCE_CONTEXT_LINES = 240;
const SOURCE_CONTEXT_LINE_COUNTS = [5, 10, 20] as const;
type SourceContextLineCount = typeof SOURCE_CONTEXT_LINE_COUNTS[number];
const DEFAULT_SOURCE_CONTEXT_LINE_COUNT: SourceContextLineCount = 5;
const AUTO_PLAY_NEXT_STATE_KEY = "autoPlayNext";
const FORCE_COMPACT_LAYOUT_STATE_KEY = "forceCompactLayout";
const SOURCE_CONTEXT_LINE_COUNT_STATE_KEY = "sourceContextLineCount";

function isSourceContextLineCount(value: unknown): value is SourceContextLineCount {
    return SOURCE_CONTEXT_LINE_COUNTS.includes(value as SourceContextLineCount);
}

function sourceContextLineCount(value: unknown): SourceContextLineCount {
    return isSourceContextLineCount(value) ? value : DEFAULT_SOURCE_CONTEXT_LINE_COUNT;
}

function previewSample(value: unknown): PreviewSample | undefined {
    if (!value || typeof value !== "object") return undefined;
    const sample = value as Record<string, unknown>;
    const position = sample.position as Record<string, unknown> | undefined;
    const normalized = sample.value as Record<string, unknown> | undefined;
    const rgba = sample.rgba;
    const object = sample.object as Record<string, unknown> | undefined;
    if (!position || !normalized || !Array.isArray(rgba) || !object
        || !Number.isFinite(position.x) || !Number.isFinite(position.y)
        || !Number.isFinite(normalized.x) || (normalized.x as number) < 0
        || (normalized.x as number) > 1
        || !Number.isFinite(normalized.y) || (normalized.y as number) < 0
        || (normalized.y as number) > 1
        || rgba.length !== 4
        || !rgba.every((channel) => Number.isInteger(channel) && channel >= 0 && channel <= 255)
        || !Number.isInteger(object.handle) || (object.handle as number) < 0
        || (object.handle as number) > 0xffffffff
        || (object.id !== null && typeof object.id !== "string")
        || typeof object.type !== "string") {
        return undefined;
    }
    return {
        position: {x: position.x as number, y: position.y as number},
        value: {x: normalized.x as number, y: normalized.y as number},
        rgba: rgba as [number, number, number, number],
        object: {
            handle: object.handle as number,
            id: object.id as string | null,
            type: object.type,
        },
    };
}

export class PreviewPanel implements vscode.Disposable {
    static current: PreviewPanel | undefined;

    static follows(uri: vscode.Uri): boolean {
        return PreviewPanel.current?.document?.uri.toString() === uri.toString();
    }

    static currentSample(): PreviewSample | undefined {
        return PreviewPanel.current?.lastSample;
    }

    private readonly disposables: vscode.Disposable[] = [];
    private document: vscode.TextDocument | undefined;
    private ready = false;
    private queuedPayload: ScenePayload | undefined;
    private updateTimer: NodeJS.Timeout | undefined;
    private sourceContextUpdateTimer: NodeJS.Timeout | undefined;
    private version = 0;
    private sourceContextUpdateGeneration = 0;
    private readonly sourceContextExpansion = new Map<string, {above: number; below: number}>();
    private sceneId = 0;
    private setDocumentGeneration = 0;
    private sourceOpenGeneration = 0;
    private sourceShowQueue: Promise<void> = Promise.resolve();
    private seriesOpenGeneration = 0;
    private sourceRevealUri: string | undefined;
    private sourceColumn: vscode.ViewColumn | undefined;
    private previewContext: PreviewContext | undefined;
    private seriesCatalog: PreviewSeries | undefined;
    private lastSample: PreviewSample | undefined;

    private constructor(
        private readonly panel: vscode.WebviewPanel,
        private readonly context: vscode.ExtensionContext,
        private readonly diagnostics: vscode.DiagnosticCollection,
        private contextResolver?: PreviewContextResolver,
    ) {
        PreviewPanel.current = this;
        void vscode.commands.executeCommand("setContext", "visPreview.hasPreview", true);
        panel.iconPath = vscode.Uri.joinPath(context.extensionUri, "media", "icon.svg");
        panel.webview.html = this.html(panel.webview);

        panel.onDidDispose(() => this.dispose(), null, this.disposables);
        panel.webview.onDidReceiveMessage((message: unknown) => this.onMessage(message), null, this.disposables);
        vscode.workspace.onDidChangeTextDocument(
            (event) => {
                if (event.document === this.document) {
                    this.scheduleUpdate();
                    return;
                }
                if (this.previewContext?.relatedSources.some(
                    (source) => source.uri.toString() === event.document.uri.toString(),
                )) {
                    this.scheduleSourceContextUpdate();
                }
            },
            null,
            this.disposables,
        );
        vscode.workspace.onDidCloseTextDocument(
            (closed) => {
                if (closed === this.document) this.document = undefined;
            },
            null,
            this.disposables,
        );
        vscode.window.onDidChangeActiveTextEditor(
            (editor) => {
                if (editor && isLuaDocument(editor.document)) void this.followDocument(editor.document, editor.viewColumn);
            },
            null,
            this.disposables,
        );
        vscode.window.tabGroups.onDidChangeTabs(
            () => void this.followActiveTab(),
            null,
            this.disposables,
        );
        vscode.window.tabGroups.onDidChangeTabGroups(
            () => void this.followActiveTab(),
            null,
            this.disposables,
        );
    }

    static async open(
        context: vscode.ExtensionContext,
        diagnostics: vscode.DiagnosticCollection,
        uri: unknown,
        column: vscode.ViewColumn,
        previewContext?: PreviewContext,
        preserveFocus?: boolean,
        contextResolver?: PreviewContextResolver,
    ): Promise<void> {
        const document = await this.resolveDocument(uri);
        if (!document) return;

        if (PreviewPanel.current) {
            PreviewPanel.current.contextResolver = contextResolver ?? PreviewPanel.current.contextResolver;
            PreviewPanel.current.panel.reveal(column, preserveFocus ?? true);
            await PreviewPanel.current.setDocument(document, previewContext);
            return;
        }

        const preview = this.createPanel(
            context,
            diagnostics,
            column,
            `viscript — ${path.basename(document.fileName)}`,
            preserveFocus,
            contextResolver,
        );
        await preview.setDocument(document, previewContext);
    }

    static async openViewer(
        context: vscode.ExtensionContext,
        diagnostics: vscode.DiagnosticCollection,
        column: vscode.ViewColumn,
        contextResolver?: PreviewContextResolver,
        series?: PreviewSeries,
    ): Promise<void> {
        if (PreviewPanel.current) {
            PreviewPanel.current.contextResolver = contextResolver ?? PreviewPanel.current.contextResolver;
            PreviewPanel.current.panel.reveal(column);
            if (!PreviewPanel.current.document) await PreviewPanel.current.setSeriesCatalog(series);
            return;
        }
        const preview = this.createPanel(context, diagnostics, column, "viscript Viewer", false, contextResolver);
        await preview.setSeriesCatalog(series);
    }

    private static createPanel(
        context: vscode.ExtensionContext,
        diagnostics: vscode.DiagnosticCollection,
        column: vscode.ViewColumn,
        title: string,
        preserveFocus = false,
        contextResolver?: PreviewContextResolver,
    ): PreviewPanel {
        const panel = vscode.window.createWebviewPanel(
            "visPreview",
            title,
            preserveFocus ? {viewColumn: column, preserveFocus} : column,
            {
                enableScripts: true,
                retainContextWhenHidden: true,
                localResourceRoots: [
                    vscode.Uri.joinPath(context.extensionUri, "media"),
                    vscode.Uri.joinPath(context.extensionUri, "runtime"),
                ],
            },
        );
        return new PreviewPanel(panel, context, diagnostics, contextResolver);
    }

    static async saveCurrent(format: PreviewExportFormat): Promise<void> {
        const preview = PreviewPanel.current;
        if (!preview || !preview.document) {
            void vscode.window.showWarningMessage("Open a viscript Preview before saving media.");
            return;
        }
        if (!preview.ready) {
            void vscode.window.showInformationMessage("viscript Preview is still loading.");
            return;
        }
        let fps: number | undefined;
        if (format !== "png") {
            const selected = await vscode.window.showQuickPick(["24", "30", "60"], {
                title: `Save current viscript viewer as ${format.toUpperCase()}`,
                placeHolder: "Frames per second",
            });
            if (!selected) return;
            fps = Number(selected);
        }
        await preview.panel.webview.postMessage({type: "export", format, fps});
    }

    private static async resolveDocument(uri?: unknown): Promise<vscode.TextDocument | undefined> {
        const active = vscode.window.activeTextEditor?.document;
        const candidate = previewUriCandidate(uri, active?.uri);
        let document: vscode.TextDocument | undefined;
        if (candidate) {
            const target = vscode.Uri.from(candidate);
            if (active?.uri.toString() === target.toString()) document = active;
            else {
                try {
                    document = await vscode.workspace.openTextDocument(target);
                } catch {
                    document = undefined;
                }
            }
        }
        if (!document && active && isLuaDocument(active)) document = active;

        if (!document || !isLuaDocument(document)) {
            const selection = await vscode.window.showOpenDialog({
                canSelectMany: false,
                openLabel: "Preview viscript Scene",
                filters: {"Lua scenes": ["lua"]},
            });
            if (!selection?.[0]) return undefined;
            try {
                document = await vscode.workspace.openTextDocument(selection[0]);
            } catch {
                return undefined;
            }
        }
        return document;
    }

    async setDocument(document: vscode.TextDocument, previewContext?: PreviewContext): Promise<void> {
        const generation = ++this.setDocumentGeneration;
        this.sourceOpenGeneration += 1;
        this.sourceRevealUri = undefined;
        const changed = this.document?.uri.toString() !== document.uri.toString();
        if (this.document && changed) {
            this.diagnostics.delete(this.document.uri);
        }
        if (changed) {
            this.sceneId += 1;
            this.lastSample = undefined;
        }
        this.document = document;
        const sourceColumn = previewContext?.sourceColumn
            ?? vscode.window.visibleTextEditors.find((editor) => editor.document === document)?.viewColumn
            ?? this.sourceColumn;
        this.sourceColumn = sourceColumn;
        const resolvedContext = previewContext ?? await this.contextResolver?.(document, sourceColumn);
        if (generation !== this.setDocumentGeneration) return;
        if (changed || resolvedContext !== this.previewContext) this.sourceContextExpansion.clear();
        this.previewContext = resolvedContext;
        this.seriesCatalog = resolvedContext?.series;
        this.sourceColumn = resolvedContext?.sourceColumn ?? sourceColumn;
        this.panel.title = `viscript — ${resolvedContext?.title ?? path.basename(document.fileName)}`;
        await this.update();
    }

    private webviewSeries(): ScenePayload["series"] {
        if (!this.seriesCatalog) return undefined;
        return {
            activeGroupIndex: this.seriesCatalog.activeGroupIndex,
            groups: this.seriesCatalog.groups.map((group) => ({
                label: group.label,
                activeIndex: group.activeIndex,
                items: group.items.map((item) => item.label),
            })),
        };
    }

    private async setSeriesCatalog(series: PreviewSeries | undefined): Promise<void> {
        this.seriesCatalog = series;
        if (this.ready) await this.panel.webview.postMessage({type: "series", series: this.webviewSeries()});
    }

    private async followDocument(document: vscode.TextDocument, column?: vscode.ViewColumn): Promise<void> {
        if (this.sourceRevealUri === document.uri.toString()) return;
        this.sourceRevealUri = undefined;
        if (this.document?.uri.toString() === document.uri.toString()) {
            this.sourceColumn = column ?? this.sourceColumn;
            return;
        }
        this.sourceColumn = column ?? this.sourceColumn;
        await this.setDocument(document);
    }

    private async followActiveTab(): Promise<void> {
        const input = vscode.window.tabGroups.activeTabGroup.activeTab?.input;
        let uri: vscode.Uri | undefined;
        if (input instanceof vscode.TabInputText) uri = input.uri;
        else if (input instanceof vscode.TabInputTextDiff) uri = input.modified;
        if (!uri || !uri.path.toLowerCase().endsWith(".lua")) return;
        const document = vscode.workspace.textDocuments.find((item) => item.uri.toString() === uri?.toString())
            ?? await vscode.workspace.openTextDocument(uri);
        await this.followDocument(document, vscode.window.tabGroups.activeTabGroup.viewColumn);
    }

    refresh(): void {
        if (this.document) void this.update();
    }

    private scheduleUpdate(): void {
        if (this.updateTimer) clearTimeout(this.updateTimer);
        const delay = vscode.workspace.getConfiguration("visPreview").get<number>("updateDelay", 280);
        this.updateTimer = setTimeout(() => void this.update(), delay);
    }

    private scheduleSourceContextUpdate(): void {
        if (this.sourceContextUpdateTimer) clearTimeout(this.sourceContextUpdateTimer);
        const generation = ++this.sourceContextUpdateGeneration;
        const delay = vscode.workspace.getConfiguration("visPreview").get<number>("updateDelay", 280);
        this.sourceContextUpdateTimer = setTimeout(
            () => void this.updateSourceContexts(generation),
            delay,
        );
    }

    private async updateSourceContexts(generation: number): Promise<void> {
        this.sourceContextUpdateTimer = undefined;
        const document = this.document;
        const previewContext = this.previewContext;
        if (!previewContext) return;
        const sourceContexts = await this.readSourceContexts(previewContext.relatedSources);
        if (generation !== this.sourceContextUpdateGeneration
            || document !== this.document
            || previewContext !== this.previewContext) return;
        const payload = {
            type: "sourceContexts",
            sourceContexts,
            activeSourceIndex: sourceContexts.length ? 0 : -1,
        };
        if (this.ready) await this.panel.webview.postMessage(payload);
        else if (this.queuedPayload) {
            this.queuedPayload.sourceContexts = sourceContexts;
            this.queuedPayload.activeSourceIndex = payload.activeSourceIndex;
        }
    }

    private async update(): Promise<void> {
        const document = this.document;
        if (!document) return;
        if (this.sourceContextUpdateTimer) clearTimeout(this.sourceContextUpdateTimer);
        this.sourceContextUpdateTimer = undefined;
        this.sourceContextUpdateGeneration += 1;
        const version = ++this.version;
        this.lastSample = undefined;
        const source = document.getText();
        const {assets, errors} = await this.readAssets(document.uri, source);
        const sourceContexts = await this.readSourceContexts();
        if (version !== this.version || document !== this.document) return;
        const payload: ScenePayload = {
            type: "scene",
            sceneId: this.sceneId,
            fileName: path.basename(document.fileName),
            displayName: this.previewContext?.title ?? path.basename(document.fileName),
            source,
            version,
            assets,
            assetErrors: errors,
            relatedSources: this.previewContext?.relatedSources.map((item) => item.label) ?? [],
            sourceContexts,
            activeSourceIndex: sourceContexts.length ? 0 : -1,
            series: this.webviewSeries(),
            description: this.previewContext?.description ?? defaultDescription(document),
        };
        if (this.ready) await this.panel.webview.postMessage(payload);
        else this.queuedPayload = payload;
    }

    /** File-backed assets must contain 1 byte..32 MiB; names resolve beside the scene. */
    private async readAssets(uri: vscode.Uri, source: string): Promise<{assets: AssetPayload[]; errors: string[]}> {
        if (uri.scheme !== "file") return {assets: [], errors: []};
        const names = new Set<string>();
        for (const match of source.matchAll(ASSET_PATTERN)) {
            if (match[2] && !match[2].includes("\0")) names.add(match[2]);
        }

        const assets: AssetPayload[] = [];
        const errors: string[] = [];
        const directory = vscode.Uri.file(path.dirname(uri.fsPath));
        for (const name of names) {
            const assetUri = vscode.Uri.joinPath(directory, ...name.replaceAll("\\", "/").split("/"));
            try {
                const bytes = await vscode.workspace.fs.readFile(assetUri);
                if (!bytes.length || bytes.length > MAX_ASSET_BYTES) {
                    errors.push(`${name} must be between 1 byte and 32 MiB`);
                    continue;
                }
                assets.push({
                    name,
                    mime: mimeFor(name),
                    base64: Buffer.from(bytes).toString("base64"),
                });
            } catch {
                errors.push(`Could not read ${name}`);
            }
        }
        return {assets, errors};
    }

    private async readSourceContexts(
        references: readonly PreviewSourceReference[] = this.previewContext?.relatedSources ?? [],
    ): Promise<SourceContextPayload[]> {
        const ids = sourceReferenceIds(references);
        return Promise.all(references.map(async (source, index) => {
            const id = ids[index];
            const displayPath = vscode.workspace.asRelativePath(source.uri, false);
            try {
                const document = await vscode.workspace.openTextDocument(source.uri);
                const location = await sourceLocation(document, source);
                if (source.range && !location) {
                    return {
                        kind: source.diff ? "diff" : "source",
                        id,
                        label: source.label,
                        path: displayPath,
                        language: document.languageId,
                        startLine: source.range.startLine,
                        endLine: source.range.endLine,
                        focusStartLine: source.range.startLine,
                        focusEndLine: source.range.endLine,
                        totalLineCount: document.lineCount,
                        hasMoreAbove: false,
                        hasMoreBelow: false,
                        baseStartLine: source.range.startLine,
                        baseEndLine: source.range.endLine,
                        expanded: false,
                        lines: [],
                        truncated: false,
                        unavailable: true,
                        oldPath: source.diff
                            ? vscode.workspace.asRelativePath(source.diff.oldUri, false)
                            : undefined,
                        diffRows: source.diff ? [] : undefined,
                    };
                }
                if (source.diff) {
                    const diff = source.diff;
                    const range = source.range;
                    const expansion = this.sourceContextExpansion.get(id) ?? {above: 0, below: 0};
                    const startLine = range ? Math.max(1, range.startLine - expansion.above) : 1;
                    const endLine = range
                        ? Math.min(document.lineCount, range.endLine + expansion.below)
                        : 1;
                    const expandedRange = {startLine, endLine};
                    const resolved = range
                        ? await verifiedGitDiff(document, diff, expandedRange)
                        : undefined;
                    if (!range || !resolved) {
                        return {
                            kind: "diff",
                            id,
                            label: source.label,
                            path: displayPath,
                            language: document.languageId,
                            startLine: range?.startLine ?? 1,
                            endLine: range?.endLine ?? 1,
                            focusStartLine: range?.startLine ?? 1,
                            focusEndLine: range?.endLine ?? 1,
                            totalLineCount: document.lineCount,
                            hasMoreAbove: false,
                            hasMoreBelow: false,
                            baseStartLine: range?.startLine ?? 1,
                            baseEndLine: range?.endLine ?? 1,
                            expanded: false,
                            lines: [],
                            truncated: false,
                            unavailable: true,
                            oldPath: vscode.workspace.asRelativePath(diff.oldUri, false),
                            diffRows: [],
                        };
                    }
                    return {
                        kind: "diff",
                        id,
                        label: source.label,
                        path: displayPath,
                        language: document.languageId,
                        startLine,
                        endLine,
                        focusStartLine: range.startLine,
                        focusEndLine: range.endLine,
                        totalLineCount: document.lineCount,
                        hasMoreAbove: startLine > 1,
                        hasMoreBelow: endLine < document.lineCount,
                        baseStartLine: range.startLine,
                        baseEndLine: range.endLine,
                        expanded: expansion.above > 0 || expansion.below > 0,
                        lines: resolved.rows.map((row) => row.text),
                        truncated: false,
                        unavailable: false,
                        oldPath: resolved.oldPath,
                        diffRows: resolved.rows,
                    };
                }
                const focusStartLine = location?.range.start.line ?? 0;
                const focusEndLine = location
                    ? rangeInclusiveEndLine(location.range)
                    : document.lineCount - 1;
                const initialEndLine = Math.min(
                    focusEndLine,
                    focusStartLine + MAX_SOURCE_CONTEXT_LINES - 1,
                );
                const expansion = this.sourceContextExpansion.get(id) ?? {above: 0, below: 0};
                const startLine = Math.max(0, focusStartLine - expansion.above);
                const endLine = Math.min(document.lineCount - 1, initialEndLine + expansion.below);
                const lines: string[] = [];
                for (let line = startLine; line <= endLine && line < document.lineCount; line += 1) {
                    lines.push(document.lineAt(line).text);
                }
                return {
                    kind: "source",
                    id,
                    label: source.label,
                    path: displayPath,
                    language: document.languageId,
                    startLine: startLine + 1,
                    endLine: Math.max(startLine, endLine) + 1,
                    focusStartLine: focusStartLine + 1,
                    focusEndLine: focusEndLine + 1,
                    totalLineCount: document.lineCount,
                    hasMoreAbove: startLine > 0,
                    hasMoreBelow: endLine < document.lineCount - 1,
                    baseStartLine: focusStartLine + 1,
                    baseEndLine: initialEndLine + 1,
                    expanded: expansion.above > 0 || expansion.below > 0,
                    lines,
                    truncated: focusEndLine > endLine,
                    unavailable: false,
                };
            } catch {
                return {
                    kind: source.diff ? "diff" : "source",
                    id,
                    label: source.label,
                    path: displayPath,
                    language: "plaintext",
                    startLine: 1,
                    endLine: 1,
                    focusStartLine: 1,
                    focusEndLine: 1,
                    totalLineCount: 1,
                    hasMoreAbove: false,
                    hasMoreBelow: false,
                    baseStartLine: 1,
                    baseEndLine: 1,
                    expanded: false,
                    lines: [],
                    truncated: false,
                    unavailable: true,
                    oldPath: source.diff
                        ? vscode.workspace.asRelativePath(source.diff.oldUri, false)
                        : undefined,
                    diffRows: source.diff ? [] : undefined,
                };
            }
        }));
    }

    private async expandSourceContext(index: number, id: string, direction: "above" | "below"): Promise<void> {
        const references = this.previewContext?.relatedSources;
        if (!references || index < 0 || index >= references.length) return;
        if (sourceReferenceIds(references)[index] !== id) return;
        const current = this.sourceContextExpansion.get(id) ?? {above: 0, below: 0};
        const step = sourceContextLineCount(
            this.context.globalState.get<number>(SOURCE_CONTEXT_LINE_COUNT_STATE_KEY),
        );
        this.sourceContextExpansion.set(id, {
            above: current.above + (direction === "above" ? step : 0),
            below: current.below + (direction === "below" ? step : 0),
        });
        if (this.sourceContextUpdateTimer) clearTimeout(this.sourceContextUpdateTimer);
        this.sourceContextUpdateTimer = undefined;
        const generation = ++this.sourceContextUpdateGeneration;
        await this.updateSourceContexts(generation);
    }

    private async resetSourceContext(index: number, id: string): Promise<void> {
        const references = this.previewContext?.relatedSources;
        if (!references || index < 0 || index >= references.length) return;
        if (sourceReferenceIds(references)[index] !== id) return;
        if (!this.sourceContextExpansion.delete(id)) return;
        if (this.sourceContextUpdateTimer) clearTimeout(this.sourceContextUpdateTimer);
        this.sourceContextUpdateTimer = undefined;
        const generation = ++this.sourceContextUpdateGeneration;
        await this.updateSourceContexts(generation);
    }

    private async onMessage(raw: unknown): Promise<void> {
        if (!raw || typeof raw !== "object") return;
        const message = raw as Record<string, unknown>;
        if (message.type === "ready") {
            this.ready = true;
            if (this.queuedPayload) {
                const payload = this.queuedPayload;
                this.queuedPayload = undefined;
                await this.panel.webview.postMessage(payload);
            } else if (this.seriesCatalog) {
                await this.panel.webview.postMessage({type: "series", series: this.webviewSeries()});
            }
            return;
        }
        if (message.type === "openSource") {
            const source = this.previewContext?.primarySource
                ?? (this.document ? {label: path.basename(this.document.fileName), uri: this.document.uri} : undefined);
            if (source) await this.openSource(source, {preservePreview: true});
            return;
        }
        if (message.type === "openLua") {
            if (this.document) {
                await this.openSource({label: path.basename(this.document.fileName), uri: this.document.uri});
            }
            return;
        }
        if (message.type === "setAutoPlayNext" && typeof message.enabled === "boolean") {
            await this.context.globalState.update(AUTO_PLAY_NEXT_STATE_KEY, message.enabled);
            return;
        }
        if (message.type === "setForceCompactLayout" && typeof message.enabled === "boolean") {
            await this.context.globalState.update(FORCE_COMPACT_LAYOUT_STATE_KEY, message.enabled);
            return;
        }
        if (message.type === "setSourceContextLineCount" && isSourceContextLineCount(message.count)) {
            await this.context.globalState.update(SOURCE_CONTEXT_LINE_COUNT_STATE_KEY, message.count);
            return;
        }
        if (message.type === "openSeries"
            && Number.isInteger(message.groupIndex)
            && Number.isInteger(message.itemIndex)) {
            await this.openSeries(
                message.groupIndex as number,
                message.itemIndex as number,
            );
            return;
        }
        if (message.type === "openRelatedSource" && Number.isInteger(message.index)) {
            const source = this.previewContext?.relatedSources[message.index as number];
            if (source) await this.openSource(source, {preservePreview: true});
            return;
        }
        if (message.type === "expandSourceContext"
            && message.sceneId === this.sceneId
            && Number.isInteger(message.index)
            && typeof message.id === "string"
            && (message.direction === "above" || message.direction === "below")) {
            await this.expandSourceContext(
                message.index as number,
                message.id,
                message.direction,
            );
            return;
        }
        if (message.type === "resetSourceContext"
            && message.sceneId === this.sceneId
            && Number.isInteger(message.index)
            && typeof message.id === "string") {
            await this.resetSourceContext(message.index as number, message.id);
            return;
        }
        if (message.type === "sample"
            && message.sceneId === this.sceneId
            && message.version === this.version) {
            const sample = previewSample(message.sample);
            if (sample) this.lastSample = sample;
            return;
        }
        if (message.type === "refresh") {
            this.refresh();
            return;
        }
        if (message.type === "renderSuccess") {
            if (this.document) this.diagnostics.delete(this.document.uri);
            return;
        }
        if (message.type === "renderError" && typeof message.message === "string") {
            this.showDiagnostic(message.message);
            return;
        }
        if (message.type === "saveMedia" && typeof message.format === "string" && typeof message.dataUrl === "string") {
            await this.saveMedia(message.format, message.dataUrl);
            return;
        }
        if (message.type === "exportError" && typeof message.message === "string") {
            void vscode.window.showErrorMessage(`viscript export: ${message.message}`);
        }
    }

    private sourceTargetColumn(): vscode.ViewColumn {
        const previewColumn = this.panel.viewColumn;
        if (previewColumn === undefined
            || !Number.isInteger(previewColumn)
            || previewColumn < vscode.ViewColumn.One) return vscode.ViewColumn.Beside;
        const nextColumn = previewColumn + 1;
        const adjacent = vscode.window.tabGroups.all.find((group) => group.viewColumn === nextColumn);
        if (adjacent) return adjacent.viewColumn;
        if (previewColumn >= vscode.ViewColumn.Nine) return vscode.ViewColumn.Beside;
        return nextColumn as vscode.ViewColumn;
    }

    private async openSource(source: PreviewSourceReference, options: SourceOpenOptions = {}): Promise<void> {
        const generation = ++this.sourceOpenGeneration;
        const callerCurrent = options.current ?? (() => true);
        const current = () => generation === this.sourceOpenGeneration && callerCurrent();
        if (!options.preservePreview) this.sourceRevealUri = undefined;
        try {
            const document = await vscode.workspace.openTextDocument(source.uri);
            if (!current()) return;
            const location = await sourceLocation(document, source);
            if (!current()) return;
            if (source.diff && source.range) {
                const diff = source.diff;
                const range = source.range;
                const showDiff = async (): Promise<void> => {
                    if (!current()) return;
                    this.sourceRevealUri = options.preservePreview ? source.uri.toString() : undefined;
                    const opened = await openVerifiedGitDiff(
                        document,
                        diff,
                        range,
                        source.label,
                        options.preservePreview
                            ? this.sourceTargetColumn()
                            : this.sourceColumn ?? vscode.ViewColumn.One,
                        current,
                    );
                    if (!opened && current()) {
                        void vscode.window.showWarningMessage(`Diff unavailable for related source: ${source.label}`);
                    }
                };
                const pending = this.sourceShowQueue.then(showDiff, showDiff);
                this.sourceShowQueue = pending.then(() => undefined, () => undefined);
                await pending;
                return;
            }
            const show = async (): Promise<void> => {
                if (!current()) return;
                this.sourceRevealUri = options.preservePreview ? source.uri.toString() : undefined;
                const editor = await vscode.window.showTextDocument(document, {
                    viewColumn: options.preservePreview
                        ? this.sourceTargetColumn()
                        : this.sourceColumn ?? vscode.ViewColumn.One,
                    preview: false,
                });
                if (!current() || !location) return;
                editor.selection = new vscode.Selection(location.selectionRange.start, location.selectionRange.start);
                editor.revealRange(location.range, vscode.TextEditorRevealType.InCenter);
            };
            const pending = this.sourceShowQueue.then(show, show);
            this.sourceShowQueue = pending.then(() => undefined, () => undefined);
            await pending;
        } catch {
            if (current()) {
                if (this.sourceRevealUri === source.uri.toString()) this.sourceRevealUri = undefined;
                void vscode.window.showErrorMessage(`Could not open related source: ${source.uri.fsPath}`);
            }
        }
    }

    private async openSeries(groupIndex: number, itemIndex: number): Promise<void> {
        const series = this.seriesCatalog;
        if (!series || groupIndex < 0 || groupIndex >= series.groups.length) return;
        const group = series.groups[groupIndex];
        if (itemIndex < 0 || itemIndex >= group.items.length) return;
        const item = group.items[itemIndex];
        this.sourceOpenGeneration += 1;
        this.sourceRevealUri = undefined;
        const generation = ++this.seriesOpenGeneration;
        const documentGeneration = this.setDocumentGeneration;
        let document: vscode.TextDocument;
        try {
            document = await vscode.workspace.openTextDocument(item.uri);
        } catch {
            if (generation === this.seriesOpenGeneration && documentGeneration === this.setDocumentGeneration) {
                void vscode.window.showErrorMessage(`Could not open series animation: ${item.uri.fsPath}`);
            }
            return;
        }
        if (generation !== this.seriesOpenGeneration || documentGeneration !== this.setDocumentGeneration) return;
        const context: PreviewContext = {
            ...item.context,
            sourceColumn: this.sourceColumn ?? item.context.sourceColumn,
            series: {
                activeGroupIndex: groupIndex,
                groups: series.groups.map((candidate, index) => ({
                    ...candidate,
                    activeIndex: index === groupIndex ? itemIndex : -1,
                })),
            },
        };
        await this.setDocument(document, context);
    }

    private showDiagnostic(message: string): void {
        const document = this.document;
        if (!document) return;
        const lineMatch = message.match(/:(\d+):(?:\d+:)?/);
        const line = Math.max(0, Math.min(document.lineCount - 1, Number(lineMatch?.[1] ?? 1) - 1));
        const range = document.lineAt(line).range;
        const diagnostic = new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error);
        diagnostic.source = "viscript Preview";
        this.diagnostics.set(document.uri, [diagnostic]);
    }

    private async saveMedia(format: string, dataUrl: string): Promise<void> {
        const media: {mime: string; label: string} | undefined = ({
            png: {mime: "image/png", label: "PNG"},
            gif: {mime: "image/gif", label: "GIF"},
            mp4: {mime: "video/mp4", label: "MP4"},
        } as Record<string, {mime: string; label: string}>)[format];
        if (!media) return;
        const match = new RegExp(`^data:${media.mime.replace("/", "\\/")};base64,(.+)$`).exec(dataUrl);
        if (!match || !this.document) return;
        const base = path.basename(this.document.fileName, path.extname(this.document.fileName));
        const defaultUri = vscode.Uri.joinPath(vscode.Uri.file(path.dirname(this.document.fileName)), `${base}.${format}`);
        const target = await vscode.window.showSaveDialog({
            defaultUri,
            filters: {[media.label]: [format]},
            saveLabel: `Save viscript ${media.label}`,
        });
        if (!target) return;
        await vscode.workspace.fs.writeFile(target, Buffer.from(match[1], "base64"));
        void vscode.window.showInformationMessage(`Saved ${path.basename(target.fsPath)}`);
    }

    private html(webview: vscode.Webview): string {
        const nonce = randomNonce();
        const styleUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "media", "main.css"));
        const scriptUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "media", "main.js"));
        const clientUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "runtime", "client.js"));
        const wasmUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "runtime", "vis-wasm.wasm"));
        const fontUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "runtime", "Pretendard.ttf"));
        const codeFontUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "runtime", "JetBrainsMono.ttf"));
        const headingFontUri = webview.asWebviewUri(
            vscode.Uri.joinPath(this.context.extensionUri, "runtime", "IBMPlexSansKR-SemiBold.ttf"),
        );
        const serifHeadingFontUri = webview.asWebviewUri(
            vscode.Uri.joinPath(this.context.extensionUri, "runtime", "SourceSerif4-Semibold.ttf"),
        );
        const config = JSON.stringify({
            client: clientUri.toString(),
            wasm: wasmUri.toString(),
            font: fontUri.toString(),
            codeFont: codeFontUri.toString(),
            headingFont: headingFontUri.toString(),
            serifHeadingFont: serifHeadingFontUri.toString(),
            autoPlayNext: this.context.globalState.get<boolean>(AUTO_PLAY_NEXT_STATE_KEY, false),
            forceCompactLayout: this.context.globalState.get<boolean>(FORCE_COMPACT_LAYOUT_STATE_KEY, false),
            sourceContextLineCount: sourceContextLineCount(
                this.context.globalState.get<number>(SOURCE_CONTEXT_LINE_COUNT_STATE_KEY),
            ),
        })
            .replaceAll("<", "\\u003c");
        const csp = [
            "default-src 'none'",
            `style-src ${webview.cspSource} 'nonce-${nonce}'`,
            `script-src 'nonce-${nonce}' ${webview.cspSource} 'wasm-unsafe-eval'`,
            `connect-src ${webview.cspSource}`,
            `font-src ${webview.cspSource}`,
            "img-src data: blob:",
        ].join("; ");

        return `<!doctype html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="Content-Security-Policy" content="${csp}">
    <link rel="stylesheet" href="${styleUri}">
    <style nonce="${nonce}">
        @font-face { font-family: "Pretendard"; src: url("${fontUri}") format("truetype"); font-display: swap; }
        @font-face { font-family: "IBM Plex Sans KR"; src: url("${headingFontUri}") format("truetype"); font-display: swap; }
        @font-face { font-family: "Source Serif 4"; src: url("${serifHeadingFontUri}") format("truetype"); font-display: swap; }
    </style>
    <title>viscript Preview</title>
</head>
<body data-layout="compact">
    <main class="viewer">
        <section class="player" aria-label="viscript animation player">
            <div class="stage-shell" id="stage-shell" aria-label="Rendered viscript canvas">
                <div class="canvas-wrap" id="canvas-wrap" data-fit="true" tabindex="0" role="region" aria-label="Interactive viscript preview canvas">
                    <div class="canvas-viewport" id="canvas-viewport">
                        <canvas id="preview" width="960" height="540"></canvas>
                    </div>
                    <div class="empty" id="empty">
                        <strong>READY FOR A SCENE</strong>
                        <span>Open a viscript Lua file to begin.</span>
                    </div>
                    <div class="error-card" id="error-card" hidden>
                        <span id="error-title">SCENE REJECTED</span>
                        <pre id="error-message"></pre>
                        <button id="error-source" type="button">OPEN SOURCE</button>
                    </div>
                </div>
            </div>
            <div class="stage-resizer" id="stage-resizer" role="separator" aria-label="Resize canvas height" aria-orientation="horizontal" aria-valuemin="64" tabindex="0">
                <span class="stage-resizer-status" aria-hidden="true">
                    <span id="canvas-stat">— × — · — FPS</span>
                </span>
            </div>
            <footer class="transport">
                <div class="seek-row">
                    <div class="primary-controls" aria-label="Playback controls">
                        <button class="icon-button" id="loop" type="button" aria-pressed="false" aria-label="Enable loop" title="Loop off">
                            <svg class="control-icon loop-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="M3 6V3.5h2.5"></path>
                                <path d="M3.4 4.1A5 5 0 0 1 12.6 6"></path>
                                <path d="M13 10v2.5h-2.5"></path>
                                <path d="M12.6 11.9A5 5 0 0 1 3.4 10"></path>
                            </svg>
                        </button>
                        <button class="icon-button play" id="play" type="button" aria-label="Play">
                            <svg class="control-icon play-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="M4.5 2.75v10.5L13 8z"></path>
                            </svg>
                            <svg class="control-icon pause-icon" viewBox="0 0 16 16" aria-hidden="true" hidden>
                                <path d="M4 3h3v10H4zM9 3h3v10H9z"></path>
                            </svg>
                        </button>
                        <button class="icon-button" id="restart" type="button" aria-label="Restart" title="Restart">
                            <svg class="control-icon restart-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="M3 3.5v4h4"></path>
                                <path d="M3.5 7A5 5 0 1 1 5 12"></path>
                            </svg>
                        </button>
                    </div>
                    <div class="timeline">
                        <input id="scrub" type="range" min="0" max="0" step="0.001" value="0" aria-label="Timeline">
                    </div>
                    <output id="time">0.00 / 0.00</output>
                    <div class="series-navigation" aria-label="Series navigation">
                        <button class="icon-button" id="series-previous" type="button" aria-label="Previous series item" title="No previous series item" disabled>
                            <svg class="control-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="m10.5 3.5-4.5 4.5 4.5 4.5"></path>
                            </svg>
                        </button>
                        <details class="series-menu" id="series-menu" data-positioned="false">
                            <summary aria-label="Open animation series" title="Animation series">
                                <svg class="control-icon series-icon" viewBox="0 0 16 16" aria-hidden="true">
                                    <circle cx="3" cy="4" r="0.8"></circle>
                                    <circle cx="3" cy="8" r="0.8"></circle>
                                    <circle cx="3" cy="12" r="0.8"></circle>
                                    <path d="M6 4h7M6 8h7M6 12h7"></path>
                                </svg>
                                <span id="series-position" aria-hidden="true">—</span>
                            </summary>
                            <div class="series-panel">
                                <div class="series-groups" id="series-groups" hidden></div>
                                <div class="series-items" id="series-items" data-empty="true">
                                    <span class="series-empty">NO SERIES</span>
                                </div>
                            </div>
                        </details>
                        <button class="icon-button" id="series-next" type="button" aria-label="Next series item" title="No next series item" disabled>
                            <svg class="control-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="m5.5 3.5 4.5 4.5-4.5 4.5"></path>
                            </svg>
                        </button>
                    </div>
                    <details class="player-menu" id="player-menu" data-positioned="false">
                        <summary class="icon-button" aria-label="Player settings" title="Player settings">
                            <svg class="control-icon settings-icon" viewBox="0 0 16 16" aria-hidden="true">
                                <path d="M2 4h12M2 8h12M2 12h12"></path>
                                <circle cx="5" cy="4" r="1.4"></circle>
                                <circle cx="11" cy="8" r="1.4"></circle>
                                <circle cx="7" cy="12" r="1.4"></circle>
                            </svg>
                        </summary>
                        <div class="menu-panel">
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Stat</span><output id="stat-value">DISABLED</output></summary>
                                <div class="submenu-panel stat-actions" role="group" aria-label="Canvas statistics visibility">
                                    <button type="button" data-stat="true" aria-label="Enable canvas statistics">ENABLE</button>
                                    <button type="button" data-stat="false" aria-label="Disable canvas statistics" aria-current="true" disabled>DISABLE</button>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Speed</span><output id="speed-value">1×</output></summary>
                                <div class="submenu-panel speed-actions" role="group" aria-label="Playback speed">
                                    <button type="button" data-speed="0.25">0.25×</button>
                                    <button type="button" data-speed="0.5">0.5×</button>
                                    <button type="button" data-speed="1" aria-current="true" disabled>1×</button>
                                    <button type="button" data-speed="1.5">1.5×</button>
                                    <button type="button" data-speed="2">2×</button>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Camera</span><output id="camera-mode">FIXED · 2D</output></summary>
                                <div class="submenu-panel menu-actions" id="camera" role="group" aria-label="Interactive camera controls">
                                    <button type="button" data-camera="view2d" aria-current="true" disabled>2D</button>
                                    <button type="button" data-camera="view3d" disabled>3D</button>
                                    <button type="button" data-camera="reset" disabled>RESET</button>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Range</span><output id="range-value" aria-live="polite">FULL</output></summary>
                                <div class="submenu-panel menu-actions" role="group" aria-label="Playback range controls">
                                    <button id="range-in" type="button" disabled>SET IN</button>
                                    <button id="range-out" type="button" disabled>SET OUT</button>
                                    <button id="range-clear" type="button" disabled>CLEAR</button>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Show Lines</span><output id="source-lines-value">5</output></summary>
                                <div class="submenu-panel menu-actions" role="group" aria-label="Source context expansion lines">
                                    <button type="button" data-source-lines="5" aria-current="true" disabled>5</button>
                                    <button type="button" data-source-lines="10">10</button>
                                    <button type="button" data-source-lines="20">20</button>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Option</span><output id="option-value">DISABLED</output></summary>
                                <div class="submenu-panel option-actions">
                                    <div class="option-group" role="group" aria-label="Auto-play next">
                                        <button type="button" data-auto-play-next="true">AUTO-PLAY NEXT ENABLE</button>
                                        <button type="button" data-auto-play-next="false" aria-current="true" disabled>AUTO-PLAY NEXT DISABLE</button>
                                    </div>
                                    <div class="option-group" role="group" aria-label="Compact layout">
                                        <button type="button" data-force-compact="true">COMPACT LAYOUT ENABLE</button>
                                        <button type="button" data-force-compact="false" aria-current="true" disabled>COMPACT LAYOUT DISABLE</button>
                                    </div>
                                </div>
                            </details>
                            <details class="menu-submenu" data-positioned="false">
                                <summary><span>Document</span></summary>
                                <div class="submenu-panel menu-actions document-actions" role="group" aria-label="Document commands">
                                    <button id="open-lua" type="button">LUA</button>
                                    <button id="open-source" type="button">SOURCE</button>
                                    <button id="refresh" type="button" aria-label="Refresh preview">REFRESH</button>
                                </div>
                            </details>
                            <div class="menu-direct-actions" role="group" aria-label="Canvas display">
                                <button id="fit-mode" type="button" aria-current="true" disabled>CANVAS TO FIT WINDOW</button>
                                <button id="actual-mode" type="button" disabled>CANVAS TO RAW SIZE</button>
                                <button id="preview-reset" type="button" disabled>RESET LAYOUT</button>
                            </div>
                        </div>
                        <span class="visually-hidden" id="status" data-state="booting" role="status" aria-live="polite"><span>BOOTING RUNTIME</span></span>
                    </details>
                </div>
            </footer>
            <section class="wide-series-slot" id="wide-series-slot" aria-label="Animation series playlist" hidden></section>
        </section>
        <div class="wide-resizer" id="wide-resizer" role="separator" aria-label="Resize canvas width" aria-orientation="vertical" aria-valuemin="280" aria-disabled="true" tabindex="-1"></div>
        <article class="document-body">
            <header class="document-header">
                <h1 class="document" id="document-name">NO SCENE</h1>
            </header>
            <section id="inspector-content" aria-label="Notes and related source contexts" tabindex="0">
                <details class="reference-item notes-item" id="notes-item" open>
                    <summary class="reference-summary notes-summary">
                        <span class="reference-summary-content">
                            <span class="reference-chevron" aria-hidden="true"></span>
                            <span class="reference-label">NOTES</span>
                        </span>
                    </summary>
                    <div class="description-section">
                        <div class="markdown-body" id="description-content"></div>
                    </div>
                </details>
                <div id="code-content"></div>
            </section>
        </article>
    </main>
    <script nonce="${nonce}">window.visPreviewConfig = ${config};</script>
    <script nonce="${nonce}" type="module" src="${scriptUri}"></script>
</body>
</html>`;
    }

    dispose(): void {
        if (PreviewPanel.current !== this) return;
        PreviewPanel.current = undefined;
        void vscode.commands.executeCommand("setContext", "visPreview.hasPreview", false);
        if (this.updateTimer) clearTimeout(this.updateTimer);
        if (this.sourceContextUpdateTimer) clearTimeout(this.sourceContextUpdateTimer);
        this.sourceContextUpdateGeneration += 1;
        if (this.document) this.diagnostics.delete(this.document.uri);
        while (this.disposables.length) this.disposables.pop()?.dispose();
    }
}

function sourceReferenceIdentity(source: PreviewSourceReference): string {
    const range = source.range;
    return JSON.stringify([
        source.uri.toString(),
        source.label,
        source.symbol ?? null,
        source.line ?? null,
        source.column ?? null,
        range
            ? [
                range.startLine,
                range.endLine,
                range.startColumn ?? null,
                range.endColumn ?? null,
            ]
            : null,
        source.diff
            ? [source.diff.base, source.diff.head, source.diff.oldUri.toString()]
            : null,
    ]);
}

function sourceReferenceIds(sources: readonly PreviewSourceReference[]): string[] {
    const occurrences = new Map<string, number>();
    return sources.map((source) => {
        const identity = sourceReferenceIdentity(source);
        const occurrence = occurrences.get(identity) ?? 0;
        occurrences.set(identity, occurrence + 1);
        return `${identity}:${occurrence}`;
    });
}

async function sourceLocation(
    document: vscode.TextDocument,
    source: PreviewSourceReference,
): Promise<LocatedSymbolRanges | undefined> {
    if (source.range) {
        const range = explicitSourceRange(document, source.range);
        return range ? {range, selectionRange: new vscode.Range(range.start, range.start)} : undefined;
    }
    if (source.symbol) return findSymbolRanges(document, source.symbol, source.line, source.column);
    if (source.line === undefined) return undefined;
    const lineNumber = Math.max(0, Math.min(document.lineCount - 1, source.line - 1));
    const text = document.lineAt(lineNumber).text;
    const character = source.column === undefined
        ? document.lineAt(lineNumber).firstNonWhitespaceCharacterIndex
        : Math.max(0, Math.min(text.length, source.column - 1));
    const selectionRange = new vscode.Range(lineNumber, character, lineNumber, character);
    return {range: document.lineAt(lineNumber).range, selectionRange};
}

function rangeInclusiveEndLine(range: vscode.Range): number {
    if (range.end.character === 0 && range.end.line > range.start.line) return range.end.line - 1;
    return range.end.line;
}

function defaultDescription(document: vscode.TextDocument): string {
    const name = path.basename(document.fileName);
    return `Live viscript scene. Changes in \`${name}\` are rendered automatically.`;
}

function mimeFor(name: string): string {
    switch (path.extname(name).toLowerCase()) {
        case ".png": return "png";
        case ".jpg":
        case ".jpeg": return "jpeg";
        case ".webp": return "webp";
        case ".svg": return "svg";
        case ".wav": return "audio/wav";
        case ".mp3": return "audio/mpeg";
        case ".ogg": return "audio/ogg";
        case ".flac": return "audio/flac";
        default: return path.extname(name).slice(1) || "application/octet-stream";
    }
}

function randomNonce(): string {
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let value = "";
    for (let index = 0; index < 32; index += 1) value += alphabet[Math.floor(Math.random() * alphabet.length)];
    return value;
}

function isLuaDocument(document: vscode.TextDocument): boolean {
    return document.languageId === "lua" || document.fileName.toLowerCase().endsWith(".lua");
}
