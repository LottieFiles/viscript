export function markdownToHtml(source) {
    const lines = String(source || "").replaceAll("\r\n", "\n").split("\n");
    const output = [];
    let paragraph = [];
    let list = "";
    let code = null;

    const flushParagraph = () => {
        if (!paragraph.length) return;
        output.push(`<p>${inlineMarkdown(paragraph.join(" "))}</p>`);
        paragraph = [];
    };
    const closeList = () => {
        if (!list) return;
        output.push(`</${list}>`);
        list = "";
    };

    for (const line of lines) {
        const fence = /^```\s*([A-Za-z0-9_-]*)\s*$/.exec(line);
        if (code) {
            if (fence) {
                output.push(`<pre><code${code.language ? ` data-language="${escapeHtml(code.language)}"` : ""}>${escapeHtml(code.lines.join("\n"))}</code></pre>`);
                code = null;
            } else code.lines.push(line);
            continue;
        }
        if (fence) {
            flushParagraph();
            closeList();
            code = {language: fence[1], lines: []};
            continue;
        }
        if (!line.trim()) {
            flushParagraph();
            closeList();
            continue;
        }
        const heading = /^(#{1,4})\s+(.+)$/.exec(line);
        if (heading) {
            flushParagraph();
            closeList();
            const level = heading[1].length;
            output.push(`<h${level}>${inlineMarkdown(heading[2])}</h${level}>`);
            continue;
        }
        if (/^\s*(?:---+|___+)\s*$/.test(line)) {
            flushParagraph();
            closeList();
            output.push("<hr>");
            continue;
        }
        const unordered = /^\s*[-*]\s+(.+)$/.exec(line);
        const ordered = /^\s*\d+[.)]\s+(.+)$/.exec(line);
        if (unordered || ordered) {
            flushParagraph();
            const nextList = unordered ? "ul" : "ol";
            if (list !== nextList) {
                closeList();
                list = nextList;
                output.push(`<${list}>`);
            }
            output.push(`<li>${inlineMarkdown((unordered ?? ordered)?.[1] ?? "")}</li>`);
            continue;
        }
        const quote = /^\s*>\s?(.*)$/.exec(line);
        if (quote) {
            flushParagraph();
            closeList();
            output.push(`<blockquote><p>${inlineMarkdown(quote[1])}</p></blockquote>`);
            continue;
        }
        closeList();
        paragraph.push(line.trim());
    }
    if (code) output.push(`<pre><code>${escapeHtml(code.lines.join("\n"))}</code></pre>`);
    flushParagraph();
    closeList();
    return output.join("\n");
}

function inlineMarkdown(source) {
    const code = [];
    let value = escapeHtml(source).replace(/`([^`]+)`/g, (_match, content) => {
        const index = code.push(`<code>${content}</code>`) - 1;
        return `\u0000${index}\u0000`;
    });
    value = value.replace(/\[([^\]]+)]\((https?:\/\/[^\s)]+)\)/g, '<a href="$2">$1</a>');
    value = value.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
    value = value.replace(/__([^_]+)__/g, "<strong>$1</strong>");
    value = value.replace(/(^|[^*])\*([^*]+)\*(?!\*)/g, "$1<em>$2</em>");
    return value.replace(/\u0000(\d+)\u0000/g, (_match, index) => code[Number(index)] ?? "");
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#39;");
}
