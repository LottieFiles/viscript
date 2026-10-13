import assert from "node:assert/strict";
import {markdownToHtml} from "../media/markdown.mjs";

const html = markdownToHtml(`# Title

An **animated** description with \`scene:play()\`.

- First source
- [Documentation](https://example.com/docs)

\`\`\`lua
return vis.scene {}
\`\`\`

<script>alert("unsafe")</script>`);

assert.match(html, /<h1>Title<\/h1>/);
assert.match(html, /<strong>animated<\/strong>/);
assert.match(html, /<code>scene:play\(\)<\/code>/);
assert.match(html, /<ul>/);
assert.match(html, /href="https:\/\/example\.com\/docs"/);
assert.match(html, /<pre><code data-language="lua">/);
assert.doesNotMatch(html, /<script>/);
assert.match(html, /&lt;script&gt;/);

console.log("safe Markdown rendering passed");
