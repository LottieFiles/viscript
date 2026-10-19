// The fonts the preview gives a scene. Every load starts a WebAssembly instance that copies each
// font registered in it, so the preview fetches a font once, when a scene first sets text in it,
// and registers in each instance only the fonts the scene needs: the ones its document needed
// last time, then any the loaded scene still lacks, after which it loads again, since its text
// was measured in the default font. A scene's texts get their fonts as the scene is built, so the
// first load names every font the timeline will set text in, later ones included.

// Font bytes by the names scenes set text in, each fetched on first use; a failed fetch is tried
// again the next time.
export function createFontCache(urls, fetchFont = (url) => fetch(url)) {
    const fonts = new Map();
    return {
        has: (name) => Object.hasOwn(urls, name),
        bytes(name) {
            let font = fonts.get(name);
            if (!font) {
                font = fetchFont(urls[name]).then(async (response) => {
                    if (!response.ok) throw new Error(`Bundled font ${name} failed to load (${response.status})`);
                    return new Uint8Array(await response.arrayBuffer());
                });
                fonts.set(name, font);
                font.catch(() => fonts.get(name) === font && fonts.delete(name));
            }
            return font;
        },
    };
}

// Loads a scene with load(runtime) and returns the names of the fonts it gave the runtime: those
// in expected and the cache has, registered first, and those the loaded scene still lacks, after
// which the scene loads again. Stops when current() turns false while fonts are fetched.
export async function loadWithFonts(runtime, load, fonts, {expected = [], current = () => true} = {}) {
    const given = [];
    const give = async (names) => {
        const wanted = [...new Set(names)].filter((name) => fonts.has(name) && !given.includes(name));
        const data = await Promise.all(wanted.map((name) => fonts.bytes(name)));
        if (!current()) return false;
        wanted.forEach((name, index) => runtime.font(name, data[index], "ttf"));
        given.push(...wanted);
        return wanted.length > 0;
    };
    await give(expected);
    if (!current()) return given;
    load(runtime);
    if (await give(runtime.missingFonts())) load(runtime);
    return given;
}
