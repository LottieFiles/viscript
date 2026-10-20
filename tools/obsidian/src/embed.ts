// Scenes in notes: an embedded scene file, `![[intro.lua]]`, and a scene written in a code block.
import {Keymap, MarkdownRenderChild, type App, type Component, type MarkdownPostProcessorContext, type TFile} from "obsidian";
import type {SceneLanguage} from "../../player/src/protocol.ts";
import type ViscriptPlugin from "./main.ts";
import {scenePairAtLine, scenePairs, type ScenePair} from "./code-pairs.ts";
import {fenceOptions, parseOptions, type SceneOptions} from "./options.ts";
import {SceneBlock, type BlockOptions, type SceneSource} from "./scene.ts";

// Obsidian's embed registry is not in its public typings, but every embed goes through it:
// reading view, Live Preview and hover popovers alike.
interface EmbedContext {
    readonly app: App;
    readonly containerEl: HTMLElement;
    readonly linktext: string;
    readonly sourcePath: string;
}

type EmbedCreator = (context: EmbedContext, file: TFile, subpath: string) => Component & {loadFile(): Promise<void>};

export interface EmbedRegistry {
    registerExtension(extension: string, creator: EmbedCreator): void;
    unregisterExtension(extension: string): void;
    isExtensionRegistered(extension: string): boolean;
}

export function embedRegistry(app: App): EmbedRegistry | null {
    const registry = (app as App & {embedRegistry?: EmbedRegistry}).embedRegistry;
    return registry && typeof registry.registerExtension === "function" ? registry : null;
}

// Obsidian reads an embed's alias itself: a size goes to the container's width and height
// attributes, and anything else stays in alt, which for a plain embed holds the link text.
function aliasOptions(el: HTMLElement): SceneOptions {
    const options = parseOptions(el.getAttribute("alt") ?? "", el.getAttribute("src") ?? undefined);
    const width = Number.parseInt(el.getAttribute("width") ?? "", 10);
    return Number.isFinite(width) && width > 0 ? {...options, width} : options;
}

/** A scene file embedded in a note. */
export class FileEmbed extends MarkdownRenderChild {
    readonly file: TFile;
    readonly #plugin: ViscriptPlugin;
    readonly #linktext: string;
    readonly #sourcePath: string;
    #block: SceneBlock | null = null;
    #alias: MutationObserver | null = null;

    constructor(plugin: ViscriptPlugin, context: EmbedContext, file: TFile) {
        super(context.containerEl);
        this.#plugin = plugin;
        this.file = file;
        this.#linktext = context.linktext;
        this.#sourcePath = context.sourcePath;
    }

    /** What Obsidian's embed loader calls, and the plugin again when the file changes. */
    async loadFile(): Promise<void> {
        const source = await this.#plugin.app.vault.cachedRead(this.file);
        if (!this.#block) {
            this.containerEl.addClass("viscript-embed");
            this.#block = this.addChild(new SceneBlock(this.#plugin, this.containerEl, {
                ...aliasOptions(this.containerEl),
                open: (event) => void this.#plugin.app.workspace.openLinkText(this.#linktext, this.#sourcePath,
                    Keymap.isModEvent(event)),
            }));
            // Editing the alias in Live Preview keeps the embed and rewrites these attributes.
            this.#alias = new MutationObserver(() => this.#block?.setOptions(aliasOptions(this.containerEl)));
            this.#alias.observe(this.containerEl, {attributes: true, attributeFilter: ["width", "height", "alt"]});
        }
        this.#block.show({source, language: "lua", name: this.file.name, path: this.file.path});
    }

    override onunload(): void {
        this.#alias?.disconnect();
        this.#plugin.embeds.delete(this);
    }

    override onload(): void {
        this.#plugin.embeds.add(this);
    }
}

const pairedBlocks = new WeakMap<ViscriptPlugin, Set<PairedSceneBlock>>();

/** A pair's Lua fence owns its player even when only the JS fence is edited or redrawn. */
class PairedSceneBlock extends SceneBlock {
    readonly pairId: string;
    readonly sourcePath: string;

    constructor(plugin: ViscriptPlugin, el: HTMLElement, options: BlockOptions, pair: ScenePair, path: string) {
        super(plugin, el, options);
        this.pairId = pair.id;
        this.sourcePath = path;
        let owners = pairedBlocks.get(plugin);
        if (!owners) pairedBlocks.set(plugin, owners = new Set());
        owners.add(this);
        this.register(() => owners.delete(this));
        this.registerEvent(plugin.app.workspace.on("editor-change", (editor, info) => {
            if (info.file?.path !== path) return;
            const updated = scenePairs(editor.getValue()).find((candidate) => candidate.id === this.pairId);
            if (updated) this.refreshPair(updated);
        }));
    }

    refreshPair(pair: ScenePair): void {
        this.refreshVariants([pair.lua, pair.js].map(({source, language}) => ({
            source, language, name: language === "js" ? "scene.js" : "scene.lua", path: this.sourcePath,
        })));
    }
}

/** A `viscript` (Lua) or `viscript-js` code block. */
export function renderCodeBlock(plugin: ViscriptPlugin, language: SceneLanguage, source: string, el: HTMLElement,
    context: MarkdownPostProcessorContext): void {
    const section = context.getSectionInfo(el);
    const pair = section ? scenePairAtLine(section.text, section.lineStart) : null;
    if (pair && language === "js" && section?.lineStart === pair.js.lineStart) {
        // The Lua block owns the one player and its language selector; keep the Markdown source.
        for (const owner of pairedBlocks.get(plugin) ?? []) {
            if (owner.pairId === pair.id && owner.sourcePath === context.sourcePath) owner.refreshPair(pair);
        }
        el.hide();
        return;
    }
    const paired = pair && language === "lua" && section?.lineStart === pair.lua.lineStart ? pair : null;
    if (paired) el.dataset.viscriptPair = paired.id;
    const fence = paired?.lua.fence ?? (section ? section.text.split("\n")[section.lineStart] ?? "" : "");
    const options = {label: "viscript scene", ...fenceOptions(fence)};
    const block = paired ? new PairedSceneBlock(plugin, el, options, paired, context.sourcePath)
        : new SceneBlock(plugin, el, options);
    context.addChild(block);
    const scene = {source: paired?.lua.source ?? source, language, name: language === "js" ? "scene.js" : "scene.lua",
        path: context.sourcePath};
    const variants: SceneSource[] | undefined = paired ? [scene, {
        source: paired.js.source, language: "js", name: "scene.js", path: context.sourcePath,
    }] : undefined;
    block.show(scene, variants);
}
