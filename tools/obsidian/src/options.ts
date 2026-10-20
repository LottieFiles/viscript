// Options a note gives one scene: in an embed's alias, `![[intro.lua|600|loop]]`, or after a code
// block's language, ```` ```viscript loop 600 ````. Words that are not options name the scene for
// assistive technology, as an image's alt text does.
import type {EndBehavior} from "../../player/src/player/player.ts";

export type Align = "left" | "center" | "right";

export interface SceneOptions {
    /** CSS pixels; a height alone is ignored, since scenes keep their proportions. */
    readonly width?: number;
    readonly end?: EndBehavior;
    readonly autoplay?: boolean;
    /** The scene's own controls take pointer and key input, and its timeline holds still. */
    readonly interactive?: boolean;
    readonly controls?: boolean;
    readonly align?: Align;
    readonly label?: string;
}

const WORDS: Readonly<Record<string, Partial<SceneOptions>>> = {
    loop: {end: "loop"},
    once: {end: "stop"},
    stop: {end: "stop"},
    autoplay: {autoplay: true},
    play: {autoplay: true},
    paused: {autoplay: false},
    still: {autoplay: false},
    interactive: {interactive: true},
    controls: {controls: true},
    nocontrols: {controls: false},
    "no-controls": {controls: false},
    left: {align: "left"},
    center: {align: "center"},
    right: {align: "right"},
};

/**
 * Reads options from `text`, `|`- or space-separated. Later words win. `ignore` is text that is
 * not a label, such as the file name Obsidian puts in a plain embed's alt.
 */
export function parseOptions(text: string, ignore?: string): SceneOptions {
    let options: SceneOptions = {};
    const words: string[] = [];
    for (const segment of text.split("|")) {
        const trimmed = segment.trim();
        if (!trimmed || trimmed === ignore) continue;
        for (const word of trimmed.split(/\s+/)) {
            const size = /^(\d+)(?:x(\d+))?$/i.exec(word);
            const option = WORDS[word.toLowerCase()];
            if (size) options = {...options, width: Number(size[1])};
            else if (option) options = {...options, ...option};
            else words.push(word);
        }
    }
    return words.length ? {...options, label: words.join(" ")} : options;
}

/** The options after a code block's language on its opening fence, as in ```` ```viscript loop ````. */
export function fenceOptions(fence: string): SceneOptions {
    const info = /^\s*(?:`{3,}|~{3,})\s*\S+\s*(.*)$/.exec(fence)?.[1] ?? "";
    return parseOptions(info);
}
