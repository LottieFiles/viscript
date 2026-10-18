// A scene file open in a tab: the scene above, its source below, saved as you type.
import {debounce, Scope, TextFileView, type ViewStateResult, type WorkspaceLeaf} from "obsidian";
import {createEditor, type SourceEditor} from "./editor.ts";
import type ViscriptPlugin from "./main.ts";
import {SceneBlock} from "./scene.ts";

export const VIEW_TYPE = "viscript-scene";

export class SceneView extends TextFileView {
    readonly #plugin: ViscriptPlugin;
    #block: SceneBlock | null = null;
    #editor: SourceEditor | null = null;
    #sourceEl: HTMLElement | null = null;
    #showSource = true;
    // The scene follows the source once typing pauses.
    readonly #refresh = debounce(() => this.#show(), 300, true);

    constructor(leaf: WorkspaceLeaf, plugin: ViscriptPlugin) {
        super(leaf);
        this.#plugin = plugin;
        // Keys a note's editor would take are free in this tab while the source is not focused.
        this.scope = new Scope(this.app.scope);
        this.scope.register([], " ", () => {
            const focused = this.containerEl.ownerDocument.activeElement;
            if (this.#editor?.view.hasFocus || focused?.instanceOf(HTMLInputElement) || focused?.instanceOf(HTMLTextAreaElement)) return;
            this.#block?.toggle();
            return false;
        });
    }

    getViewType(): string {
        return VIEW_TYPE;
    }

    override getIcon(): string {
        return "clapperboard";
    }

    override getDisplayText(): string {
        return this.file?.basename ?? "viscript scene";
    }

    override onload(): void {
        super.onload();
        this.#build();
        this.addAction("code-2", "Show or hide the source", () => {
            this.#showSource = !this.#showSource;
            this.#applySource();
            this.app.workspace.requestSaveLayout();
        });
        this.addAction("rotate-ccw", "Play from the start", () => this.#block?.replay());
    }

    setViewData(data: string, clear: boolean): void {
        this.#build();
        this.data = data;
        this.#editor!.set(data, clear);
        this.#show();
    }

    getViewData(): string {
        return this.data;
    }

    clear(): void {
        this.data = "";
        this.#editor?.set("", true);
    }

    /** Takes the file's text again when it changed outside this tab. */
    async reloadFromDisk(): Promise<void> {
        const file = this.file;
        if (!file) return;
        const text = await this.app.vault.read(file);
        if (file === this.file && text !== this.data) this.setViewData(text, false);
    }

    override getState(): Record<string, unknown> {
        return {...super.getState(), source: this.#showSource};
    }

    override async setState(state: unknown, result: ViewStateResult): Promise<void> {
        if (state && typeof state === "object" && "source" in state) this.#showSource = state.source !== false;
        this.#applySource();
        await super.setState(state, result);
    }

    #build(): void {
        if (this.#block) return;
        this.contentEl.addClass("viscript-view");
        const stage = this.contentEl.createDiv({cls: "viscript-view-stage"});
        this.#sourceEl = this.contentEl.createDiv({cls: "viscript-view-source"});
        this.#block = this.addChild(new SceneBlock(this.#plugin, stage, {layout: "pane", controls: true}));
        this.#editor = createEditor(this.#sourceEl, "lua", (text) => {
            this.data = text;
            this.requestSave();
            this.#refresh();
        });
        this.#applySource();
    }

    #applySource(): void {
        this.contentEl.toggleClass("is-source-hidden", !this.#showSource);
    }

    #show(): void {
        const file = this.file;
        if (file) this.#block?.show({source: this.data, language: "lua", name: file.name, path: file.path});
    }
}
