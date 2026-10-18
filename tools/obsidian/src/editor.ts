// The source editor in the scene view: CodeMirror 6, which Obsidian provides, with the Lua or
// JavaScript mode and the theme's code colors.
import {defaultKeymap, history, historyKeymap, indentWithTab} from "@codemirror/commands";
import {bracketMatching, indentOnInput, StreamLanguage, syntaxTree} from "@codemirror/language";
import {javascript} from "@codemirror/legacy-modes/mode/javascript";
import {lua} from "@codemirror/legacy-modes/mode/lua";
import {Annotation, EditorState, RangeSetBuilder} from "@codemirror/state";
import {
    Decoration, drawSelection, EditorView, highlightActiveLine, highlightActiveLineGutter, keymap, lineNumbers, ViewPlugin,
    type DecorationSet, type ViewUpdate,
} from "@codemirror/view";
import type {SceneLanguage} from "../../player/src/protocol.ts";

// The stream modes name their tokens as CodeMirror 5 did ("keyword", "variable-2"). Obsidian's
// CodeMirror keeps only those names, without highlight tags, so tokens are colored by name; the
// names standard CodeMirror gives them ("variableName.standard") are read by their first part.
const TOKEN_KINDS: Readonly<Record<string, string>> = {
    keyword: "keyword",
    string: "string", "string-2": "string", regexp: "string", quasi: "string",
    comment: "comment", meta: "comment",
    number: "value", atom: "value", bool: "value", null: "value",
    builtin: "function", "variable-2": "function", "variable-3": "function", "variableName.standard": "function",
    "variableName.special": "function",
    def: "property", property: "property", propertyName: "property", "variableName.definition": "property",
    operator: "operator",
    punctuation: "punctuation", bracket: "punctuation",
};

const marks = new Map<string, Decoration>();

function tokenMark(name: string): Decoration | undefined {
    const kind = TOKEN_KINDS[name] ?? TOKEN_KINDS[name.split(".")[0]!];
    if (!kind) return undefined;
    let mark = marks.get(kind);
    if (!mark) marks.set(kind, mark = Decoration.mark({class: `viscript-token-${kind}`}));
    return mark;
}

function tokenMarks(view: EditorView): DecorationSet {
    const builder = new RangeSetBuilder<Decoration>();
    const tree = syntaxTree(view.state);
    let end = 0;
    for (const {from, to} of view.visibleRanges) {
        tree.iterate({from, to, enter: (node) => {
            const mark = node.from >= end && node.to > node.from ? tokenMark(node.name) : undefined;
            if (!mark) return;
            builder.add(node.from, node.to, mark);
            end = node.to;
        }});
    }
    return builder.finish();
}

const highlightTokens = ViewPlugin.fromClass(class {
    decorations: DecorationSet;

    constructor(view: EditorView) {
        this.decorations = tokenMarks(view);
    }

    update(update: ViewUpdate) {
        if (update.docChanged || update.viewportChanged || syntaxTree(update.startState) !== syntaxTree(update.state)) {
            this.decorations = tokenMarks(update.view);
        }
    }
}, {decorations: (plugin) => plugin.decorations});

const theme = EditorView.theme({
    "&": {height: "100%", color: "var(--code-normal)", backgroundColor: "var(--background-primary)", fontSize: "var(--code-size)"},
    "&.cm-focused": {outline: "none"},
    ".cm-scroller": {fontFamily: "var(--font-monospace)", lineHeight: "1.55"},
    ".cm-content": {caretColor: "var(--caret-color)"},
    ".cm-gutters": {backgroundColor: "var(--background-primary)", color: "var(--text-faint)", border: "none"},
    ".cm-activeLine, .cm-activeLineGutter": {backgroundColor: "var(--background-primary-alt)"},
    ".cm-selectionBackground, &.cm-focused .cm-selectionBackground": {backgroundColor: "var(--text-selection)"},
    ".cm-matchingBracket": {backgroundColor: "var(--text-highlight-bg)", color: "inherit"},
    ".viscript-token-keyword": {color: "var(--code-keyword)"},
    ".viscript-token-string": {color: "var(--code-string)"},
    ".viscript-token-comment": {color: "var(--code-comment)", fontStyle: "italic"},
    ".viscript-token-value": {color: "var(--code-value)"},
    ".viscript-token-function": {color: "var(--code-function)"},
    ".viscript-token-property": {color: "var(--code-property)"},
    ".viscript-token-operator": {color: "var(--code-operator)"},
    ".viscript-token-punctuation": {color: "var(--code-punctuation)"},
});

/** Marks text set from the file, which is not reported back as an edit. */
const fromFile = Annotation.define<boolean>();

export interface SourceEditor {
    readonly view: EditorView;
    /** Replaces the text; `fresh` also forgets the undo history, as for another file. */
    set(text: string, fresh: boolean): void;
}

export function createEditor(parent: HTMLElement, language: SceneLanguage, changed: (text: string) => void): SourceEditor {
    const extensions = [
        lineNumbers(),
        highlightActiveLineGutter(),
        history(),
        drawSelection(),
        indentOnInput(),
        bracketMatching(),
        highlightActiveLine(),
        StreamLanguage.define(language === "js" ? javascript : lua),
        highlightTokens,
        EditorState.tabSize.of(4),
        keymap.of([...defaultKeymap, ...historyKeymap, indentWithTab]),
        theme,
        EditorView.updateListener.of((update) => {
            if (update.docChanged && !update.transactions.some((transaction) => transaction.annotation(fromFile))) {
                changed(update.state.doc.toString());
            }
        }),
    ];
    const view = new EditorView({parent, state: EditorState.create({doc: "", extensions})});
    return {
        view,
        set(text, fresh) {
            if (fresh) {
                view.setState(EditorState.create({doc: text, extensions}));
                return;
            }
            if (view.state.doc.toString() === text) return;
            view.dispatch({changes: {from: 0, to: view.state.doc.length, insert: text}, annotations: fromFile.of(true)});
        },
    };
}
