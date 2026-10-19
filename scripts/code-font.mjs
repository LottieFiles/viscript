// Writes src/examples/assets/JetBrainsMono.ttf: the font the themes set code in, JetBrains Mono NL,
// without what ThorVG never reads, which halves it and leaves every character's outline and
// advance and the line metrics as they were. Gone are the layout tables, the TrueType hinting
// (ThorVG draws outlines unhinted), the glyph names and the glyphs no character reaches, which
// only layout tables could. Its names stay, as JetBrains Mono's license reserves none, and so do
// its copyright and license. scripts/sync-wasm.mjs copies it into the runtime bundles.
// Needs hb-subset from HarfBuzz.
// Usage: node scripts/code-font.mjs [JetBrainsMonoNL-Regular.ttf]   (by default, the font already there)
import {execFileSync} from "node:child_process";
import {mkdtempSync, readFileSync, rmSync, writeFileSync} from "node:fs";
import {tmpdir} from "node:os";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const output = join(root, "src/examples/assets/JetBrainsMono.ttf");
const source = process.argv[2] ? resolve(process.argv[2]) : output;

const scratch = mkdtempSync(join(tmpdir(), "viscript-font-"));
try {
    const stripped = join(scratch, "code.ttf");
    // hb-subset keeps only names 0 to 6 unless told otherwise, which would drop the license.
    execFileSync("hb-subset", [source, "--unicodes=*", "--drop-tables+=GSUB,GPOS,GDEF,DSIG,BASE,vmtx,vhea",
        "--no-hinting", "--notdef-outline", "--name-IDs=*", "--name-languages=*", `--output-file=${stripped}`],
    {stdio: "inherit"});
    const bytes = readFileSync(stripped);
    writeFileSync(output, bytes);
    console.log(`${output}: ${bytes.length} bytes of font`);
} finally {
    rmSync(scratch, {recursive: true, force: true});
}
