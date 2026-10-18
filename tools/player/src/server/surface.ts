// Presents an engine's frames on a canvas: an OffscreenCanvas in the render worker, or the
// page's own canvas when the server runs on the main thread.
import type {VisScene} from "vis-wasm-bundle";
import type {FramePlan} from "../protocol.ts";

export type Canvas = HTMLCanvasElement | OffscreenCanvas;
type Context = CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;

function context(canvas: Canvas): Context {
    const ctx = canvas.getContext("2d") as Context | null;
    if (!ctx) throw new Error("This canvas has no 2D context");
    return ctx;
}

function resize(canvas: Canvas, width: number, height: number): void {
    if (canvas.width !== width) canvas.width = width;
    if (canvas.height !== height) canvas.height = height;
}

// The CSS `ease` curve the page's old ghost canvas faded with, near enough.
const ease = (t: number): number => t * t * (3 - 2 * t);

interface Ghost {
    readonly canvas: Canvas;
    readonly start: number;
    readonly duration: number;
}

export class Surface {
    readonly #canvas: Canvas;
    readonly #scratch: () => Canvas;
    /** The integer-scale WASM raster, before it is resampled to the backing store. */
    #source: Canvas | null = null;
    #half: Canvas | null = null;
    /** The newest frame, kept apart while a ghost fades over it. */
    #frame: Canvas | null = null;
    #ghost: Ghost | null = null;
    #timer: ReturnType<typeof setTimeout> | undefined;

    constructor(canvas: Canvas, scratch: () => Canvas) {
        this.#canvas = canvas;
        this.#scratch = scratch;
    }

    /**
     * Draws `engine` at `time`. With `crossfade`, the frame on screen is kept and dissolved into
     * the new ones over that many milliseconds, so a replay starts without a cut.
     */
    present(engine: VisScene, time: number, plan: FramePlan, crossfade = 0): void {
        if (crossfade > 0 && this.#canvas.width > 0 && this.#canvas.height > 0) this.#hold(crossfade);
        if (!this.#ghost) {
            this.#draw(engine, time, plan, this.#canvas);
            return;
        }
        this.#frame ??= this.#scratch();
        this.#draw(engine, time, plan, this.#frame);
        this.#composite();
    }

    dispose(): void {
        clearTimeout(this.#timer);
        this.#timer = undefined;
        this.#ghost = null;
        this.#source = this.#half = this.#frame = null;
    }

    // WASM rasterizes at a positive integer scale only. When that is not the backing store's
    // size, the raster is halved until it is within 1.5x of it and then resampled, since one
    // large downscale aliases.
    #draw(engine: VisScene, time: number, {width, height, ratio}: FramePlan, target: Canvas): void {
        const [logicalWidth] = engine.size();
        if (logicalWidth * ratio === width) {
            engine.draw(target as HTMLCanvasElement, time, ratio);
            return;
        }
        const source = this.#source ??= this.#scratch();
        engine.draw(source as HTMLCanvasElement, time, ratio);
        let from: Canvas = source, fromWidth = source.width, fromHeight = source.height;
        while (fromWidth / 2 >= width * 1.5) {
            const nextWidth = Math.round(fromWidth / 2), nextHeight = Math.round(fromHeight / 2);
            const half = this.#half ??= this.#scratch();
            if (half.width < nextWidth || half.height < nextHeight) {
                resize(half, Math.max(half.width, nextWidth), Math.max(half.height, nextHeight));
            }
            const ctx = context(half);
            ctx.imageSmoothingQuality = "high";
            ctx.clearRect(0, 0, nextWidth, nextHeight);
            ctx.drawImage(from, 0, 0, fromWidth, fromHeight, 0, 0, nextWidth, nextHeight);
            from = half;
            fromWidth = nextWidth;
            fromHeight = nextHeight;
        }
        resize(target, width, height);
        const ctx = context(target);
        ctx.imageSmoothingQuality = "high";
        // Drawing does not replace pixels as putImageData does, so a transparent scene would
        // otherwise show the frame before it.
        ctx.clearRect(0, 0, width, height);
        ctx.drawImage(from, 0, 0, fromWidth, fromHeight, 0, 0, width, height);
    }

    #hold(duration: number): void {
        const ghost = this.#ghost?.canvas ?? this.#scratch();
        resize(ghost, this.#canvas.width, this.#canvas.height);
        const ctx = context(ghost);
        ctx.clearRect(0, 0, ghost.width, ghost.height);
        ctx.drawImage(this.#canvas, 0, 0);
        this.#ghost = {canvas: ghost, start: performance.now(), duration};
    }

    // The fade finishes on its own clock: a paused player sends no more frames, and the ghost
    // must not stay half visible.
    #composite(): void {
        const ghost = this.#ghost, frame = this.#frame;
        if (!ghost || !frame) return;
        const progress = Math.min(1, (performance.now() - ghost.start) / ghost.duration);
        resize(this.#canvas, frame.width, frame.height);
        const ctx = context(this.#canvas);
        ctx.clearRect(0, 0, frame.width, frame.height);
        ctx.drawImage(frame, 0, 0);
        if (progress >= 1) {
            this.#ghost = null;
            return;
        }
        ctx.globalAlpha = 1 - ease(progress);
        ctx.drawImage(ghost.canvas, 0, 0, frame.width, frame.height);
        ctx.globalAlpha = 1;
        this.#timer ??= setTimeout(() => {
            this.#timer = undefined;
            this.#composite();
        }, 16);
    }
}
