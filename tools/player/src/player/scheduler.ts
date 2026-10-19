// One requestAnimationFrame loop for every player in a window, and shared observers. Nothing runs
// while all players are paused or out of view, so an idle page stays idle. A host can show
// players in several windows from one script, as Obsidian does in its popout windows, so each
// window has its own loop, observers and media queries, taken from that window: another window's
// observers do not see its elements, and its animation frames stop while it is hidden.

export interface Tickable {
    /** Returns whether it wants another frame. */
    tick(now: number): boolean;
    /** Forget the last frame time, as after the page was hidden or resized. */
    suspend(): void;
}

type HostWindow = Window & typeof globalThis;

export class Scheduler {
    readonly window: HostWindow;
    readonly #tickables = new Set<Tickable>();
    #frame: number | null = null;
    #listening = false;
    #mobileQuery: MediaQueryList | null = null;
    readonly #observers = new Map<string, IntersectionObserver>();
    readonly #visibility = new WeakMap<Element, (visible: boolean) => void>();

    constructor(window: HostWindow) {
        this.window = window;
    }

    /** Whether the window is hidden, as a minimized window or a background tab is. */
    get hidden(): boolean {
        return this.window.document.hidden;
    }

    /** Phones and coarse pointers get the lighter frame rate and resolution policy. */
    isMobile(): boolean {
        this.#mobileQuery ??= this.window.matchMedia("(max-width: 768px), (pointer: coarse)");
        return this.#mobileQuery.matches;
    }

    requestTick(): void {
        if (this.#frame === null && !this.hidden) this.#frame = this.window.requestAnimationFrame((now) => this.#run(now));
    }

    #run(now: number): void {
        this.#frame = null;
        let more = false;
        for (const tickable of this.#tickables) if (tickable.tick(now)) more = true;
        if (more) this.requestTick();
    }

    readonly #suspendAll = (): void => {
        for (const tickable of this.#tickables) tickable.suspend();
    };

    track(tickable: Tickable): void {
        this.#tickables.add(tickable);
        if (this.#listening) return;
        this.#listening = true;
        const {document} = this.window;
        document.addEventListener("visibilitychange", () => {
            if (document.hidden && this.#frame !== null) {
                this.window.cancelAnimationFrame(this.#frame);
                this.#frame = null;
            }
            this.#suspendAll();
        });
        // The device pixel ratio can change without the canvas's CSS width changing: zooming, or
        // moving the window to another monitor.
        this.window.addEventListener("resize", this.#suspendAll);
        this.isMobile();
        this.#mobileQuery!.addEventListener("change", this.#suspendAll);
    }

    untrack(tickable: Tickable): void {
        this.#tickables.delete(tickable);
    }

    /** Calls `callback` as `element` enters and leaves the viewport grown by `rootMargin`. */
    watchVisibility(element: Element, rootMargin: string, callback: (visible: boolean) => void): () => void {
        let observer = this.#observers.get(rootMargin);
        if (!observer) {
            observer = new this.window.IntersectionObserver((entries) => {
                for (const entry of entries) this.#visibility.get(entry.target)?.(entry.isIntersecting);
            }, {rootMargin});
            this.#observers.set(rootMargin, observer);
        }
        this.#visibility.set(element, callback);
        observer.observe(element);
        return () => {
            observer.unobserve(element);
            this.#visibility.delete(element);
        };
    }

    resizeObserver(callback: ResizeObserverCallback): ResizeObserver {
        return new this.window.ResizeObserver(callback);
    }
}

const schedulers = new WeakMap<Window, Scheduler>();

/** The scheduler of the window `node` is in. */
export function schedulerFor(node: Node): Scheduler {
    const window = (node.ownerDocument ?? node as Document).defaultView ?? globalThis.window;
    let scheduler = schedulers.get(window);
    if (!scheduler) schedulers.set(window, scheduler = new Scheduler(window));
    return scheduler;
}
