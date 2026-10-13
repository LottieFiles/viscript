import * as vscode from "vscode";
import { registerLanguageFeatures } from "./languageProvider";
import {PreviewPanel, type PreviewContext} from "./previewPanel";
import {registerCodeVisualizations} from "./visualizationProvider";

export function activate(context: vscode.ExtensionContext): void {
    const diagnostics = vscode.languages.createDiagnosticCollection("vis-preview");
    const visualizationDiagnostics = vscode.languages.createDiagnosticCollection("vis-visualizations");
    let visualizations: ReturnType<typeof registerCodeVisualizations>;
    const openPreview = (uri: unknown, previewContext?: PreviewContext, preserveFocus?: boolean): Promise<void> => PreviewPanel.open(
        context,
        diagnostics,
        uri,
        vscode.ViewColumn.Beside,
        previewContext,
        preserveFocus,
        visualizations.previewContextForAnimation,
    );
    visualizations = registerCodeVisualizations(visualizationDiagnostics, openPreview);
    const updateSceneContext = (editor = vscode.window.activeTextEditor): void => {
        const active = editor?.document;
        const isScene = Boolean(active && isVisScene(active));
        void vscode.commands.executeCommand("setContext", "visPreview.isScene", isScene);
    };
    context.subscriptions.push(
        diagnostics,
        visualizationDiagnostics,
        ...registerLanguageFeatures(context, (document) => PreviewPanel.follows(document.uri) || isVisScene(document)),
        ...visualizations.disposables,
        vscode.window.onDidChangeActiveTextEditor(updateSceneContext),
        vscode.workspace.onDidChangeTextDocument((event) => {
            if (event.document === vscode.window.activeTextEditor?.document) updateSceneContext();
        }),
        vscode.commands.registerCommand("visPreview.openToSide", async (uri?: unknown) => {
            await openPreview(uri);
        }),
        vscode.commands.registerCommand("visPreview.openViewer", async () => {
            const series = await visualizations.previewSeriesCatalog();
            await PreviewPanel.openViewer(
                context,
                diagnostics,
                vscode.ViewColumn.Beside,
                visualizations.previewContextForAnimation,
                series,
            );
        }),
        vscode.commands.registerCommand("visPreview.refresh", () => PreviewPanel.current?.refresh()),
        vscode.commands.registerCommand("visPreview.savePng", () => PreviewPanel.saveCurrent("png")),
        vscode.commands.registerCommand("visPreview.saveGif", () => PreviewPanel.saveCurrent("gif")),
        vscode.commands.registerCommand("visPreview.saveMp4", () => PreviewPanel.saveCurrent("mp4")),
        vscode.commands.registerCommand("visPreview.openRuntimeFolder", async () => {
            const runtimeUri = vscode.Uri.joinPath(context.extensionUri, "runtime");
            await vscode.commands.executeCommand("revealFileInOS", runtimeUri);
        }),
        vscode.commands.registerCommand("visPreview.enableLuaLs", async () => {
            const folder = vscode.window.activeTextEditor
                ? vscode.workspace.getWorkspaceFolder(vscode.window.activeTextEditor.document.uri)
                : vscode.workspace.workspaceFolders?.[0];
            if (!folder) {
                await vscode.window.showWarningMessage("Open a workspace before enabling viscript types for Lua Language Server.");
                return;
            }
            const source = vscode.Uri.joinPath(context.extensionUri, "api", "vis.d.lua");
            const library = vscode.Uri.joinPath(context.globalStorageUri, "luals");
            await vscode.workspace.fs.createDirectory(library);
            await vscode.workspace.fs.copy(source, vscode.Uri.joinPath(library, "vis.d.lua"), {overwrite: true});
            const configuration = vscode.workspace.getConfiguration("Lua", folder.uri);
            const libraries = configuration.get<string[]>("workspace.library", []);
            if (!libraries.includes(library.fsPath)) {
                await configuration.update("workspace.library", [...libraries, library.fsPath], vscode.ConfigurationTarget.Workspace);
            }
            await vscode.window.showInformationMessage(`vis LuaLS types enabled from ${library.fsPath}`);
        }),
    );
    void vscode.commands.executeCommand("setContext", "visPreview.hasPreview", false);
    updateSceneContext();
}

export function deactivate(): void {
    PreviewPanel.current?.dispose();
}

function isVisScene(document: vscode.TextDocument): boolean {
    return document.languageId === "lua" && /\bvis\s*\.\s*scene\s*[{(]/.test(document.getText());
}
