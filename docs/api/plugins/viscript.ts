import type { Root, RootContent, Code, Heading } from "mdast"
import type { QuartzTransformerPlugin } from "../quartz/plugins/types"

// The Markdown stays usable in Obsidian: the source is an ordinary viscript fence,
// and optional comments only identify paired examples and coverage for validation.
export const Viscript: QuartzTransformerPlugin = () => ({
  name: "Viscript",
  markdownPlugins() {
    return [() => (tree: Root) => {
      let count = 0
      const text = (node: RootContent | Heading): string => {
        if ("value" in node) return String(node.value)
        return "children" in node ? node.children.map((child) => text(child as RootContent)).join("") : ""
      }
      const walk = (children: RootContent[]) => {
        let id: string | undefined
        let label = ""
        for (const [index, node] of children.entries()) {
          if (node.type === "heading") {
            label = text(node)
            id = undefined
          }
          if (node.type === "html") {
            const marker = /<!--\s*api-example:\s*([^\s>]+)\s*-->/.exec(node.value)
            if (marker) id = marker[1]
          }
          if (node.type === "code") {
            const language = node.lang ?? ""
            const kind = language === "viscript" || language === "viscript-lua" ? "lua"
              : language === "viscript-js" ? "js"
              : id && /^(?:javascript|js)$/.test(language) ? "host" : null
            if (!kind) continue
            id ??= `scene-${++count}`
            const properties = {
              "data-viscript": kind,
              "data-api-example": id,
              "data-example-label": label || id,
              "data-source": node.value,
              "data-interactive": String(/\binteractive\b/.test(node.meta ?? "")),
            }
            // Let Quartz highlight these sources with its normal Lua/JS grammars.
            ;(node as Code).lang = kind === "lua" ? "lua" : "javascript"
            // Pretty Code replaces <pre>/<code> properties when highlighting.
            // An ordinary container keeps the example metadata around that work.
            children[index] = {
              type: "blockquote",
              data: {hName: "div", hProperties: properties},
              children: [node],
            }
          }
          if ("children" in node) walk(node.children as RootContent[])
        }
      }
      walk(tree.children)
    }]
  },
})
