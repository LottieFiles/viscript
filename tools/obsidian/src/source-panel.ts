// A read-only view of the source the player is showing, without another editor per code block.
import {loadPrism, setIcon, setTooltip} from "obsidian";
import type {SceneSource} from "./scene.ts";

const languageName = (scene: SceneSource) => scene.language === "js" ? "JavaScript" : "Lua";

export class SourcePanel {
    readonly #tools: HTMLElement;
    readonly #languages: HTMLElement;
    readonly #toggle: HTMLButtonElement;
    readonly #copy: HTMLButtonElement;
    readonly #panel: HTMLElement;
    readonly #code: HTMLElement;
    readonly #select: (scene: SceneSource) => void;
    #scene: SceneSource | null = null;
    #defaultOpen = false;
    #override: boolean | null = null;
    #generation = 0;
    #disposed = false;

    constructor(parent: HTMLElement, select: (scene: SceneSource) => void) {
        this.#select = select;
        this.#tools = parent.createDiv({cls: "viscript-source-tools"});
        this.#languages = this.#tools.createDiv({cls: "viscript-source-languages"});
        this.#copy = this.#tools.createEl("button", {cls: "clickable-icon viscript-source-copy", attr: {"aria-label": "Copy source"}});
        setIcon(this.#copy, "copy");
        setTooltip(this.#copy, "Copy source");
        this.#copy.addEventListener("click", () => void this.#copySource());
        this.#toggle = this.#tools.createEl("button", {cls: "clickable-icon viscript-source-toggle"});
        setIcon(this.#toggle, "code-2");
        this.#toggle.addEventListener("click", () => {
            this.#override = !this.#open();
            this.#show();
        });
        this.#panel = parent.createDiv({cls: "viscript-source-panel"});
        this.#code = this.#panel.createEl("pre").createEl("code");
        this.#show();
    }

    set(scene: SceneSource, variants: readonly SceneSource[]): void {
        const changed = this.#scene?.source !== scene.source || this.#scene.language !== scene.language;
        this.#scene = scene;
        this.#languages.replaceChildren();
        const choices = [...new Map(variants.map((variant) => [variant.language, variant])).values()];
        if (choices.length > 1) {
            for (const variant of choices) {
                const button = this.#languages.createEl("button", {
                    cls: "viscript-source-language",
                    text: languageName(variant),
                    attr: {"data-language": variant.language, "aria-pressed": String(variant.language === scene.language)},
                });
                button.addEventListener("click", () => {
                    if (variant.language !== this.#scene?.language) this.#select(variant);
                });
            }
        } else this.#languages.createSpan({text: languageName(scene)});
        if (!changed) return;
        setIcon(this.#copy, "copy");
        setTooltip(this.#copy, "Copy source");
        this.#generation++;
        this.#code.className = `language-${scene.language === "js" ? "javascript" : "lua"}`;
        this.#code.textContent = scene.source;
        if (this.#open()) this.#highlight();
    }

    /** A new global preference replaces the local toggle; other settings keep it. */
    configure(open: boolean): void {
        if (this.#defaultOpen !== open) this.#override = null;
        this.#defaultOpen = open;
        this.#show();
    }

    dispose(): void {
        this.#disposed = true;
        this.#generation++;
        this.#tools.remove();
        this.#panel.remove();
    }

    #open(): boolean {
        return this.#override ?? this.#defaultOpen;
    }

    #show(): void {
        const open = this.#open();
        this.#panel.hidden = !open;
        this.#copy.hidden = !open;
        this.#toggle.setAttribute("aria-expanded", String(open));
        this.#toggle.setAttribute("aria-label", open ? "Hide source" : "Show source");
        setTooltip(this.#toggle, open ? "Hide source" : "Show source");
        if (open) this.#highlight();
    }

    #highlight(): void {
        const generation = this.#generation;
        // Prism reads textContent, so source remains text even when it contains HTML or fences.
        void loadPrism().then((prism: {highlightElement: (element: HTMLElement) => void}) => {
            if (!this.#disposed && generation === this.#generation && this.#open()) prism.highlightElement(this.#code);
        }).catch(() => { /* The plain source is still readable when highlighting is unavailable. */ });
    }

    async #copySource(): Promise<void> {
        const scene = this.#scene;
        if (!scene) return;
        try {
            const view = this.#code.ownerDocument.defaultView;
            await (view?.navigator ?? navigator).clipboard.writeText(scene.source);
            if (this.#disposed || this.#scene !== scene) return;
            setIcon(this.#copy, "check");
            setTooltip(this.#copy, "Copied");
        } catch {
            if (!this.#disposed) setTooltip(this.#copy, "Could not copy source");
        }
    }
}
