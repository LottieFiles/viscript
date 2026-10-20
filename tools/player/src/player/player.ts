// A scene playing on a canvas. The player keeps the timeline, the playback state and the input
// queue on the main thread and asks a render host for each frame. A worker host runs the runtime
// off the main thread; the inline fallback runs it on the page. Its states and events follow
// the thorvg.web lottie-player.
import type {HostThemePalette, VisRuntimeSound} from "vis-wasm-bundle";
import {SceneError, SupersededError, type HostSession, type LostReason, type RenderHost} from "../host/host.ts";
import {fitPlan, frameInterval, type Quality} from "../policy.ts";
import type {
    CameraAction, CameraMove, ErrorInfo, FrameRequest, FrameResult, InputEvent, SceneInfo, SceneLanguage, SceneResources,
} from "../protocol.ts";
import {bindInput} from "./input.ts";
import {schedulerFor, type Scheduler, type Tickable} from "./scheduler.ts";

/**
 * `frozen` is playing but out of view, so no frames are drawn; `stopped` is after stop() or at the
 * end of playback; `error` is a scene that could not load or draw.
 */
export type PlayerState = "loading" | "playing" | "paused" | "stopped" | "frozen" | "error" | "destroyed";

/**
 * What happens at the end of the timeline. "loop" restarts immediately; "stop" holds the last
 * frame.
 */
export type EndBehavior = "loop" | "stop";

export interface PlayerScene {
    readonly source: string;
    /** "lua" unless given. */
    readonly language?: SceneLanguage;
    /** The file name errors and stack traces show. */
    readonly name?: string;
    /** How this scene takes input, in place of the player's `interactive` option. */
    readonly interactive?: boolean | "timeline";
}

export interface PlaybackOptions {
    /** Restart immediately at the end, or hold the last frame; "loop" unless given. */
    readonly end?: EndBehavior;
    /** How fast the timeline and the retained runtime run, from 0.125 to 8; 1 unless given. */
    readonly rate?: number;
    /**
     * Drags, the wheel and keys on the canvas move the camera of a scene whose camera is
     * interactive, unless the scene takes input; false unless given.
     */
    readonly camera?: boolean;
    /** Frames drawn per second at most; 60, or 30 on phones, unless given. */
    readonly maxFps?: number;
    readonly quality?: Quality;
    /** The largest integer scale WASM may rasterize at. */
    readonly maxRatio?: number;
}

export interface PlayerOptions extends PlaybackOptions {
    readonly host: RenderHost;
    /** A canvas to draw on, styled by the page. */
    readonly canvas?: HTMLCanvasElement;
    /** Or an element to append a new canvas to, as wide as the element. */
    readonly container?: HTMLElement;
    /**
     * The images a scene loads, such as a catalog's `resources`, or a promise of them; none
     * unless given. Fonts come from the render host's FontProvider.
     */
    readonly resources?: (source: string, language: SceneLanguage) => SceneResources | PromiseLike<SceneResources>;
    /** Colors for scenes that follow the host's theme, asked for at each load. */
    readonly theme?: () => HostThemePalette | undefined;
    /** Start playing once loaded; true unless given. */
    readonly autoplay?: boolean;
    /**
     * The scene is driven by its own UI controls: they take pointer and key input and the
     * timeline stays at 0. With "timeline" they take input as the timeline plays, for controls
     * whose transitions run on it. Scenes with a retained runtime take input either way, and
     * their runtime steps while the scene plays. A scene's own `interactive` overrides this.
     */
    readonly interactive?: boolean | "timeline";
    /** The canvas's accessible name. */
    readonly label?: string;
    /**
     * Milliseconds out of view before the scene's engine is freed; it loads again when shown.
     * Infinity keeps it, as for the only scene of a view.
     */
    readonly releaseDelay?: number;
    /** Grows the viewport the player counts as in view, as an IntersectionObserver rootMargin. */
    readonly rootMargin?: string;
    /**
     * Plays the scene's sounds and music once the page has had a click or a key press. Only a
     * render host in inline mode can, since browsers play audio on the main thread alone.
     */
    readonly audio?: boolean;
}

/** A frame drawn apart from the canvas: at `time` (the time shown unless given), `ratio` times the scene's size. */
export interface CaptureOptions {
    readonly time?: number;
    readonly ratio?: number;
}

export interface FrameDetail {
    readonly time: number;
    readonly duration: number;
}

export interface StateDetail {
    readonly state: PlayerState;
    readonly previous: PlayerState;
}

export interface PlayerEventMap {
    /** A scene finished loading; a scene loaded again after being out of view does not count. */
    load: CustomEvent<SceneInfo>;
    play: CustomEvent<undefined>;
    pause: CustomEvent<undefined>;
    stop: CustomEvent<undefined>;
    /** Playing, and moved out of view. */
    freeze: CustomEvent<undefined>;
    /** A frame was asked for. */
    frame: CustomEvent<FrameDetail>;
    loop: CustomEvent<undefined>;
    /** Playback reached the end of the timeline or of the range, and stopped there. */
    complete: CustomEvent<undefined>;
    /**
     * A load, draw or retained runtime failed. A failed load keeps the scene before it on screen;
     * a failed draw stops the player until the next load.
     */
    error: CustomEvent<ErrorInfo>;
    /** Sounds the retained runtime started; the player plays them only with the `audio` option. */
    sound: CustomEvent<readonly VisRuntimeSound[]>;
    /** The camera's view changed, after camera moves. */
    camera: CustomEvent<{readonly view: "2d" | "3d"}>;
    statechange: CustomEvent<StateDetail>;
    destroyed: CustomEvent<undefined>;
}

type Listener<K extends keyof PlayerEventMap> = (this: VisPlayer, event: PlayerEventMap[K]) => void;

// Typed overloads for the events above.
export interface VisPlayer {
    addEventListener<K extends keyof PlayerEventMap>(type: K, listener: Listener<K>, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject | null, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof PlayerEventMap>(type: K, listener: Listener<K>, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject | null, options?: boolean | EventListenerOptions): void;
}

interface Scene {
    readonly source: string;
    readonly language: SceneLanguage;
    readonly name: string;
    readonly interactive: boolean | "timeline";
}

const PLACEHOLDER_SIZE = [960, 540] as const;
const NO_RESOURCES: SceneResources = {assets: []};
// The rates the runtime's audio takes.
const MIN_RATE = 0.125, MAX_RATE = 8;

const errorText = (error: unknown): string => error instanceof Error ? error.message : String(error);

const isPromiseLike = <T>(value: T | PromiseLike<T>): value is PromiseLike<T> =>
    typeof (value as PromiseLike<T> | null)?.then === "function";

export class VisPlayer extends EventTarget implements Tickable {
    readonly #host: RenderHost;
    readonly #resources: PlayerOptions["resources"];
    readonly #theme: PlayerOptions["theme"];
    readonly #interactive: boolean | "timeline";
    readonly #audio: boolean;
    readonly #releaseDelay: number;
    readonly #rootMargin: string;
    /** The loop and observers of the window the canvas is in. */
    readonly #scheduler: Scheduler;
    #playback: Required<Omit<PlaybackOptions, "maxFps">> & Pick<PlaybackOptions, "maxFps">;
    #canvas: HTMLCanvasElement;
    #session: HostSession;
    readonly #resize: ResizeObserver;
    #unbind: () => void = () => undefined;
    #unwatch: () => void = () => undefined;
    #unlisten: () => void = () => undefined;

    #state: PlayerState = "loading";
    #scene: Scene | null = null;
    #info: SceneInfo | null = null;
    /** The server holds the scene and can draw it. */
    #resident = false;
    /** Identifies the newest load; answers to older ones are ignored. */
    #loadToken = 0;
    #loadInFlight = false;
    /** A load() the caller asked for has not finished; reloads of a scene already shown do not count. */
    #loading = false;
    #waiters: (() => void)[] = [];
    #error: ErrorInfo | null = null;
    /** Drawing failed, or the scene stalled its worker: no frames until the next load. */
    #halted = false;
    /** A frame failed because another scene stopped the shared instance, and a reload is under way. */
    #recovering = false;
    #destroyed = false;

    #playing: boolean;
    #stopped = false;
    #time = 0;
    #range: readonly [number, number] | null = null;
    #live = false;
    #input: InputEvent[] = [];
    #advance = 0;
    #releaseInput = false;
    #cameraMoves: CameraMove[] = [];
    #cameraView: "2d" | "3d" = "2d";
    /** The page may play audio, after a click or a key press. */
    #audioAllowed = false;
    /** The timeline jumped since the sound last heard of it. */
    #audioSeek = true;

    #shown = false;
    #canvasWidth = 0;
    #dirty = true;
    #last = 0;
    #nextDraw = 0;
    #releaseTimer: ReturnType<typeof setTimeout> | undefined;

    constructor(options: PlayerOptions) {
        super();
        this.#host = options.host;
        this.#resources = options.resources;
        this.#theme = options.theme;
        this.#interactive = options.interactive ?? false;
        this.#audio = options.audio ?? false;
        this.#releaseDelay = options.releaseDelay ?? 2000;
        this.#rootMargin = options.rootMargin ?? "0px";
        this.#playing = options.autoplay ?? true;
        this.#playback = {end: "loop", rate: 1, camera: false, quality: "auto", maxRatio: 3};

        let canvas = options.canvas;
        if (!canvas) {
            if (!options.container) throw new TypeError("VisPlayer needs a canvas or a container");
            canvas = options.container.ownerDocument.createElement("canvas");
            // Sized before the canvas may go to a worker, which no longer lets the page resize it.
            [canvas.width, canvas.height] = PLACEHOLDER_SIZE;
            Object.assign(canvas.style, {display: "block", width: "100%", height: "auto"});
            options.container.append(canvas);
        }
        if (options.label) canvas.setAttribute("aria-label", options.label);
        this.#canvas = canvas;
        const scheduler = this.#scheduler = schedulerFor(canvas);
        this.configure(options);
        this.#resize = scheduler.resizeObserver(() => {
            const width = this.#canvas.clientWidth;
            if (width !== this.#canvasWidth) {
                this.#canvasWidth = width;
                this.#invalidate();
            }
        });
        this.#session = this.#open();
        this.#attach();
        if (this.#audio) this.#awaitGesture();
        scheduler.track(this);
    }

    /** The canvas drawn on. It is replaced when a render worker is, so read it again after events. */
    get canvas(): HTMLCanvasElement {
        return this.#canvas;
    }

    get state(): PlayerState {
        return this.#state;
    }

    /** The loaded scene, or null before the first load. */
    get info(): SceneInfo | null {
        return this.#info;
    }

    get duration(): number {
        return this.#info?.duration ?? 0;
    }

    /** Seconds into the timeline. */
    get time(): number {
        return Math.min(this.#time, this.#info?.duration ?? this.#time);
    }

    /** Whether playback runs, or will once the scene is loaded and in view. */
    get playing(): boolean {
        return this.#playing;
    }

    /** The part of the timeline playback is confined to, from playRange(). */
    get range(): readonly [number, number] | null {
        return this.#range;
    }

    /** The scene takes input now. */
    get live(): boolean {
        return this.#live;
    }

    /** How fast playback runs. */
    get rate(): number {
        return this.#playback.rate;
    }

    /** The view of the scene's camera, which camera moves change. */
    get cameraView(): "2d" | "3d" {
        return this.#cameraView;
    }

    /** The last failure, cleared by the next load that succeeds. */
    get error(): ErrorInfo | null {
        return this.#error;
    }

    /**
     * Loads `scene` in place of the one playing, keeping the time and whether it plays. A player
     * out of view loads when it is first shown. Settles when this load finishes, fails or gives
     * way to a later one, and never rejects: failures are `error` events.
     */
    load(scene: PlayerScene): Promise<void> {
        if (this.#destroyed) return Promise.resolve();
        const language = scene.language ?? "lua";
        this.#scene = {source: scene.source, language, name: scene.name ?? `scene.${language}`,
            interactive: scene.interactive ?? this.#interactive};
        this.#loading = true;
        this.#settle();
        const done = new Promise<void>((resolve) => this.#waiters.push(resolve));
        // Whatever the server holds or is loading is no longer the scene wanted.
        this.#loadToken++;
        this.#resident = this.#loadInFlight = false;
        this.#sync();
        if (this.#shown) this.#startLoad();
        return done;
    }

    /** Loads the current scene again, as after the host's theme changed. */
    reload(): Promise<void> {
        return this.#scene ? this.load(this.#scene) : Promise.resolve();
    }

    /** Plays from the current time, or from the start after stop() or the end. */
    play(): void {
        if (this.#destroyed || (this.#playing && !this.#stopped)) return;
        const start = this.#range?.[0] ?? 0;
        if (this.#stopped || (this.#info && this.#time >= this.#end(this.#info.duration))) {
            this.#time = start;
            this.#audioSeek = true;
        }
        this.#stopped = false;
        this.#playing = true;
        this.#last = 0;
        this.#invalidate();
        this.#emit("play");
        this.#sync();
    }

    pause(): void {
        if (this.#destroyed || !this.#playing) return;
        this.#playing = false;
        // The sound hears of it with the next frame.
        if (this.#audio) this.#invalidate();
        this.#emit("pause");
        this.#sync();
    }

    /** Stops and returns to the start of the range, or of the timeline. */
    stop(): void {
        if (this.#destroyed) return;
        this.#playing = false;
        this.#stopped = true;
        this.#time = this.#range?.[0] ?? 0;
        this.#audioSeek = true;
        this.#invalidate();
        this.#emit("stop");
        this.#sync();
    }

    /** Moves to `time` seconds; a playing player plays on from there. */
    seek(time: number): void {
        if (this.#destroyed || !Number.isFinite(time)) return;
        this.#time = Math.max(0, this.#info ? Math.min(time, this.#info.duration) : time);
        this.#stopped = false;
        this.#audioSeek = true;
        this.#last = 0;
        this.#invalidate();
        this.#sync();
    }

    /** Plays from `start` and stops at `end`, such as one section of a longer scene. */
    playRange(start: number, end: number): void {
        if (this.#destroyed || !Number.isFinite(start) || !Number.isFinite(end)) return;
        const from = Math.max(0, Math.min(start, end));
        this.#range = [from, Math.max(start, end)];
        this.#time = from;
        this.#audioSeek = true;
        this.#stopped = false;
        this.#playing = true;
        this.#last = 0;
        this.#invalidate();
        this.#emit("play");
        this.#sync();
    }

    /** Lets playback run over the whole timeline again. */
    clearRange(): void {
        this.#range = null;
    }

    /** Changes how playback ends, how fast it runs, whether the camera moves and how frames are drawn. */
    configure(options: PlaybackOptions): void {
        const playback = this.#playback;
        const rate = options.rate !== undefined && Number.isFinite(options.rate) && options.rate > 0
            ? Math.min(MAX_RATE, Math.max(MIN_RATE, options.rate)) : playback.rate;
        this.#playback = {
            end: options.end ?? playback.end,
            rate,
            camera: options.camera ?? playback.camera,
            // Given as undefined, the default rate applies again.
            maxFps: "maxFps" in options ? options.maxFps : playback.maxFps,
            quality: options.quality ?? playback.quality,
            maxRatio: options.maxRatio ?? playback.maxRatio,
        };
        if (rate !== playback.rate) this.#audioSeek = true;
        this.#focusable();
        this.#nextDraw = 0;
        this.#invalidate();
    }

    /**
     * Moves the camera of a scene whose camera is interactive, with the next frame: a pan or an
     * orbit by `x` and `y` as fractions of the canvas, a zoom by `y` (less than 0 zooms in), or a
     * reset or a view. Works whether or not the `camera` option lets the canvas move it; false
     * when the scene's camera is fixed.
     */
    moveCamera(action: CameraAction, x = 0, y = 0): boolean {
        if (this.#destroyed || this.#info?.cameraMode !== "interactive" || !Number.isFinite(x) || !Number.isFinite(y)) return false;
        this.#cameraMoves.push({action, x, y});
        this.#invalidate();
        return true;
    }

    /**
     * Draws a frame apart from the canvas, as for an export: at the time shown unless given, at
     * the scene's size times `ratio`. Rejects when no scene is loaded or the frame cannot be drawn.
     */
    capture(options: CaptureOptions = {}): Promise<ImageData> {
        const info = this.#info;
        if (this.#destroyed || !info || !this.#resident) return Promise.reject(new Error("No scene is loaded to capture"));
        const time = Math.max(0, Math.min(options.time ?? this.time, info.duration));
        const ratio = Math.max(1, Math.floor(options.ratio ?? 1));
        return this.#session.capture({time: Number.isFinite(time) ? time : 0, ratio}).then((frame) =>
            new ImageData(new Uint8ClampedArray(frame.data), frame.width, frame.height));
    }

    /** Calls `listener` for `type` until the returned function is called. */
    on<K extends keyof PlayerEventMap>(type: K, listener: Listener<K>): () => void {
        this.addEventListener(type, listener);
        return () => this.removeEventListener(type, listener);
    }

    /** Stops for good and frees the scene's engine. The canvas stays in the document. */
    destroy(): void {
        if (this.#destroyed) return;
        this.#destroyed = true;
        clearTimeout(this.#releaseTimer);
        this.#scheduler.untrack(this);
        this.#detach();
        this.#unlisten();
        this.#resize.disconnect();
        this.#session.close();
        this.#input = [];
        this.#cameraMoves = [];
        this.#settle();
        this.#sync();
        this.#emit("destroyed");
    }

    /** @internal Called by the shared animation frame loop. */
    tick(now: number): boolean {
        if (this.#destroyed || !this.#shown || !this.#resident || this.#halted || this.#scheduler.hidden) {
            this.#last = 0;
            this.#nextDraw = 0;
            return false;
        }
        const duration = this.#info!.duration;
        const elapsed = (now - (this.#last || now)) / 1000;
        const rate = this.#playback.rate;
        this.#last = now;
        if (this.#playing && duration > 0 && !this.#holdsTimeline()) this.#step(elapsed * rate, duration);
        // The retained runtime steps while the scene plays; only drawing is held to the frame rate.
        if (this.#live && this.#playing) this.#advance += Math.min(0.1, elapsed) * rate;
        const moving = this.#playing && (this.#live || duration > 0);
        if (!this.#dirty && !moving) return false;
        if (!this.#dirty && now < this.#nextDraw - 0.5) return moving;
        this.#draw(duration);
        const interval = frameInterval(this.#scheduler.isMobile(), this.#playback.maxFps);
        // Keeps the cadence through animation frame jitter, without catching up on missed frames.
        this.#nextDraw = this.#nextDraw > 0 && now >= this.#nextDraw
            ? now + interval - ((now - this.#nextDraw) % interval) : now + interval;
        return moving;
    }

    /** @internal Forgets the last frame time, as after the page was hidden or resized. */
    suspend(): void {
        this.#last = 0;
        this.#nextDraw = 0;
        this.#invalidate();
    }

    #end(duration: number): number {
        return this.#range ? Math.min(this.#range[1], duration) : duration;
    }

    #step(elapsed: number, duration: number): void {
        let time = this.#time + elapsed;
        if (this.#range || this.#playback.end === "stop") {
            const end = this.#end(duration);
            if (time >= end) {
                this.#time = end;
                this.#complete();
                return;
            }
        } else if (time >= duration) {
            time %= duration;
            this.#audioSeek = true;
            this.#emit("loop");
        }
        this.#time = time;
    }

    #complete(): void {
        this.#playing = false;
        this.#stopped = true;
        this.#dirty = true;
        this.#emit("complete");
        this.#sync();
    }

    #draw(duration: number): void {
        this.#dirty = false;
        const info = this.#info!;
        if (!this.#canvasWidth) return;
        const time = Math.min(this.#time, duration);
        const plan = fitPlan({
            width: this.#canvasWidth, logicalWidth: info.width, logicalHeight: info.height,
            dpr: this.#scheduler.window.devicePixelRatio || 1, mobile: this.#scheduler.isMobile(), maxRatio: this.#playback.maxRatio,
            quality: this.#playback.quality,
        });
        const frame: FrameRequest = {
            time, plan,
            ...(this.#live ? {advance: this.#advance} : {}),
            ...(this.#input.length ? {input: this.#input} : {}),
            ...(this.#releaseInput ? {releaseInput: true} : {}),
            ...(this.#cameraMoves.length ? {camera: this.#cameraMoves} : {}),
            ...(this.#plays() && this.#audioAllowed
                ? {audio: {playing: this.#playing, rate: this.#playback.rate, seek: this.#audioSeek}} : {}),
        };
        this.#input = [];
        this.#advance = 0;
        this.#releaseInput = false;
        this.#cameraMoves = [];
        if (frame.audio) this.#audioSeek = false;
        this.#session.frame(frame);
        this.#emit("frame", {time, duration});
    }

    #open(): HostSession {
        return this.#host.open(this.#canvas, {
            framed: (result) => this.#framed(result),
            failed: (error) => this.#frameFailed(error),
            lost: (reason) => this.#lost(reason),
        });
    }

    #attach(): void {
        const canvas = this.#canvas;
        this.#unbind = bindInput(canvas, {
            live: () => this.#live,
            camera: () => this.#takesCamera(),
            view: () => this.#cameraView,
            size: () => this.#info ? [this.#info.width, this.#info.height] : PLACEHOLDER_SIZE,
            queue: (event) => {
                this.#input.push(event);
                this.#invalidate();
            },
            moveCamera: (action, x, y) => this.moveCamera(action, x, y),
            releaseAll: () => {
                this.#input = [{type: "pointercancel", buttons: 0}];
                this.#releaseInput = true;
                this.#invalidate();
            },
            // A click on a drawn scene that does not take input plays or pauses it.
            click: () => {
                if (!this.#info) return;
                if (this.#playing) this.pause();
                else this.play();
            },
        });
        this.#resize.observe(canvas);
        this.#unwatch = this.#scheduler.watchVisibility(canvas, this.#rootMargin, (visible) => this.#visible(visible));
    }

    #detach(): void {
        this.#unbind();
        this.#resize.unobserve(this.#canvas);
        this.#unwatch();
    }

    #startLoad(): void {
        const scene = this.#scene;
        if (!scene || this.#destroyed) return;
        const token = ++this.#loadToken;
        this.#loadInFlight = true;
        this.#resident = false;
        const failed = (error: unknown) => this.#loadFailed(token, {phase: "resource", message: errorText(error)});
        let resources: SceneResources | PromiseLike<SceneResources>;
        try {
            resources = this.#resources?.(scene.source, scene.language) ?? NO_RESOURCES;
        } catch (error) {
            failed(error);
            return;
        }
        if (!isPromiseLike(resources)) {
            this.#loadScene(token, scene, resources);
            return;
        }
        // A later load, a release or a replaced worker may take over while they are found.
        Promise.resolve(resources).then((found) => {
            if (token === this.#loadToken) this.#loadScene(token, scene, found);
        }, failed);
    }

    #loadScene(token: number, scene: Scene, resources: SceneResources): void {
        const theme = this.#theme?.();
        this.#session.load({
            source: scene.source, language: scene.language, name: scene.name, resources,
            ...(theme ? {theme} : {}),
            ...(this.#plays() ? {audio: true} : {}),
        }).then((info) => this.#loaded(token, info), (error: unknown) => {
            // A later load, a released engine or a replaced worker took over.
            if (error instanceof SupersededError) return;
            this.#loadFailed(token, {phase: error instanceof SceneError ? error.phase : "load", message: errorText(error)});
        });
    }

    #loaded(token: number, info: SceneInfo): void {
        if (token !== this.#loadToken || this.#destroyed) return;
        const announce = this.#loading;
        this.#loadInFlight = this.#loading = false;
        this.#info = info;
        this.#resident = true;
        this.#error = null;
        this.#halted = false;
        this.#live = info.input && (this.#scene!.interactive !== false || (info.retained && info.runtime));
        this.#input = [];
        this.#advance = 0;
        this.#releaseInput = false;
        this.#cameraMoves = [];
        this.#cameraView = info.cameraView;
        this.#audioSeek = true;
        if (this.#holdsTimeline()) this.#time = 0;
        if (this.#time > info.duration) {
            this.#time = 0;
            this.#range = null;
        }
        this.#focusable();
        this.#canvas.style.aspectRatio = `${info.width} / ${info.height}`;
        this.#last = 0;
        this.#nextDraw = 0;
        this.#invalidate();
        this.#settle();
        this.#sync();
        if (announce) this.#emit("load", info);
    }

    #loadFailed(token: number, error: ErrorInfo): void {
        if (token !== this.#loadToken || this.#destroyed) return;
        this.#loadInFlight = this.#loading = false;
        // The server keeps the scene before a failed load, so it plays on.
        this.#resident = this.#info !== null;
        this.#recovering = false;
        // Code that ran too long runs again only when asked, not each time the scene comes into view.
        if (error.phase === "timeout" && !this.#resident) this.#halted = true;
        this.#error = error;
        this.#invalidate();
        this.#settle();
        this.#sync();
        this.#emit("error", error);
    }

    #framed(result: FrameResult): void {
        this.#recovering = false;
        if (result.runtimeError) {
            // The scene still draws; its runtime and input stop.
            this.#live = false;
            this.#focusable();
            this.#error = result.runtimeError;
            this.#emit("error", result.runtimeError);
        }
        if (result.sounds?.length) this.#emit("sound", result.sounds);
        if (result.cameraView && result.cameraView !== this.#cameraView) {
            this.#cameraView = result.cameraView;
            this.#emit("camera", {view: result.cameraView});
        }
    }

    #frameFailed(error: ErrorInfo): void {
        if (error.phase === "engine" && !this.#recovering) {
            // Another scene stopped the WASM instance this one shared: load again in a new one.
            this.#recovering = true;
            this.#startLoad();
            return;
        }
        this.#halt(error);
    }

    #halt(error: ErrorInfo): void {
        this.#halted = true;
        this.#live = false;
        this.#focusable();
        this.#error = error;
        this.#sync();
        this.#emit("error", error);
    }

    // The canvas went with a render worker that was replaced: a copy of it takes its place, and
    // the scene loads again there, unless this scene is what stalled the worker.
    #lost(reason: LostReason): void {
        if (this.#destroyed) return;
        this.#detach();
        const canvas = this.#canvas.cloneNode(false) as HTMLCanvasElement;
        this.#canvas.replaceWith(canvas);
        this.#canvas = canvas;
        // Whatever was loading went with the old worker.
        this.#loadToken++;
        this.#resident = this.#loadInFlight = false;
        this.#input = [];
        this.#advance = 0;
        this.#releaseInput = false;
        this.#cameraMoves = [];
        this.#session = this.#open();
        this.#attach();
        if (reason === "timeout") {
            this.#loading = false;
            this.#settle();
            this.#halt({phase: "timeout", message: "This scene stopped responding, so it was halted"});
            return;
        }
        if (this.#shown && this.#mayLoad()) this.#startLoad();
    }

    // A halted scene loads again only when asked to.
    #mayLoad(): boolean {
        return this.#loading || !this.#halted;
    }

    #visible(visible: boolean): void {
        if (this.#destroyed || this.#shown === visible) return;
        this.#shown = visible;
        this.#last = 0;
        this.#nextDraw = 0;
        clearTimeout(this.#releaseTimer);
        if (visible) {
            this.#canvasWidth = this.#canvas.clientWidth;
            if (!this.#resident && !this.#loadInFlight && this.#mayLoad()) this.#startLoad();
            this.#invalidate();
        } else {
            // A timer given more than 2^31 - 1 milliseconds fires at once.
            const delay = this.#releaseDelay;
            if (delay !== Infinity) this.#releaseTimer = setTimeout(() => this.#release(), Math.min(delay, 2 ** 31 - 1));
        }
        const before = this.#state;
        this.#sync();
        if (before === "playing" && this.#state === "frozen") this.#emit("freeze");
    }

    // Out of view for a while: the engine goes, and the scene loads again when shown. A load
    // under way is dropped too; it starts again when shown.
    #release(): void {
        if (this.#shown || this.#destroyed || (!this.#resident && !this.#loadInFlight)) return;
        this.#loadToken++;
        this.#resident = this.#loadInFlight = false;
        this.#input = [];
        this.#advance = 0;
        this.#releaseInput = false;
        this.#session.release();
    }

    #invalidate(): void {
        this.#dirty = true;
        this.#scheduler.requestTick();
    }

    // The scene's own controls drive it, and its timeline stays at 0.
    #holdsTimeline(): boolean {
        return this.#live && this.#scene?.interactive === true;
    }

    // Drags, the wheel and keys move the camera: the option allows it, the scene's camera is
    // interactive, and the scene does not take the input itself.
    #takesCamera(): boolean {
        return this.#playback.camera && this.#info?.cameraMode === "interactive" && !this.#live && !this.#halted;
    }

    // The canvas takes focus, and so keys, when the scene takes input or the camera moves.
    #focusable(): void {
        this.#canvas.tabIndex = this.#live || this.#takesCamera() ? 0 : -1;
    }

    // The scene's sound plays where the server can play it: on the main thread.
    #plays(): boolean {
        return this.#audio && this.#host.mode === "inline";
    }

    // Browsers play audio once the page has had a click or a key press, which may have come before.
    #awaitGesture(): void {
        const view = this.#scheduler.window;
        if ((view.navigator as Navigator & {userActivation?: {hasBeenActive: boolean}}).userActivation?.hasBeenActive) {
            this.#audioAllowed = true;
            return;
        }
        const controller = new AbortController();
        const allow = () => {
            controller.abort();
            this.#audioAllowed = true;
            this.#audioSeek = true;
            this.#invalidate();
        };
        for (const type of ["pointerdown", "keydown"]) view.addEventListener(type, allow, {capture: true, signal: controller.signal});
        this.#unlisten = () => controller.abort();
    }

    #settle(): void {
        for (const resolve of this.#waiters.splice(0)) resolve();
    }

    #sync(): void {
        const previous = this.#state;
        const state: PlayerState = this.#destroyed ? "destroyed"
            : this.#loading ? "loading"
            : this.#halted || !this.#info ? (this.#error ? "error" : "loading")
            : this.#playing ? (this.#shown ? "playing" : "frozen")
            : this.#stopped ? "stopped" : "paused";
        if (state === previous) return;
        this.#state = state;
        this.#emit("statechange", {state, previous});
    }

    #emit(type: keyof PlayerEventMap, detail?: unknown): void {
        this.dispatchEvent(new CustomEvent(type, {detail}));
    }
}
