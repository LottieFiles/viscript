// The fonts and images the page's scenes can use. Pure data, so scripts/check-page.mjs renders
// every scene with exactly what the page gives it.

// The font the page registers under each name scenes set text in, fetched when a scene first
// needs it; text in any other font is drawn in the engine's default font. "Pretendard" is the
// official Pretendard Std, the same Latin, Greek and Cyrillic glyphs without Hangul and CJK,
// 0.2 MB gzipped instead of 1.2 MB, until a scene needs the full font (FULL_PRETENDARD).
export const FONTS = new Map([
  ["Pretendard", "runtime/PretendardStd-Regular.ttf"],
  ["JetBrainsMono", "runtime/JetBrainsMono.ttf"],
  ["Source Serif 4", "runtime/SourceSerif4-Semibold.ttf"],
  // adaptive_vscode sets text with Hangul in it in this font instead of Source Serif 4
  ["IBM Plex Sans KR", "runtime/IBMPlexSansKR-SemiBold.ttf"],
  ["CMUTypewriter-Regular", "assets/thorvg/CMUTypewriter-Regular.ttf"],
  ["CMUSerif-Roman", "assets/thorvg/CMUSerif-Roman.ttf"],
]);
export const FULL_PRETENDARD = "runtime/Pretendard.ttf";
// name the scene loads -> file and type
export const ASSETS = new Map([["earth.png", {path: "assets/earth.png", mime: "png"}]]);
// Code points only the full Pretendard has (generated from both fonts' cmaps), or escapes and
// char-code calls that could build them at run time.
const FULL_LETTERS = /[ɩ‌-‍⁠Ω∆≪-≫⋯〈-〉▪▶▻◄-◅➀-➉⦅-⦆　-〃〈-】〔-〟ぁ-ゔ゙-ゞァ-ヾㄱ-ㅤㆍ㈜㎀-㎄㎈-㏊㏏-㏐㏓㏖㏘㏛-㏝가-힣！-｠ﾡ-ﾾￂ-ￇￊ-ￏￒ-ￗￚ-ￜ￠-￡￥-￦\u{1f10b}]|\\u|utf8\.char|fromCharCode|fromCodePoint/u;

// Whether a scene may show letters only the full Pretendard has. The page then registers the full
// font under "Pretendard" for every scene that loads after it.
export const needsFullPretendard = (source) => FULL_LETTERS.test(source);

// The images a scene loads: those it names.
export const assetsFor = (source) => [...ASSETS.keys()].filter((name) => source.includes(name));
