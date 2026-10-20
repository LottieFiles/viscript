// One scene on screen: a canvas the render worker draws, a bar of controls and room for an error.
// Embeds, code blocks and the scene view each show their scene through one.
import {MarkdownRenderChild, setIcon, setTooltip} from "obsidian";
import type {RenderHost} from "../../player/src/host/host.ts";
import {VisPlayer, type PlayerState} from "../../player/src/player/player.ts";
import type {SceneLanguage} from "../../player/src/protocol.ts";
import {paletteKey} from "../../player/src/theme.ts";
import type ViscriptPlugin from "./main.ts";
import type {SceneOptions} from "./options.ts";
import {looksLikeScene, scanSource} from "./scan.ts";
import {obsidianPalette} from "./theme.ts";

export interface SceneSource {
    readonly source: string;
    readonly language: SceneLanguage;
    /** The file name errors show. */
    readonly name: string;
    /** The note or file that images are found from. */
    readonly path: string;
}

export interface BlockOptions extends SceneOptions {
    /** "pane" fits the scene inside its parent, as the scene view does; "inline" is as wide as the note. */
    readonly layout?: "inline" | "pane";
    /** Opens the scene's file; given, the bar has a button for it. */
    readonly open?: (event: MouseEvent) => void;
    /** Presses on the scene leave the focus where it is, as above a code block being edited. */
    readonly keepFocus?: boolean;
}

const PLACEHOLDER = [960, 540] as const;

const reducedMotion = () => matchMedia("(prefers-reduced-motion: reduce)").matches;
const message = (error: unknown) => error instanceof Error ? error.message : String(error);

export class SceneBlock extends MarkdownRenderChild {
    readonly #plugin: ViscriptPlugin;
    #options: BlockOptions;
    readonly #root: HTMLElement;
    readonly #stage: HTMLElement;
    readonly #notice: HTMLElement;
    readonly #bar: HTMLElement;
    readonly #play: HTMLButtonElement;
    readonly #scrub: HTMLInputElement;
    readonly #time: HTMLElement;
    readonly #error: HTMLElement;
    #player: VisPlayer | null = null;
    #scene: SceneSource | null = null;
    /** The palette the scene last loaded with. */
    #palette = "";
    /** Counts starts, so a start that finishes after a later one gives way to it. */
    #generation = 0;
    #active = false;
    #resize: ResizeObserver | null = null;
    /** The timeline follows a pointer held on it, rather than playback. */
    #scrubbing = false;

    constructor(plugin: ViscriptPlugin, containerEl: HTMLElement, options: BlockOptions = {}) {
        super(containerEl);
        this.#plugin = plugin;
        this.#options = options;
        this.#root = containerEl.createDiv({cls: "viscript-scene"});
        this.#stage = this.#root.createDiv({cls: "viscript-stage"});
        this.#notice = this.#stage.createDiv({cls: "viscript-notice"});
        this.#notice.hide();
        this.#bar = this.#stage.createDiv({cls: "viscript-bar"});
        this.#play = this.#bar.createEl("button", {cls: "clickable-icon viscript-play"});
        // A click on a scene that takes no input does the same; the player sees to it.
        this.#play.addEventListener("click", () => this.toggle());
        this.#scrub = this.#bar.createEl("input", {cls: "viscript-scrub", type: "range"});
        Object.assign(this.#scrub, {min: "0", max: "1", step: "0.01", value: "0"});
        this.#scrub.setAttribute("aria-label", "Scene time");
        this.#scrub.addEventListener("input", () => {
            this.#player?.pause();
            this.#player?.seek(Number(this.#scrub.value));
        });
        this.#time = this.#bar.createSpan({cls: "viscript-time"});
        if (options.open) {
            const open = this.#bar.createEl("button", {cls: "clickable-icon viscript-open"});
            setIcon(open, "maximize-2");
            setTooltip(open, "Open the scene");
            open.addEventListener("click", (event) => this.#options.open?.(event));
        }
        this.#error = this.#root.createEl("pre", {cls: "viscript-error", attr: {role: "alert"}});
        this.#error.hide();
        if (options.keepFocus) this.#keepFocus();
        this.#showState("loading");
        this.#layout();
    }

    get player(): VisPlayer | null {
        return this.#player;
    }

    override onload(): void {
        this.#active = true;
        this.#plugin.blocks.add(this);
        if (this.#options.layout === "pane") {
            // The observer of the stage's own window, which may be a popout.
            const view = this.#stage.ownerDocument.defaultView as (Window & typeof globalThis) | null;
            this.#resize = new (view ?? window).ResizeObserver(() => this.#fit());
            this.#resize.observe(this.#stage);
        }
        void this.#start();
    }

    override onunload(): void {
        this.#active = false;
        this.#plugin.blocks.delete(this);
        this.#resize?.disconnect();
        this.#stop();
        this.#root.remove();
    }

    /** Shows `scene`; the same source again changes nothing. */
    show(scene: SceneSource): void {
        const previous = this.#scene;
        this.#scene = scene;
        if (previous && previous.source === scene.source && previous.language === scene.language
            && previous.path === scene.path) return;
        this.#load();
    }

    /** Changes the options an embed's alias gives. */
    setOptions(options: BlockOptions): void {
        const previous = this.#options;
        this.#options = {...options, layout: previous.layout, open: previous.open, keepFocus: previous.keepFocus};
        this.#layout();
        if (previous.interactive !== options.interactive) this.rebuild();
        else this.configure();
    }

    toggle(): void {
        const player = this.#player;
        if (!player) return;
        if (player.playing) player.pause();
        else player.play();
    }

    /** Plays from the start. */
    replay(): void {
        this.#player?.stop();
        this.#player?.play();
    }

    /** Applies the playback settings; a changed autoplay setting plays or pauses the scene. */
    configure(autoplayChanged = false): void {
        const settings = this.#plugin.settings;
        this.#player?.configure({
            end: this.#options.end ?? settings.end,
            maxFps: settings.maxFps || undefined,
            quality: settings.quality,
        });
        if (autoplayChanged && this.#player) {
            if (this.#autoplay()) this.#player.play();
            else this.#player.pause();
        }
        this.#layout();
    }

    /** Loads the scene again, as after the fonts changed. */
    reload(): void {
        void this.#player?.reload();
    }

    /** Loads the scene again if it follows the theme and the theme's colors changed. */
    retheme(): void {
        const player = this.#player;
        if (!player?.info?.adaptiveTheme) return;
        if (paletteKey(obsidianPalette(this.#root)) !== this.#palette) void player.reload();
    }

    /** Starts over on a new canvas, as after the render host was replaced. */
    rebuild(): void {
        this.#stop();
        if (this.#active) void this.#start();
    }

    async #start(): Promise<void> {
        const generation = ++this.#generation;
        let host;
        try {
            host = await this.#plugin.host();
        } catch (error) {
            if (generation === this.#generation && this.#active) this.#showError(`The viscript runtime could not start: ${message(error)}`);
            return;
        }
        if (generation !== this.#generation || !this.#active) return;
        // Made by the stage, so a scene in a popout window draws in that window's document.
        const canvas = this.#stage.createEl("canvas", {cls: "viscript-canvas"});
        // Sized before the canvas goes to the worker, which takes over its size.
        [canvas.width, canvas.height] = PLACEHOLDER;
        this.#stage.prepend(canvas);
        try {
            this.#player = this.#makePlayer(host, canvas);
        } catch (error) {
            canvas.remove();
            this.#showError(`The scene could not start: ${message(error)}`);
            return;
        }
        this.#load();
    }

    #makePlayer(host: RenderHost, canvas: HTMLCanvasElement): VisPlayer {
        const settings = this.#plugin.settings;
        const player = new VisPlayer({
            host,
            canvas,
            resources: (source, language) => this.#plugin.resources({...this.#scene ?? {name: "scene", path: ""}, source, language}),
            theme: () => {
                const palette = obsidianPalette(this.#root);
                this.#palette = paletteKey(palette);
                return palette;
            },
            autoplay: this.#autoplay(),
            end: this.#options.end ?? settings.end,
            interactive: this.#options.interactive ?? false,
            maxFps: settings.maxFps || undefined,
            quality: settings.quality,
            label: this.#label(),
            rootMargin: "200px",
        });
        player.on("statechange", ({detail}) => this.#showState(detail.state));
        player.on("frame", ({detail}) => this.#showTime(detail.time, detail.duration));
        player.on("load", () => {
            this.#error.hide();
            this.#root.toggleClass("is-live", player.live);
            this.#root.toggleClass("is-still", player.duration === 0 && !player.live);
            this.#showTime(player.time, player.duration);
            this.#fit();
        });
        player.on("error", ({detail}) => this.#showError(detail.message));
        return player;
    }

    #stop(): void {
        this.#generation++;
        const player = this.#player;
        this.#player = null;
        if (!player) return;
        const canvas = player.canvas;
        player.destroy();
        canvas.remove();
    }

    #load(): void {
        const scene = this.#scene, player = this.#player;
        if (!scene) return;
        const scanned = scanSource(scene.source, scene.language);
        if (!looksLikeScene(scanned)) {
            this.#notice.setText(scene.source.trim()
                ? `${scene.name} is not a viscript scene: it never calls vis.`
                : `${scene.name} is empty. A scene returns vis.scene {...}.`);
            this.#notice.show();
            this.#root.addClass("is-empty");
            player?.pause();
            return;
        }
        this.#notice.hide();
        this.#root.removeClass("is-empty");
        void player?.load({source: scene.source, language: scene.language, name: scene.name});
    }

    // A press that takes no focus: the canvas and buttons still get their clicks, but a range input
    // moves only once focused, so the timeline follows the pointer itself.
    #keepFocus(): void {
        this.#root.addEventListener("mousedown", (event) => event.preventDefault());
        const scrub = this.#scrub;
        const seek = (event: PointerEvent) => {
            const box = scrub.getBoundingClientRect();
            const at = Math.min(1, Math.max(0, (event.clientX - box.left) / Math.max(1, box.width)));
            scrub.value = String(at * Number(scrub.max));
            scrub.dispatchEvent(new Event("input"));
        };
        scrub.addEventListener("pointerdown", (event) => {
            if (event.button !== 0) return;
            scrub.setPointerCapture(event.pointerId);
            this.#scrubbing = true;
            seek(event);
        });
        scrub.addEventListener("pointermove", (event) => {
            if (this.#scrubbing && scrub.hasPointerCapture(event.pointerId)) seek(event);
        });
        scrub.addEventListener("lostpointercapture", () => { this.#scrubbing = false; });
    }

    #showError(message: string): void {
        this.#error.setText(message);
        this.#error.show();
    }

    #showState(state: PlayerState): void {
        this.#root.dataset.state = state;
        const playing = this.#player?.playing ?? false;
        setIcon(this.#play, playing ? "pause" : "play");
        setTooltip(this.#play, playing ? "Pause" : "Play");
        this.#play.setAttribute("aria-label", playing ? "Pause" : "Play");
    }

    #showTime(time: number, duration: number): void {
        if (!this.#scrubbing && this.#scrub.ownerDocument.activeElement !== this.#scrub) {
            this.#scrub.max = String(duration || 1);
            this.#scrub.value = String(time);
        }
        this.#time.setText(`${time.toFixed(1)} / ${duration.toFixed(1)}`);
        // The play button follows playback, which ends on its own.
        const playing = this.#player?.playing ?? false;
        if (this.#play.dataset.playing !== String(playing)) {
            this.#play.dataset.playing = String(playing);
            this.#showState(this.#player?.state ?? "loading");
        }
    }

    #autoplay(): boolean {
        return (this.#options.autoplay ?? this.#plugin.settings.autoplay) && !reducedMotion();
    }

    #label(): string {
        return this.#options.label ?? `${this.#scene?.name.replace(/\.(lua|js)$/, "") ?? "viscript"} scene`;
    }

    #layout(): void {
        const {width, align, controls, layout = "inline"} = this.#options;
        this.#root.dataset.layout = layout;
        this.#root.dataset.controls = controls === false ? "never" : controls === true || layout === "pane"
            ? "always" : this.#plugin.settings.controls;
        this.#root.style.width = width && layout === "inline" ? `${width}px` : "";
        if (align) this.#root.dataset.align = align;
        else delete this.#root.dataset.align;
        this.#player?.canvas.setAttribute("aria-label", this.#label());
    }

    // In a pane the scene is as large as fits, keeping its proportions.
    #fit(): void {
        const canvas = this.#player?.canvas;
        if (this.#options.layout !== "pane" || !canvas) return;
        const info = this.#player?.info;
        const ratio = info ? info.width / info.height : PLACEHOLDER[0] / PLACEHOLDER[1];
        const width = this.#stage.clientWidth, height = this.#stage.clientHeight - this.#bar.offsetHeight;
        if (width <= 0 || height <= 0) return;
        canvas.style.width = `${Math.floor(Math.min(width, height * ratio))}px`;
    }
}
