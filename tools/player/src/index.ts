// @viscript/player: plays viscript scenes on canvases, rendered by the WASM runtime in a worker.
//
//   const host = createRenderHost({server: {url: workerUrl}, wasm: () => fetch(wasmUrl).then((r) => r.arrayBuffer()),
//       fonts: runtimeFonts("runtime/")});
//   const player = new VisPlayer({host, container});
//   player.load({source, language: "lua"});
export {
    createRenderHost, SceneError, SupersededError,
    type AssetProvider, type HostMode, type HostSession, type LostReason, type RenderHost, type RenderHostOptions,
    type SceneCompiler, type ServerScript, type SessionHandlers,
} from "./host/host.ts";
export {createSandboxCompiler, type SandboxOptions} from "./host/sandbox.ts";
export {
    VisPlayer,
    type EndBehavior, type FrameDetail, type PlaybackOptions, type PlayerEventMap, type PlayerOptions,
    type PlayerScene, type PlayerState, type StateDetail,
} from "./player/player.ts";
export {keyName} from "./player/input.ts";
export {isMobile} from "./player/scheduler.ts";
export {fitPlan, frameInterval, type FitInput, type FitPlan, type Quality} from "./policy.ts";
export {createCatalog, type AssetSource, type Catalog, type CatalogOptions} from "./resources.ts";
export {
    chainFonts, fontTable, fontType, runtimeFonts, THEME_FONTS,
    type FontProvider, type FontProviderResult, type FontSource, type FontType,
} from "./fonts/provider.ts";
export {
    FONTSOURCE_API, FONTSOURCE_CDN, fontsourceId, FontsourceProvider,
    type FontsourceOptions, type FontsourceProviderOptions, type FontsourceResponse,
} from "./fonts/fontsource.ts";
export {
    cssColor, DARK_PALETTE, LIGHT_PALETTE, paletteKey, readPalette, resolvePalette, type PaletteVariables,
} from "./theme.ts";
export {
    extractedSize, extractFace, MAX_FONT_BYTES, memoryFile, readCharset, readFaces,
    type Charset, type ExtractOptions, type FontFace, type FontFile, type Outlines, type TableRecord,
} from "./fonts/sfnt.ts";
export {FontLibrary, type FontLocation, type LibraryFace, type LibraryOptions, type OpenFontFile} from "./fonts/library.ts";
export type {
    AssetRef, ErrorInfo, ErrorPhase, FramePlan, FrameRequest, FrameResult, InputEvent, SceneInfo,
    SceneLanguage, SceneRequest, SceneResources,
} from "./protocol.ts";
