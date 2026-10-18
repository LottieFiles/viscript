import assert from "node:assert/strict";
import test, {afterEach, mock} from "node:test";
import {FONTSOURCE_API, FONTSOURCE_CDN, fontsourceId, FontsourceProvider, type FontsourceResponse} from "../src/fonts/fontsource.ts";
import {chainFonts, fontTable, fontType, runtimeFonts, type FontProvider} from "../src/fonts/provider.ts";

afterEach(() => mock.restoreAll());

const TTF = new Uint8Array([0, 1, 0, 0, 7]);
const OTF = new Uint8Array([0x4f, 0x54, 0x54, 0x4f, 7]);

// fetch() answering each URL from `files`, with 404 for any other.
function serve(files: Record<string, Uint8Array<ArrayBuffer>>) {
    const asked: string[] = [];
    mock.method(globalThis, "fetch", async (url: string | URL) => {
        asked.push(String(url));
        const body = files[String(url)];
        return new Response(body ?? null, {status: body ? 200 : 404});
    });
    return asked;
}

test("a font's type is read from its first bytes", () => {
    assert.equal(fontType(TTF), "ttf");
    assert.equal(fontType(OTF.buffer), "otf");
    assert.equal(fontType(new Uint8Array(2)), "ttf");
});

test("a table answers its own names only, fetching each once, and again after a failure", async () => {
    const asked = serve({"https://fonts.test/Mono.ttf": TTF});
    let tries = 0;
    const table = fontTable({
        Mono: "https://fonts.test/Mono.ttf",
        Gone: "https://fonts.test/Gone.ttf",
        Made: async () => OTF,
        Flaky: async () => {
            if (tries++ === 0) throw new Error("offline");
            return {data: TTF, type: "ttf" as const};
        },
        Nothing: async () => null,
    });
    assert.equal(await table.fetch("toString"), null);
    assert.equal(await table.fetch("Other"), null);
    assert.deepEqual([...new Uint8Array((await table.fetch("Mono"))!.data as ArrayBuffer)], [...TTF]);
    await table.fetch("Mono");
    assert.deepEqual(asked, ["https://fonts.test/Mono.ttf"]);
    await assert.rejects(table.fetch("Gone"), /404/);
    assert.deepEqual(await table.fetch("Made"), {data: OTF});
    await assert.rejects(table.fetch("Flaky"), /offline/);
    assert.deepEqual(await table.fetch("Flaky"), {data: TTF, type: "ttf"});
    assert.equal(await table.fetch("Nothing"), null);
});

test("a chain answers with the first font, passing over failures", async () => {
    const answers = (font: Uint8Array | null | Error): FontProvider => ({
        fetch: async () => {
            if (font instanceof Error) throw font;
            return font && {data: font};
        },
    });
    assert.deepEqual(await chainFonts(answers(null), false, answers(TTF), answers(OTF)).fetch("Any"), {data: TTF});
    assert.deepEqual(await chainFonts(answers(new Error("offline")), answers(OTF)).fetch("Any"), {data: OTF});
    await assert.rejects(chainFonts(answers(null), answers(new Error("offline")), answers(null)).fetch("Any"), /offline/);
    assert.equal(await chainFonts(answers(null), undefined).fetch("Any"), null);
    assert.equal(await chainFonts().fetch("Any"), null);
});

test("the runtime's fonts are its code font, beside the runtime", async () => {
    const asked = serve({"https://site.test/runtime/JetBrainsMono.ttf": TTF});
    const fonts = runtimeFonts("https://site.test/runtime");
    assert.ok(await fonts.fetch("JetBrainsMono"));
    assert.equal(await fonts.fetch("Pretendard"), null);
    assert.deepEqual(asked, ["https://site.test/runtime/JetBrainsMono.ttf"]);
});

// A request function answering Fontsource's API and CDN from `files`, 404 for anything else.
function fontsource(files: Record<string, unknown>) {
    const asked: string[] = [];
    const request = async (url: string): Promise<FontsourceResponse> => {
        asked.push(url);
        const file = files[url];
        if (file === undefined) return {status: 404, body: new ArrayBuffer(0)};
        if (file instanceof Error) throw file;
        if (typeof file === "number") return {status: file, body: new ArrayBuffer(0)};
        const body = file instanceof Uint8Array ? file : new TextEncoder().encode(JSON.stringify(file));
        return {status: 200, body: body.buffer.slice(body.byteOffset, body.byteOffset + body.byteLength) as ArrayBuffer};
    };
    return {asked, request};
}

const variant = (id: string, subset: string, weight: number, style = "normal") =>
    ({[weight]: {[style]: {[subset]: {url: {ttf: `${FONTSOURCE_CDN}${id}@latest/${subset}-${weight}-${style}.ttf`}}}}});

test("Fontsource ids are made from family names as WebCanvas makes them", () => {
    assert.equal(fontsourceId("Noto Sans KR"), "noto-sans-kr");
    assert.equal(fontsourceId("  Source  Serif 4 "), "source-serif-4");
    assert.equal(fontsourceId("../evil?x=1"), "evilx1");
});

test("Fontsource gives a font's Korean, Japanese or Chinese file, which has Latin too, else its Latin one", async () => {
    const korean = `${FONTSOURCE_CDN}noto-sans-kr@latest/korean-400-normal.ttf`;
    const latin = `${FONTSOURCE_CDN}jetbrains-mono@latest/latin-400-normal.ttf`;
    const {asked, request} = fontsource({
        [`${FONTSOURCE_API}noto-sans-kr`]: {subsets: ["cyrillic", "korean", "latin"], weights: [100, 400, 900], styles: ["normal"],
            variants: variant("noto-sans-kr", "korean", 400)},
        [korean]: TTF,
        [`${FONTSOURCE_API}jetbrains-mono`]: {subsets: ["cyrillic", "latin"], weights: [400], styles: ["italic", "normal"]},
        [latin]: TTF,
    });
    const provider = new FontsourceProvider({request});
    assert.deepEqual(await provider.fetch("Noto Sans KR"), {data: TTF.buffer, type: "ttf"});
    assert.ok(await provider.fetch("JetBrains Mono"));
    assert.deepEqual(asked, [`${FONTSOURCE_API}noto-sans-kr`, korean, `${FONTSOURCE_API}jetbrains-mono`, latin]);
});

test("Fontsource takes the nearest weight, the asked subset, and only files on its CDN", async () => {
    const bold = `${FONTSOURCE_CDN}pretendard@latest/latin-700-normal.ttf`;
    const {asked, request} = fontsource({
        [`${FONTSOURCE_API}pretendard`]: {subsets: ["latin"], weights: [100, 700], styles: ["normal"],
            variants: {700: {normal: {latin: {url: {ttf: "https://elsewhere.test/pretendard.ttf"}}}}}},
        [bold]: TTF,
    });
    assert.ok(await new FontsourceProvider({request, weight: 600}).fetch("Pretendard"));
    assert.equal(asked.at(-1), bold);
    await new FontsourceProvider({request}).fetch("Pretendard", {subset: "latin-ext"});
    assert.equal(asked.at(-1), `${FONTSOURCE_CDN}pretendard@latest/latin-ext-100-normal.ttf`);
});

test("a family Fontsource lacks is none; a failure to reach it rejects", async () => {
    const {request} = fontsource({
        [`${FONTSOURCE_API}busy`]: 503,
        [`${FONTSOURCE_API}offline`]: new Error("network down"),
        [`${FONTSOURCE_API}listed`]: {subsets: ["latin"], weights: [400], styles: ["normal"]},
    });
    const provider = new FontsourceProvider({request});
    assert.equal(await provider.fetch("Helvetica Neue"), null);
    assert.equal(await provider.fetch("!!!"), null);
    assert.equal(await provider.fetch("Listed"), null, "a listed font whose file is gone is none");
    await assert.rejects(provider.fetch("Busy"), /503/);
    await assert.rejects(provider.fetch("Offline"), /network down/);
});
