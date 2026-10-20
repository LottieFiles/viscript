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

export class Surface {
    readonly #canvas: Canvas;
    readonly #scratch: () => Canvas;
    /** The integer-scale WASM raster, before it is resampled to the backing store. */
    #source: Canvas | null = null;
    #half: Canvas | null = null;

    constructor(canvas: Canvas, scratch: () => Canvas) {
        this.#canvas = canvas;
        this.#scratch = scratch;
    }

    /** Draws `engine` at `time`, replacing the frame on screen immediately. */
    present(engine: VisScene, time: number, plan: FramePlan): void {
        this.#draw(engine, time, plan, this.#canvas);
    }

    dispose(): void {
        this.#source = this.#half = null;
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
}
