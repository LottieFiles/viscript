// Where the plugin finds fonts, and the FontProvider the render host asks for them by name: the
// fonts it ships, a folder in the vault, and Fontsource, whose fonts it saves in the folder. The
// folder is read on first use and again after a change; a font file's bytes are read only when a
// scene needs that face.
import {normalizePath, requestUrl, type App} from "obsidian";
import {FontsourceProvider} from "../../player/src/fonts/fontsource.ts";
import {FontLibrary, type FontLocation, type LibraryFace} from "../../player/src/fonts/library.ts";
import {fontType, type FontProvider, type FontProviderResult} from "../../player/src/fonts/provider.ts";
import {memoryFile} from "../../player/src/fonts/sfnt.ts";
import {fontFileName, fontProvider, namedFile, type FontSources, type ThemeRole} from "./fonts.ts";
import type {ViscriptSettings} from "./settings.ts";

const FONT_FILE = /\.(ttf|ttc|otf|otc)$/i;
/** Enough for any font folder; a folder deeper or larger than this is not one. */
const MAX_DEPTH = 5, MAX_FILES = 6000;
/** The folder downloaded fonts are saved in when none is set. */
export const DEFAULT_FONT_FOLDER = "Fonts";
/** How long the vault's word that the plugin wrote a font file is taken as its own doing. */
const OWN_WRITE_MS = 5000;

export interface FontScan {
    /** Faces from the vault's font folder. */
    readonly folder: FontLibrary;
    /** Font files read, and those that held no face the engine can draw. */
    readonly files: number;
    readonly unusable: number;
}

/** "1 font", "3 fonts". */
export const fontCount = (count: number) => `${count} ${count === 1 ? "font" : "fonts"}`;

/** A font, and where it came from, as `folder:Fonts/Lobster.ttf` or `bundled:text`. */
export interface FoundFont extends FontProviderResult {
    readonly source: string;
}

// A vault file's id as a font location; the time in it makes a changed file a new font.
const vaultId = (path: string, mtime: number) => `folder:${path}@${mtime}`;
const vaultPath = (id: string) => id.slice("folder:".length, id.lastIndexOf("@"));

export class FontService implements FontProvider {
    readonly #app: App;
    readonly #settings: () => ViscriptSettings;
    readonly #bundled: (role: ThemeRole) => Promise<ArrayBuffer>;
    readonly #useFolder: (folder: string) => Promise<void>;
    readonly #provider: FontProvider;
    readonly #fontsource = new FontsourceProvider({
        request: async (url) => {
            const response = await requestUrl({url, throw: false});
            return {status: response.status, body: response.arrayBuffer};
        },
    });
    /** Downloads under way, so a font asked for twice at once is fetched once. */
    readonly #downloads = new Map<string, Promise<FoundFont | null>>();
    /** Font files the plugin wrote, and when, whose news from the vault is not a change to look into. */
    readonly #written = new Map<string, number>();
    #scan: Promise<FontScan> | null = null;

    /** `bundled` gives the font the plugin ships for a role; `useFolder` sets the font folder, for
     * the first font downloaded while none is set. */
    constructor(app: App, settings: () => ViscriptSettings, bundled: (role: ThemeRole) => Promise<ArrayBuffer>,
        useFolder: (folder: string) => Promise<void>) {
        this.#app = app;
        this.#settings = settings;
        this.#bundled = bundled;
        this.#useFolder = useFolder;
        const sources: FontSources = {
            folder: (name) => this.#folderFont(name),
            download: (name) => this.#download(name),
            bundled: (role) => this.#bundledFont(role),
        };
        this.#provider = fontProvider(settings, sources);
    }

    /** The font to register under `name`, found as fonts.ts says. */
    fetch(name: string): Promise<FoundFont> {
        return this.#provider.fetch(name) as Promise<FoundFont>;
    }

    /** The fonts in the font folder, read on first use. */
    scan(): Promise<FontScan> {
        if (!this.#scan) {
            const scan = this.#read();
            this.#scan = scan;
            scan.catch(() => { if (this.#scan === scan) this.#scan = null; });
        }
        return this.#scan;
    }

    /** The fonts are read again when next needed, as after the font folder or a file in it changed. */
    invalidate(): void {
        this.#scan = null;
    }

    /** Whether a change to `path` in the vault is one to the font folder that the plugin did not make. */
    changed(path: string): boolean {
        const folder = this.#settings().fontFolder.trim();
        if (folder === "" || !FONT_FILE.test(path) || !(path + "/").startsWith(normalizePath(folder) + "/")) return false;
        const written = this.#written.get(path);
        return written === undefined || Date.now() - written > OWN_WRITE_MS;
    }

    async #found(library: FontLibrary, entry: LibraryFace | undefined, source: string): Promise<FoundFont | null> {
        if (!entry) return null;
        const data = await library.bytes(entry.id);
        return {data, type: fontType(data), source};
    }

    async #folderFont(name: string): Promise<FoundFont | null> {
        const {folder} = await this.scan();
        const entry = folder.find(name) ?? folder.faces().find((face) => namedFile(vaultPath(face.location.id), name));
        return this.#found(folder, entry, `folder:${entry ? vaultPath(entry.location.id) : ""}`);
    }

    async #bundledFont(role: ThemeRole): Promise<FoundFont> {
        return {data: await this.#bundled(role), type: "ttf", source: `bundled:${role}`};
    }

    #download(name: string): Promise<FoundFont | null> {
        let pending = this.#downloads.get(name);
        if (!pending) {
            pending = this.#fetchAndSave(name);
            this.#downloads.set(name, pending);
            const forget = () => { if (this.#downloads.get(name) === pending) this.#downloads.delete(name); };
            pending.then(forget, forget);
        }
        return pending;
    }

    // From Fontsource, then into the font folder, so the next scene that names it finds it there, on
    // every device the vault syncs to. A font that cannot be saved is still used.
    async #fetchAndSave(name: string): Promise<FoundFont | null> {
        const found = await this.#fontsource.fetch(name);
        if (!found) return null;
        const data = found.data instanceof ArrayBuffer ? found.data
            : found.data.buffer.slice(found.data.byteOffset, found.data.byteOffset + found.data.byteLength) as ArrayBuffer;
        const set = this.#settings().fontFolder.trim();
        const folder = normalizePath(set || DEFAULT_FONT_FOLDER);
        const path = normalizePath(`${folder}/${fontFileName(name)}`);
        try {
            const adapter = this.#app.vault.adapter;
            if (!await adapter.exists(path)) {
                if (!await adapter.exists(folder)) await adapter.mkdir(folder);
                this.#written.set(path, Date.now());
                await adapter.writeBinary(path, data);
                await this.#remember(path);
            }
            if (!set) await this.#useFolder(folder);
        } catch (error) {
            console.warn(`viscript: could not save the font "${name}" in ${folder}:`, error);
        }
        return {data, type: "ttf", source: `fontsource:${path}`};
    }

    // A font saved in the folder joins the faces already read, rather than all being read again.
    async #remember(path: string): Promise<void> {
        const scan = this.#scan;
        if (scan) await (await scan).folder.add([await this.#location(path)]);
    }

    async #read(): Promise<FontScan> {
        const folder = new FontLibrary({cacheBytes: false});
        const files = await this.#folderLocations(this.#settings().fontFolder);
        await folder.add(files);
        const used = new Set(folder.faces().map((entry) => entry.location.id));
        return {folder, files: files.length, unusable: files.filter((file) => !used.has(file.id)).length};
    }

    // Font files anywhere under the folder, hidden folders included, as the adapter sees them.
    async #folderLocations(folder: string): Promise<FontLocation[]> {
        if (!folder.trim()) return [];
        const adapter = this.#app.vault.adapter;
        const paths: string[] = [];
        const walk = async (dir: string, depth: number): Promise<void> => {
            let listed;
            try {
                listed = await adapter.list(dir);
            } catch {
                return;
            }
            paths.push(...listed.files.filter((path) => FONT_FILE.test(path)));
            if (depth < MAX_DEPTH) for (const sub of listed.folders) await walk(sub, depth + 1);
        };
        await walk(normalizePath(folder), 0);
        return Promise.all(paths.slice(0, MAX_FILES).map((path) => this.#location(path)));
    }

    async #location(path: string): Promise<FontLocation> {
        const adapter = this.#app.vault.adapter;
        const stat = await adapter.stat(path).catch(() => null);
        return {id: vaultId(path, stat?.mtime ?? 0), open: async () => memoryFile(await adapter.readBinary(path))};
    }
}
