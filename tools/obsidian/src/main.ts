// viscript for Obsidian: scenes in notes (embedded .lua files and code blocks) and in their own
// tabs, drawn by the viscript WASM runtime in a render worker through @viscript/player.
import {
    debounce, Keymap, MarkdownView, normalizePath, Notice, Plugin, TFile, TFolder, type PaneType, type TAbstractFile,
    type WorkspaceLeaf,
} from "obsidian";
import {createRenderHost, type RenderHost} from "../../player/src/host/host.ts";
import {createSandboxCompiler} from "../../player/src/host/sandbox.ts";
import type {AssetRef, SceneResources} from "../../player/src/protocol.ts";
import {embedRegistry, FileEmbed, renderCodeBlock} from "./embed.ts";
import {bundledFont, compilerCode, runtimeWasm, serverCode} from "./embedded.ts";
import {fontCount, FontService} from "./font-service.ts";
import {editingScenes} from "./live-preview.ts";
import {CODE_BLOCK, SCENE, untitledName} from "./new-scene.ts";
import {imageMime, imageNames, scanSource, type ScannedSource} from "./scan.ts";
import type {SceneBlock, SceneSource} from "./scene.ts";
import {DEFAULT_SETTINGS, readSettings, type ViscriptSettings} from "./settings.ts";
import {ViscriptSettingTab} from "./settings-tab.ts";
import {SceneView, VIEW_TYPE} from "./view.ts";

/** Scene files: the extension embeds and tabs take. */
const EXTENSION = "lua";
const CODE_BLOCKS = [["viscript", "lua"], ["viscript-lua", "lua"], ["viscript-js", "js"]] as const;
const FONT_SETTINGS: readonly (keyof ViscriptSettings)[] = [
    "textFont", "textFontName", "codeFont", "codeFontName", "downloadFonts", "fontFolder",
];
const PLAYBACK_SETTINGS: readonly (keyof ViscriptSettings)[] = ["autoplay", "end", "controls", "maxFps", "quality"];

export default class ViscriptPlugin extends Plugin {
    override settings: ViscriptSettings = DEFAULT_SETTINGS;
    /** Every scene on screen, in notes, tabs and the settings preview. */
    readonly blocks = new Set<SceneBlock>();
    /** Embedded scene files, which follow their file. */
    readonly embeds = new Set<FileEmbed>();
    fonts!: FontService;
    #host: Promise<RenderHost> | null = null;
    #renderHost: RenderHost | null = null;

    override async onload(): Promise<void> {
        this.settings = readSettings(await this.loadData());
        this.fonts = new FontService(this.app, () => this.settings, bundledFont, (folder) => this.update({fontFolder: folder}));
        this.addSettingTab(new ViscriptSettingTab(this.app, this));

        this.registerView(VIEW_TYPE, (leaf) => new SceneView(leaf, this));
        let opensFiles = false;
        if (this.settings.openFiles) {
            try {
                this.registerExtensions([EXTENSION], VIEW_TYPE);
                opensFiles = true;
            } catch (error) {
                console.warn(`viscript: another plugin opens .${EXTENSION} files`, error);
            }
        }
        // A folder's menu in the file explorer offers a new scene file beside Obsidian's "New
        // canvas", while scene files open in the scene tab, where the new one opens.
        if (opensFiles) {
            this.registerEvent(this.app.workspace.on("file-menu", (menu, file, source) => {
                if (source !== "file-explorer-context-menu" || !(file instanceof TFolder)) return;
                menu.addItem((item) => item.setSection("action-primary").setIcon("clapperboard").setTitle("New viscript")
                    .onClick((event) => void this.#newScene(file, Keymap.isModEvent(event))));
            }));
        }
        const registry = embedRegistry(this.app);
        if (registry) {
            // A previous copy of the plugin that did not unload cleanly may still hold the extension.
            if (registry.isExtensionRegistered(EXTENSION)) registry.unregisterExtension(EXTENSION);
            registry.registerExtension(EXTENSION, (context, file) => new FileEmbed(this, context, file));
            this.register(() => registry.unregisterExtension(EXTENSION));
        }
        for (const [name, language] of CODE_BLOCKS) {
            this.registerMarkdownCodeBlockProcessor(name, (source, el, context) =>
                renderCodeBlock(this, language, source, el, context));
        }
        this.registerEditorExtension(editingScenes(this, new Map(CODE_BLOCKS)));

        this.registerEvent(this.app.workspace.on("css-change", () => {
            for (const block of this.blocks) block.retheme();
        }));
        this.registerEvent(this.app.vault.on("modify", (file) => this.#modified(file)));
        this.registerEvent(this.app.vault.on("create", (file) => this.#fontFileChanged(file.path)));
        this.registerEvent(this.app.vault.on("delete", (file) => this.#fontFileChanged(file.path)));
        this.registerEvent(this.app.vault.on("rename", (file, old) => {
            this.#fontFileChanged(file.path);
            this.#fontFileChanged(old);
        }));

        this.addCommand({
            id: "insert-scene",
            name: "Insert a scene code block",
            editorCallback: (editor) => editor.replaceSelection(CODE_BLOCK),
        });
        this.addCommand({
            id: "toggle-autoplay",
            name: "Toggle autoplay",
            callback: () => void this.update({autoplay: !this.settings.autoplay}),
        });
        this.addCommand({
            id: "reload-scenes",
            name: "Reload all scenes",
            callback: () => {
                for (const block of this.blocks) block.reload();
            },
        });
        this.addCommand({
            id: "rescan-fonts",
            name: "Look for fonts again",
            callback: async () => {
                this.rescanFonts();
                const scan = await this.fonts.scan();
                new Notice(`viscript: ${fontCount(scan.folder.size)} in the font folder`);
            },
        });

        // Enabled with notes open: Live Preview keeps widgets a previous copy built, and code
        // blocks drawn before this one existed stay plain, so those notes are drawn again.
        if (this.app.workspace.layoutReady) await this.#rebuildNotes();
    }

    override onunload(): void {
        for (const block of [...this.blocks]) block.unload();
        this.#renderHost?.dispose();
        this.#renderHost = null;
        this.#host = null;
    }

    /** The render host every scene shares, started by the first scene shown. */
    host(): Promise<RenderHost> {
        if (!this.#host) {
            const host = Promise.all([serverCode(), compilerCode()]).then(([code, compiler]) => {
                const renderHost = createRenderHost({
                    server: {code},
                    wasm: runtimeWasm,
                    // Fonts by the names scenes set text in, found as fonts.ts says.
                    fonts: this.fonts,
                    assets: {bytes: (id) => this.#imageBytes(id)},
                    mode: this.settings.worker ? "auto" : "inline",
                    // Notes may come from anyone, and Obsidian gives the page and its workers Node:
                    // JavaScript scenes run in the sandbox, and only the Lua they build is drawn.
                    compiler: createSandboxCompiler({code: compiler}),
                });
                if (this.#host !== host) {
                    renderHost.dispose();
                    throw new Error("The plugin stopped");
                }
                this.#renderHost = renderHost;
                return renderHost;
            });
            this.#host = host;
            host.catch(() => { if (this.#host === host) this.#host = null; });
        }
        return this.#host;
    }

    /** The images a scene loads, found in the vault as links from its note or file would be. */
    resources(scene: SceneSource): SceneResources {
        return {assets: this.#images(scanSource(scene.source, scene.language), scene.path)};
    }

    /** Changes and saves settings, and applies them to the scenes on screen. */
    async update(changes: Partial<ViscriptSettings>): Promise<void> {
        const previous = this.settings;
        this.settings = readSettings({...previous, ...changes});
        await this.saveData(this.settings);
        const changed = (key: keyof ViscriptSettings) => previous[key] !== this.settings[key];
        if (changed("worker")) {
            // The players move to a new host before the old one stops.
            const old = this.#renderHost;
            this.#renderHost = null;
            this.#host = null;
            for (const block of this.blocks) block.rebuild();
            old?.dispose();
        }
        if (changed("fontFolder")) this.fonts.invalidate();
        if (FONT_SETTINGS.some(changed)) this.#refreshFonts();
        if (PLAYBACK_SETTINGS.some(changed)) for (const block of this.blocks) block.configure(changed("autoplay"));
    }

    /** Reads the font folder again, and reloads every scene with its fonts. */
    rescanFonts(): void {
        this.fonts.invalidate();
        this.#refreshFonts();
    }

    // A name keeps the font it was first given in the render worker, so the worker asks again and
    // every scene loads again.
    #refreshFonts(): void {
        this.#renderHost?.refreshFonts();
        for (const block of this.blocks) block.reload();
    }

    // A new scene file in a folder takes a free name, as Obsidian's own new files do, and opens with
    // its name ready to change.
    async #newScene(folder: TFolder, pane: PaneType | boolean): Promise<void> {
        try {
            const name = untitledName(folder.children.map((child) => child.name), EXTENSION);
            const file = await this.app.vault.create(normalizePath(`${folder.path}/${name}`), SCENE);
            await this.app.workspace.getLeaf(pane).openFile(file, {eState: {rename: "all"}});
        } catch (error) {
            new Notice(`viscript: could not make a scene file: ${error instanceof Error ? error.message : error}`);
        }
    }

    // A changed scene file reloads where it is embedded or open; a changed font reloads all.
    #modified(file: TAbstractFile): void {
        if (!(file instanceof TFile)) return;
        this.#fontFileChanged(file.path);
        if (file.extension !== EXTENSION) return;
        for (const embed of this.embeds) if (embed.file === file) void embed.loadFile();
        for (const leaf of this.app.workspace.getLeavesOfType(VIEW_TYPE)) {
            if (leaf.view instanceof SceneView && leaf.view.file === file) void leaf.view.reloadFromDisk();
        }
    }

    readonly #fontsChanged = debounce(() => this.rescanFonts(), 500, true);

    #fontFileChanged(path: string): void {
        if (this.fonts.changed(path)) this.#fontsChanged();
    }

    // Images a scene loads by name, found as a link from its note or file would be.
    #images(scanned: ScannedSource, path: string): AssetRef[] {
        return imageNames(scanned).flatMap((name) => {
            const file = this.app.metadataCache.getFirstLinkpathDest(name, path);
            // The time in the id makes an edited image new to the render worker, which keeps bytes.
            return file ? [{id: `${file.path}@${file.stat.mtime}`, name, mime: imageMime(name)}] : [];
        });
    }

    async #imageBytes(id: string): Promise<ArrayBuffer> {
        const path = id.slice(0, id.lastIndexOf("@"));
        const file = this.app.vault.getFileByPath(path);
        if (!file) throw new Error(`The image ${path} is not in the vault`);
        return this.app.vault.readBinary(file);
    }

    async #rebuildNotes(): Promise<void> {
        for (const leaf of this.app.workspace.getLeavesOfType("markdown")) {
            // rebuildView() is what Obsidian itself uses; it is not in the public typings.
            const rebuild = (leaf as WorkspaceLeaf & {rebuildView?: () => Promise<void>}).rebuildView;
            if (leaf.view instanceof MarkdownView && rebuild) await rebuild.call(leaf);
        }
    }
}
