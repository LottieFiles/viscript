// @viscript/player: plays viscript scenes on canvases, rendered by the WASM runtime in a worker.
// This is what a page uses, as build.mjs bundles it into player.js; a host that bundles the
// sources, as the Obsidian plugin does, imports the other modules itself.
//
//   const host = createRenderHost({server: {url: workerUrl}, wasm: () => fetch(wasmUrl).then((r) => r.arrayBuffer()),
//       fonts: runtimeFonts("runtime/")});
//   const player = new VisPlayer({host, container});
//   player.load({source, language: "lua"});
export {
    createRenderHost,
    type AssetProvider, type HostMode, type RenderHost, type RenderHostOptions, type SceneCompiler, type ServerScript,
} from "./host/host.ts";
export {createSandboxCompiler, type SandboxOptions} from "./host/sandbox.ts";
export {
    VisPlayer,
    type EndBehavior, type FrameDetail, type PlaybackOptions, type PlayerEventMap, type PlayerOptions,
    type PlayerScene, type PlayerState, type StateDetail,
} from "./player/player.ts";
export type {Quality} from "./policy.ts";
export {createCatalog, type AssetSource, type Catalog, type CatalogOptions} from "./resources.ts";
export {
    chainFonts, fontTable, runtimeFonts,
    type FontProvider, type FontProviderResult, type FontSource, type FontType,
} from "./fonts/provider.ts";
export type {AssetRef, ErrorInfo, ErrorPhase, SceneInfo, SceneLanguage, SceneResources} from "./protocol.ts";
