// Fonts from Fontsource (fontsource.org), served by jsDelivr, which WebCanvas (thorvg.web) loads
// fonts from by default. Asked for a family, the provider looks it up in Fontsource's API for the
// files it has, then fetches one TrueType file: in the weight nearest the one asked for, and in
// the font's Korean, Japanese or Chinese subset where it has one, since those files hold its
// Latin letters too, where WebCanvas takes the Latin subset. A family Fontsource does not have is
// no font (null); a network failure rejects, so a later load asks again.
import type {FontProvider, FontProviderResult} from "./provider.ts";

export const FONTSOURCE_API = "https://api.fontsource.org/v1/fonts/";
export const FONTSOURCE_CDN = "https://cdn.jsdelivr.net/fontsource/fonts/";

/** Subsets that hold a font's Latin letters along with their own, preferred in this order. */
const WIDE_SUBSETS = ["korean", "japanese", "chinese-simplified", "chinese-traditional", "chinese-hongkong"];

export interface FontsourceOptions {
    /** 400 unless given; the font's nearest weight is fetched. */
    readonly weight?: number;
    readonly style?: "normal" | "italic";
    /** One subset, as Fontsource names them, instead of the one chosen as above. */
    readonly subset?: string;
}

/** An answer to a GET request, as fetch() or a host's own request function gives it. */
export interface FontsourceResponse {
    readonly status: number;
    readonly body: ArrayBuffer;
}

export interface FontsourceProviderOptions extends FontsourceOptions {
    /** Sends a GET request; fetch() unless given, which a host may replace, as Obsidian does. */
    readonly request?: (url: string) => Promise<FontsourceResponse>;
}

/** What the API tells of a font: its files, by weight, style and subset. */
interface Metadata {
    readonly subsets?: readonly string[];
    readonly weights?: readonly number[];
    readonly styles?: readonly string[];
    readonly variants?: Readonly<Record<string, Readonly<Record<string, Readonly<Record<string, {readonly url?: {readonly ttf?: string}}>>>>>>;
}

/** Fontsource's id for a family, as WebCanvas makes it: "Noto Sans KR" is "noto-sans-kr". */
export function fontsourceId(family: string): string {
    return family.trim().toLowerCase().replace(/\s+/g, "-").replace(/[^a-z0-9-]/g, "");
}

const request = async (url: string): Promise<FontsourceResponse> => {
    const response = await fetch(url);
    return {status: response.status, body: await response.arrayBuffer()};
};

const nearest = (values: readonly number[], wanted: number) =>
    values.reduce((best, value) => Math.abs(value - wanted) < Math.abs(best - wanted) ? value : best, values[0] ?? wanted);

export class FontsourceProvider implements FontProvider {
    readonly #options: FontsourceOptions;
    readonly #request: (url: string) => Promise<FontsourceResponse>;

    constructor({request: send = request, ...options}: FontsourceProviderOptions = {}) {
        this.#options = options;
        this.#request = send;
    }

    async fetch(name: string, options: Readonly<Record<string, unknown>> = {}): Promise<FontProviderResult | null> {
        const id = fontsourceId(name);
        if (!id || id.length > 100) return null;
        const wanted = {...this.#options, ...options} as FontsourceOptions;
        const found = await this.#request(FONTSOURCE_API + id);
        if (found.status === 404) return null;
        if (found.status !== 200) throw new Error(`Fontsource answered ${found.status} for ${name}`);
        const metadata = JSON.parse(new TextDecoder().decode(found.body)) as Metadata;
        const subsets = metadata.subsets ?? [];
        const subset = wanted.subset ?? WIDE_SUBSETS.find((wide) => subsets.includes(wide))
            ?? (subsets.includes("latin") || !subsets.length ? "latin" : subsets[0]!);
        const weight = nearest(metadata.weights ?? [], wanted.weight ?? 400);
        const styles = metadata.styles ?? [];
        const style = styles.includes(wanted.style ?? "normal") || !styles.length ? wanted.style ?? "normal" : styles[0]!;
        // The API's own link to the file, unless it leads anywhere but the CDN.
        const listed = metadata.variants?.[weight]?.[style]?.[subset]?.url?.ttf;
        const url = listed?.startsWith(FONTSOURCE_CDN) ? listed : `${FONTSOURCE_CDN}${id}@latest/${subset}-${weight}-${style}.ttf`;
        const file = await this.#request(url);
        if (file.status === 404) return null;
        if (file.status !== 200) throw new Error(`Fontsource answered ${file.status} for ${url}`);
        return {data: file.body, type: "ttf"};
    }
}
