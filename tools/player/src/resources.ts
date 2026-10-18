// A fixed table of images a host offers its scenes, fetched once each on first use. Fonts come
// from the host's FontProvider instead, by the names scenes use (see fonts/provider.ts).
import type {AssetProvider} from "./host/host.ts";
import type {SceneLanguage, SceneResources} from "./protocol.ts";

export interface AssetSource {
    readonly mime: string;
    readonly url?: string | URL;
    readonly load?: () => Promise<ArrayBuffer>;
}

export interface CatalogOptions {
    /** Asset name, as scenes load it → source. */
    readonly assets: Readonly<Record<string, AssetSource>>;
    /** The names of the images one scene loads. */
    readonly select: (source: string, language: SceneLanguage) => readonly string[];
}

export interface Catalog extends AssetProvider {
    resources(source: string, language: SceneLanguage): SceneResources;
}

async function download(url: string | URL): Promise<ArrayBuffer> {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`Could not load resource: ${url} (${response.status})`);
    return response.arrayBuffer();
}

export function createCatalog({assets, select}: CatalogOptions): Catalog {
    const cache = new Map<string, Promise<ArrayBuffer>>();
    return {
        resources(source, language) {
            return {
                assets: select(source, language).map((name) => {
                    const asset = assets[name];
                    if (!asset) throw new Error(`Unknown asset: ${name}`);
                    return {id: name, name, mime: asset.mime};
                }),
            };
        },
        bytes(id) {
            const source = Object.hasOwn(assets, id) ? assets[id] : undefined;
            if (!source) return Promise.reject(new Error(`Unknown resource: ${id}`));
            let pending = cache.get(id);
            if (!pending) {
                pending = (source.load ?? (() => download(source.url!)))();
                cache.set(id, pending);
                // A failed download is tried again by the next scene that needs it.
                pending.catch(() => cache.delete(id));
            }
            return pending;
        },
    };
}
