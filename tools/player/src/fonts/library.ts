// Fonts a host offers beyond the ones it bundles: files the user picked, a font folder, or the
// fonts installed on the system. The engine knows fonts by name and draws TrueType and CFF
// outlines, so faces are looked up by name, the regular face preferred, and handed over as
// standalone files.
import {
    extractedSize, extractFace, MAX_FONT_BYTES, readCharset, readFaces, type Charset, type FontFace, type FontFile,
} from "./sfnt.ts";

/** A font file the library can open when it needs to read it. */
export interface FontLocation {
    /** Stable across runs, such as the file's path. */
    readonly id: string;
    open(): Promise<OpenFontFile>;
}

export interface OpenFontFile extends FontFile {
    close?(): void | Promise<void>;
}

/** One face of one file. */
export interface LibraryFace {
    /** The location's id, and the face's index in a collection. */
    readonly id: string;
    readonly face: FontFace;
    readonly location: FontLocation;
}

const normal = (name: string) => name.toLowerCase().replace(/[\s_-]+/g, "");

async function using<T>(location: FontLocation, read: (file: FontFile) => Promise<T>): Promise<T> {
    const file = await location.open();
    try {
        return await read(file);
    } finally {
        await file.close?.();
    }
}

export interface LibraryOptions {
    /**
     * Keep each face's bytes once extracted; true unless given. A host whose render server keeps
     * the bytes it is sent can leave them out, so large fonts are not held twice.
     */
    readonly cacheBytes?: boolean;
}

export class FontLibrary {
    readonly #faces = new Map<string, LibraryFace>();
    readonly #bytes = new Map<string, Promise<ArrayBuffer>>();
    readonly #charsets = new Map<string, Promise<Charset>>();
    readonly #cacheBytes: boolean;

    constructor({cacheBytes = true}: LibraryOptions = {}) {
        this.#cacheBytes = cacheBytes;
    }

    /**
     * Lists the faces in each location, keeping those the engine can draw: TrueType or CFF
     * outlines, in a file it accepts once extracted. A file that cannot be read, or is not a font,
     * is skipped.
     */
    async add(locations: Iterable<FontLocation>): Promise<LibraryFace[]> {
        const added: LibraryFace[] = [];
        for (const location of locations) {
            let faces: FontFace[];
            try {
                faces = await using(location, readFaces);
            } catch {
                continue;
            }
            for (const face of faces) {
                if (face.outlines === "other" || !face.family || extractedSize(face) > MAX_FONT_BYTES) continue;
                const entry = {id: face.collection ? `${location.id}#${face.index}` : location.id, face, location};
                this.#faces.set(entry.id, entry);
                added.push(entry);
            }
        }
        return added;
    }

    get size(): number {
        return this.#faces.size;
    }

    get(id: string): LibraryFace | undefined {
        return this.#faces.get(id);
    }

    /** Every face, in the order the locations were added. */
    faces(): LibraryFace[] {
        return [...this.#faces.values()];
    }

    /** Family names, as a settings list would show them. */
    families(): string[] {
        return [...new Set([...this.#faces.values()].map(({face}) => face.family))].sort((a, b) => a.localeCompare(b));
    }

    /**
     * The face named `name`: a full or PostScript name picks that face; a family name, in any of
     * the font's languages, picks its most regular face, TrueType before CFF, which the engine
     * reads less of. Case, spaces and dashes are ignored.
     */
    find(name: string): LibraryFace | undefined {
        const wanted = normal(name);
        if (!wanted) return undefined;
        let best: LibraryFace | undefined, bestScore = Infinity;
        for (const entry of this.#faces.values()) {
            const {face} = entry;
            if (normal(face.fullName) === wanted || normal(face.postscriptName) === wanted) return entry;
            if (!face.names.some((alias) => normal(alias) === wanted)) continue;
            const score = Math.abs(face.weight - 400) + (face.italic ? 1000 : 0) + (face.outlines === "cff" ? 0.5 : 0);
            if (score < bestScore) {
                best = entry;
                bestScore = score;
            }
        }
        return best;
    }

    /** The face as a standalone font file for the engine. */
    bytes(id: string): Promise<ArrayBuffer> {
        const entry = this.#faces.get(id);
        if (!entry) return Promise.reject(new Error(`Unknown font: ${id}`));
        let pending = this.#bytes.get(id);
        if (!pending) {
            pending = using(entry.location, async (file) => {
                const data = await extractFace(file, entry.face);
                return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength) as ArrayBuffer;
            });
            // Kept while it is read, so requests at the same time share one read.
            this.#bytes.set(id, pending);
            const forget = () => { if (this.#bytes.get(id) === pending) this.#bytes.delete(id); };
            pending.then(() => { if (!this.#cacheBytes) forget(); }, forget);
        }
        return pending;
    }

    /** Which code points the face has glyphs for, read once; undefined for an unknown face. */
    async charset(id: string): Promise<Charset | undefined> {
        const entry = this.#faces.get(id);
        if (!entry) return undefined;
        let charset = this.#charsets.get(id);
        if (!charset) {
            charset = using(entry.location, (file) => readCharset(file, entry.face));
            this.#charsets.set(id, charset);
            charset.catch(() => this.#charsets.delete(id));
        }
        return charset;
    }

    /** Whether the face has a glyph for every one of `codePoints`. */
    async covers(id: string, codePoints: Iterable<number>): Promise<boolean> {
        const has = await this.charset(id);
        if (!has) return false;
        for (const codePoint of codePoints) if (!has(codePoint)) return false;
        return true;
    }

    /** Forgets every face, as before scanning the folders again. */
    clear(): void {
        this.#faces.clear();
        this.#bytes.clear();
        this.#charsets.clear();
    }
}
