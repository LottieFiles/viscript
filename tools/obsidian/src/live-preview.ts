// Live Preview draws a scene code block's source in place of its scene while the block is being
// edited. The scene then shows above the source and follows it as you type.
//
// Obsidian shows a code block's source while the editor has focus and a selection touches the
// block, from its opening fence to the end of its closing one. Blocks are found here as Obsidian
// finds them, by the line classes its Markdown mode gives, so a scene shows above the source
// exactly while Obsidian's own scene for the block is gone.
import * as language from "@codemirror/language";
import {StateEffect, StateField, type EditorState, type Extension, type Text} from "@codemirror/state";
import {Decoration, EditorView, WidgetType, type DecorationSet} from "@codemirror/view";
import type {NodeProp, Tree} from "@lezer/common";
import {debounce, editorInfoField, editorLivePreviewField, type Debouncer} from "obsidian";
import type {SceneLanguage} from "../../player/src/protocol.ts";
import type ViscriptPlugin from "./main.ts";
import {scenePairAtLine} from "./code-pairs.ts";
import {fenceOptions} from "./options.ts";
import {SceneBlock, type BlockOptions, type SceneSource} from "./scene.ts";

// The node prop that holds the line classes of Obsidian's Markdown mode, which Obsidian's
// CodeMirror has and the published package does not.
const lineClasses = (language as {lineClassNodeProp?: NodeProp<string>}).lineClassNodeProp;

// Obsidian's own patterns: an opening fence, whose first word is the language, and what comes
// before a list item's text.
const FENCE = /^\s*(~~~+|```+)[ \t]*([\w/+#-]*)[^\n`]*$/;
const LIST_MARKER = /^([>\s]*)(([*+-] |(\d+)([.)] ))(?:\[(.)\] )?)?/;

/** A scene code block, measured as Obsidian measures it. */
interface CodeBlock {
    /** From the opening fence, after any list marker, to the end of the closing fence. */
    readonly from: number;
    readonly to: number;
    /** The start of the opening fence's line, where the scene shows. */
    readonly line: number;
    readonly fence: string;
    readonly language: SceneLanguage;
    readonly source: string;
}

function codeBlocks(state: EditorState, classProp: NodeProp<string>, languages: ReadonlyMap<string, SceneLanguage>): CodeBlock[] {
    const doc = state.doc, blocks: CodeBlock[] = [];
    let from = -1, fence = "";
    language.syntaxTree(state).iterate({enter: (node) => {
        const classes = node.type.prop(classProp)?.split(" ");
        if (!classes) return;
        if (classes.includes("HyperMD-codeblock-begin") && from === -1) {
            const line = doc.lineAt(node.from);
            from = node.from;
            fence = line.text;
            const marker = classes.includes("HyperMD-list-line") ? LIST_MARKER.exec(fence) : null;
            if (marker) {
                from = line.from + marker[0].length;
                fence = fence.slice(marker[0].length);
            }
        }
        if (classes.includes("HyperMD-codeblock-end") && from !== -1) {
            const start = from;
            from = -1;
            const language = languages.get(FENCE.exec(fence)?.[2] ?? "");
            const first = doc.lineAt(start), last = doc.lineAt(node.from);
            // Obsidian draws no scene for these either.
            if (!language || last.number <= 1 || first.number >= doc.lines) return;
            const code = doc.line(first.number + 1).from;
            blocks.push({
                from: start, to: node.to, line: first.from, fence, language,
                source: doc.sliceString(code, Math.max(code, doc.line(last.number - 1).to)),
            });
        }
    }});
    return blocks;
}

// The blocks each parse of a note holds, so moving the cursor reads the note's tree no further.
const found = new WeakMap<Tree, {readonly doc: Text; readonly blocks: readonly CodeBlock[]}>();

function blocksOf(state: EditorState, classProp: NodeProp<string>, languages: ReadonlyMap<string, SceneLanguage>): readonly CodeBlock[] {
    const tree = language.syntaxTree(state);
    let entry = found.get(tree);
    if (entry?.doc !== state.doc) found.set(tree, entry = {doc: state.doc, blocks: codeBlocks(state, classProp, languages)});
    return entry.blocks;
}

const focusChange = StateEffect.define<boolean>();

/** Scenes above the code blocks being edited in Live Preview. */
export function editingScenes(plugin: ViscriptPlugin, languages: ReadonlyMap<string, SceneLanguage>): Extension {
    const classProp = lineClasses;
    if (!classProp) return [];
    // Whether the editor has focus, as Obsidian asks before it shows a block's source.
    const focused = StateField.define<boolean>({
        create: () => false,
        update: (value, tr) => tr.effects.reduce((value, effect) => effect.is(focusChange) ? effect.value : value, value),
    });
    const decorate = (state: EditorState): DecorationSet => {
        if (!state.field(focused) || !state.field(editorLivePreviewField, false)) return Decoration.none;
        const ranges = state.selection.ranges;
        const path = state.field(editorInfoField, false)?.file?.path ?? "";
        const blocks = blocksOf(state, classProp, languages);
        const selected = blocks.filter((block) => ranges.some((range) => range.from <= block.to && range.to >= block.from));
        const source = state.doc.toString(), handled = new Set<number>();
        const decorations = [];
        for (const block of selected) {
            const pair = scenePairAtLine(source, state.doc.lineAt(block.from).number - 1);
            let editing = block;
            if (pair) {
                if (handled.has(pair.lua.lineStart)) continue;
                handled.add(pair.lua.lineStart);
                const members = blocks.filter((candidate) => {
                    const line = state.doc.lineAt(candidate.from).number - 1;
                    return line === pair.lua.lineStart || line === pair.js.lineStart;
                });
                const touched = members.filter((candidate) => selected.includes(candidate));
                editing = touched.find((candidate) => state.selection.main.head >= candidate.from
                    && state.selection.main.head <= candidate.to) ?? touched[0] ?? block;
            }
            decorations.push(Decoration.widget({widget: new SceneWidget(plugin, editing, path, pair?.id), block: true, side: -1})
                .range(editing.line));
        }
        return Decoration.set(decorations, true);
    };
    const scenes = StateField.define<DecorationSet>({
        create: decorate,
        update: (value, tr) => tr.docChanged || tr.selection || tr.reconfigured
            || tr.state.field(focused) !== tr.startState.field(focused)
            || tr.state.field(editorLivePreviewField, false) !== tr.startState.field(editorLivePreviewField, false)
            || language.syntaxTree(tr.state) !== language.syntaxTree(tr.startState) ? decorate(tr.state) : value,
        provide: (field) => EditorView.decorations.from(field),
    });
    return [focused, scenes, EditorView.focusChangeEffect.of((_state, focusing) => focusChange.of(focusing))];
}

interface Shown {
    readonly scene: SceneBlock;
    readonly follow: Debouncer<[SceneSource], void>;
    fence: string;
    pairId: string | undefined;
    restorePair: () => void;
}

/** The scene in each widget's element, which stays as the source changes under it. */
const shown = new WeakMap<HTMLElement, Shown>();

// Editing either language replaces the shared reading preview with the editing preview.
// Keep Obsidian's document decorations intact so its cursor and source editing stay native.
function suppressPair(view: EditorView, pairId: string | undefined): () => void {
    if (!pairId) return () => {};
    const hidden = new Map<HTMLElement, boolean>();
    const hide = () => {
        let changed = false;
        for (const el of view.dom.querySelectorAll<HTMLElement>("[data-viscript-pair]")) {
            if (el.dataset.viscriptPair !== pairId) continue;
            if (!hidden.has(el)) {
                hidden.set(el, el.hidden);
                changed = true;
            }
            el.hidden = true;
        }
        if (changed) view.requestMeasure();
    };
    const observer = new MutationObserver(hide);
    observer.observe(view.dom, {childList: true, subtree: true});
    hide();
    return () => {
        observer.disconnect();
        for (const [el, wasHidden] of hidden) el.hidden = wasHidden;
        hidden.clear();
    };
}

class SceneWidget extends WidgetType {
    readonly #plugin: ViscriptPlugin;
    readonly #block: CodeBlock;
    readonly #path: string;
    readonly #pairId: string | undefined;

    constructor(plugin: ViscriptPlugin, block: CodeBlock, path: string, pairId?: string) {
        super();
        this.#plugin = plugin;
        this.#block = block;
        this.#path = path;
        this.#pairId = pairId;
    }

    override eq(other: SceneWidget): boolean {
        return other.#block.source === this.#block.source && other.#block.fence === this.#block.fence
            && other.#block.language === this.#block.language && other.#path === this.#path
            && other.#pairId === this.#pairId;
    }

    toDOM(view: EditorView): HTMLElement {
        const el = view.dom.ownerDocument.createElement("div");
        el.className = "viscript-editing";
        const scene = new SceneBlock(this.#plugin, el, this.#options());
        scene.load();
        scene.show(this.#scene());
        // The scene follows the source once typing pauses, as in the scene tab.
        shown.set(el, {scene, follow: debounce((source: SceneSource) => scene.show(source), 300, true),
            fence: this.#block.fence, pairId: this.#pairId, restorePair: suppressPair(view, this.#pairId)});
        return el;
    }

    override updateDOM(el: HTMLElement, view: EditorView): boolean {
        const entry = shown.get(el);
        if (!entry) return false;
        if (entry.pairId !== this.#pairId) {
            entry.restorePair();
            entry.pairId = this.#pairId;
            entry.restorePair = suppressPair(view, this.#pairId);
        }
        if (entry.fence !== this.#block.fence) {
            entry.fence = this.#block.fence;
            entry.scene.setOptions(this.#options());
        }
        entry.follow(this.#scene());
        return true;
    }

    override destroy(el: HTMLElement): void {
        const entry = shown.get(el);
        shown.delete(el);
        entry?.restorePair();
        entry?.follow.cancel();
        entry?.scene.unload();
    }

    // Focus on the scene would end the editing, so it keeps none, and an interactive scene takes
    // its input once the block is left.
    #options(): BlockOptions {
        return {label: "viscript scene", ...fenceOptions(this.#block.fence), interactive: false, keepFocus: true,
            sourceDisplay: false};
    }

    #scene(): SceneSource {
        const {language, source} = this.#block;
        return {source, language, name: language === "js" ? "scene.js" : "scene.lua", path: this.#path};
    }
}
