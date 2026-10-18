// Written by esbuild.config.mjs: each file gzipped and base64-encoded, since Obsidian installs a
// plugin as main.js, manifest.json and styles.css alone.
declare module "viscript:embedded" {
    /** The render server bundle (player-worker.js). */
    export const server: string;
    /** The compiler JavaScript scenes run in, in the sandbox (player-compiler.js). */
    export const compiler: string;
    /** The runtime binary (vis-wasm.wasm). */
    export const wasm: string;
    /** The fonts the engine's themes set text in: Pretendard and JetBrains Mono NL Regular, and
     * adaptive_vscode's Source Serif 4 and IBM Plex Sans KR SemiBold. */
    export const textFont: string;
    export const codeFont: string;
    export const serifFont: string;
    export const serifKoreanFont: string;
}
