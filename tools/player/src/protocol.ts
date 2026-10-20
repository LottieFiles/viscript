// What the main thread and the render server say to each other. The server runs the WASM
// runtime, in a worker or (as a fallback) on the main thread; both use these messages, so a
// player cannot tell which one draws its frames.
import type {HostThemePalette, VisInputEvent, VisRuntimeSound} from "vis-wasm-bundle";
import type {FontType} from "./fonts/provider.ts";

export type SceneLanguage = "lua" | "js";

/** An image a scene loads by `name`; `id` names the bytes, so two files may share a name. */
export interface AssetRef {
    readonly id: string;
    readonly name: string;
    readonly mime: string;
}

/** The images a scene loads. Fonts are asked for by name as scenes use them (see FontProvider). */
export interface SceneResources {
    readonly assets: readonly AssetRef[];
}

export interface SceneRequest {
    readonly source: string;
    readonly language: SceneLanguage;
    /** Shown in error messages and stack traces. */
    readonly name: string;
    readonly resources: SceneResources;
    /** Colors for scenes that use the host-adaptive theme. */
    readonly theme?: HostThemePalette;
    /** Load the scene's sounds to play them; only a server on the main thread can. */
    readonly audio?: boolean;
}

/** What the main thread needs to know about a loaded scene to play it. */
export interface SceneInfo {
    readonly duration: number;
    /** Logical size; input coordinates are in these pixels. */
    readonly width: number;
    readonly height: number;
    readonly loop: boolean;
    /** The scene declared vis.runtime(...), so a retained Lua runtime steps it. */
    readonly retained: boolean;
    /** The runtime was built with UI or Input and takes pointer and key events. */
    readonly input: boolean;
    /** The runtime can advance a retained scene. */
    readonly runtime: boolean;
    readonly adaptiveTheme: boolean;
    readonly cameraMode: "fixed" | "interactive";
    readonly cameraView: "2d" | "3d";
}

/**
 * An input event without its time, which the server stamps from the runtime clock. Fields left
 * out are 0, except `button`, which is -1.
 */
export type InputEvent = Omit<VisInputEvent, "time">;

/** The moves of a scene's interactive camera, as VisScene.cameraInput takes them. */
export type CameraAction = "pan" | "orbit" | "zoom" | "reset" | "view2d" | "view3d";

/**
 * A camera move. A pan or an orbit goes by `x` and `y` as fractions of the canvas, a zoom by `y`
 * (less than 0 zooms in); a reset and the views take neither.
 */
export interface CameraMove {
    readonly action: CameraAction;
    readonly x: number;
    readonly y: number;
}

/** The player's transport, which the scene's sounds and music follow. */
export interface AudioState {
    readonly playing: boolean;
    readonly rate: number;
    /** The timeline jumped, as after a seek, a loop or a new rate. */
    readonly seek: boolean;
}

/** Backing-store size in device pixels, and the integer scale WASM rasterizes at. */
export interface FramePlan {
    readonly width: number;
    readonly height: number;
    readonly ratio: number;
}

export interface FrameRequest {
    readonly time: number;
    readonly plan: FramePlan;
    /** Events for a live scene, applied in order before it steps. */
    readonly input?: readonly InputEvent[];
    /** Seconds to step a live scene's retained runtime by. */
    readonly advance?: number;
    /** Release every held key and button first, as after the canvas lost focus. */
    readonly releaseInput?: boolean;
    /** Camera moves, applied in order before the frame is drawn. */
    readonly camera?: readonly CameraMove[];
    /** Where the scene's sound should be; sent once the browser lets the page play audio. */
    readonly audio?: AudioState;
}

export interface FrameResult {
    readonly runtimeTime?: number;
    readonly sounds?: readonly VisRuntimeSound[];
    /** The retained runtime failed; the frame was still drawn. */
    readonly runtimeError?: ErrorInfo;
    /** The camera's view after the frame's camera moves. */
    readonly cameraView?: "2d" | "3d";
}

/** A frame to draw apart from the canvas, as for an export. */
export interface CaptureRequest {
    readonly time: number;
    /** The integer scale to draw at; 1 is the scene's own size. */
    readonly ratio: number;
}

/** A captured frame: its size in pixels and its RGBA pixels, row by row. */
export interface CapturedFrame {
    readonly width: number;
    readonly height: number;
    readonly data: ArrayBuffer;
}

/**
 * Where a failure happened. `resource` is an image that could not be fetched, `engine` a WASM
 * instance or engine that could not start, and `timeout` scene code that ran too long: in a
 * render worker that stopped answering and was replaced, or in the sandbox, which stopped it. A
 * font is never a failure: text in one that cannot be had is drawn in the default font.
 */
export type ErrorPhase = "load" | "render" | "runtime" | "resource" | "engine" | "timeout";

export interface ErrorInfo {
    readonly message: string;
    readonly phase: ErrorPhase;
}

export type ClientMessage =
    /** The WASM binary, or why it could not be loaded; sent again to retry after a failure. */
    | {readonly type: "init"; readonly wasm?: ArrayBuffer; readonly error?: string}
    | {readonly type: "open"; readonly session: number; readonly canvas: OffscreenCanvas | HTMLCanvasElement}
    | {readonly type: "load"; readonly session: number; readonly seq: number; readonly scene: SceneRequest}
    | {readonly type: "frame"; readonly session: number; readonly seq: number; readonly frame: FrameRequest}
    | {readonly type: "capture"; readonly session: number; readonly seq: number; readonly capture: CaptureRequest}
    | {readonly type: "release"; readonly session: number}
    | {readonly type: "close"; readonly session: number}
    /** An image's bytes, or why they could not be had. */
    | {readonly type: "bytes"; readonly request: number; readonly data?: ArrayBuffer; readonly error?: string}
    /** A font for the name asked for; neither data nor error when there is none. */
    | {readonly type: "font"; readonly request: number; readonly data?: ArrayBuffer; readonly fontType?: FontType; readonly error?: string}
    /** The host's fonts changed: fonts registered so far may not be the ones it would give now. */
    | {readonly type: "fonts"}
    | {readonly type: "ping"};

export type ServerMessage =
    | {readonly type: "loaded"; readonly session: number; readonly seq: number; readonly info: SceneInfo}
    /** A newer load for the session arrived before this one finished; nothing was loaded. */
    | {readonly type: "skipped"; readonly session: number; readonly seq: number}
    | {readonly type: "framed"; readonly session: number; readonly seq: number; readonly result: FrameResult}
    | {readonly type: "captured"; readonly session: number; readonly seq: number; readonly frame: CapturedFrame}
    | {readonly type: "failed"; readonly session: number; readonly seq: number; readonly error: ErrorInfo}
    /**
     * The server is about to run a scene's code for this load. Should it stop answering, this
     * scene is the one that stalled it, whatever else is waiting.
     */
    | {readonly type: "running"; readonly session: number; readonly seq: number}
    /** The server needs an image's bytes; the host answers with "bytes". */
    | {readonly type: "fetch"; readonly request: number; readonly id: string}
    /** A scene uses a font the server has not registered; the host answers with "font". */
    | {readonly type: "font"; readonly request: number; readonly name: string}
    | {readonly type: "pong"}
    /** The server cannot run at all, such as when the WASM binary does not compile. */
    | {readonly type: "fatal"; readonly error: ErrorInfo};

/** Where the server bundle leaves its factory when it runs on the main thread instead of a worker. */
export const INLINE_SERVER = "__viscriptRenderServer";
