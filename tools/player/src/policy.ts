// How large a frame to draw and how often. Pure, so the rules can be tested without a canvas.

export type Quality = "auto" | "high";

export interface FitInput {
    /** The canvas's CSS width. */
    readonly width: number;
    readonly logicalWidth: number;
    readonly logicalHeight: number;
    readonly dpr?: number;
    readonly mobile?: boolean;
    /** The largest integer scale WASM may rasterize at. */
    readonly maxRatio?: number;
    readonly quality?: Quality;
}

export interface FitPlan {
    /** Backing-store size in device pixels. */
    readonly width: number;
    readonly height: number;
    /** The integer scale WASM rasterizes at; the result is resampled to `width` × `height`. */
    readonly ratio: number;
}

/**
 * The "auto" quality caps the device pixel ratio (2 on desktop, 1.5 on phones) and keeps phones
 * at a 1× WASM raster; "high" keeps the original integer supersampling. WASM only takes a
 * positive integer scale, so whatever it draws is resampled to the backing-store size.
 */
export function fitPlan({width, logicalWidth, logicalHeight, dpr = 1, mobile = false,
    maxRatio = 3, quality = "auto"}: FitInput): FitPlan {
    const deviceRatio = Number.isFinite(dpr) && dpr > 0 ? dpr : 1;
    const cap = quality === "high" ? deviceRatio : Math.min(deviceRatio, mobile ? 1.5 : 2);
    const w = Math.max(1, Math.round(width * cap));
    const h = Math.max(1, Math.round(w * logicalHeight / logicalWidth));
    const limit = Math.max(1, Math.floor(Number.isFinite(maxRatio) ? maxRatio : 3));
    const ratio = Math.min(limit, mobile && quality !== "high" ? 1 : limit,
        Math.max(1, Math.ceil(w / logicalWidth - 0.01)));
    return {width: w, height: h, ratio};
}

/** Milliseconds between drawn frames: 30 fps on phones and 60 elsewhere unless capped, at most 120. */
export function frameInterval(mobile: boolean, maxFps?: number): number {
    const fps = maxFps !== undefined && Number.isFinite(maxFps) && maxFps > 0
        ? Math.min(120, maxFps) : mobile ? 30 : 60;
    return 1000 / fps;
}
