import { mkdir, writeFile } from "node:fs/promises"
import path from "node:path"
import { toHtml } from "hast-util-to-html"
import { visit } from "unist-util-visit"
import type { Element } from "hast"
import type { QuartzEmitterPlugin } from "./quartz/plugins/types"
import type { ProcessedContent } from "./quartz/plugins/vfile"
import type { FilePath } from "./quartz/util/path"

function metadata([, file]: ProcessedContent) {
  const frontmatter = file.data.frontmatter ?? {}
  const source = String(file.data.relativePath).replaceAll("\\", "/")
  const slug = String(file.data.slug)
  const id = String(frontmatter.id ?? path.posix.basename(slug).replace(/^[\d.]+-/, ""))
  return {
    id,
    title: String(frontmatter.title ?? id),
    parent: frontmatter.parent ? String(frontmatter.parent) : null,
    order: Number(frontmatter.order ?? 0),
    path: `${slug}.html`,
    slug,
    source,
  }
}

// Keep all routing in the existing site's #/api tree while supporting ordinary
// relative Markdown links and Obsidian wikilinks in the source vault.
function rewriteLinks(tree: ProcessedContent[0], source: string, pages: ReturnType<typeof metadata>[]) {
  const bySource = new Map(pages.map(page => [page.source, page.id]))
  const bySlug = new Map(pages.map(page => [page.slug, page.id]))
  const byTitle = new Map(pages.map(page => [page.title, page.id]))

  const byFilename = new Map<string, string | null>()
  for (const page of pages) {
    const name = path.posix.basename(page.slug)
    byFilename.set(name, byFilename.has(name) ? null : page.id)
  }
  const ownId = bySource.get(source)

  visit(tree, "element", (node: Element) => {
    if (node.tagName !== "a" || typeof node.properties.href !== "string") return
    const href = node.properties.href
    if (/^(?:[a-z][a-z\d+.-]*:|\/\/|#\/)/i.test(href)) return
    const [target, anchor] = href.split("#", 2)
    if (!target) {
      if (ownId && anchor) node.properties.href = `#/api/${ownId}/${anchor}`
      return
    }
    let decoded: string
    try { decoded = decodeURIComponent(target) } catch { return }
    const relative = path.posix.normalize(path.posix.join(path.posix.dirname(source), decoded))
    const clean = (value: string) => value.replace(/\.(?:md|html)$/, "").replace(/\/$/, "")
    const id = bySource.get(relative) ?? bySlug.get(clean(relative)) ??
      bySlug.get(clean(decoded.replace(/^\//, ""))) ?? byFilename.get(clean(decoded)) ?? byTitle.get(decoded)
    if (id) node.properties.href = `#/api/${id}${anchor ? `/${anchor}` : ""}`
  })
}

function orderedPages(content: ProcessedContent[]) {
  const pages = content.map(metadata)
  const children = new Map<string | null, typeof pages>()
  for (const page of pages) {
    const siblings = children.get(page.parent) ?? []
    siblings.push(page)
    children.set(page.parent, siblings)
  }
  for (const siblings of children.values()) {
    siblings.sort((a, b) => a.order - b.order || a.title.localeCompare(b.title, undefined, { numeric: true }))
  }
  const ordered: typeof pages = []
  const visited = new Set<string>()
  const append = (parent: string | null) => {
    for (const page of children.get(parent) ?? []) {
      if (visited.has(page.id)) throw new Error(`Duplicate or cyclic API page: ${page.id}`)
      visited.add(page.id)
      ordered.push(page)
      append(page.id)
    }
  }
  append(null)
  if (ordered.length !== pages.length) throw new Error("API page tree contains an unknown parent or cycle")
  return ordered
}

function apiGraph(pages: ReturnType<typeof metadata>[]) {
  const byId = new Map(pages.map(page => [page.id, page]))
  const nodes = pages.map(page => {
    let category = page
    while (category.parent && byId.get(category.parent)?.parent) {
      category = byId.get(category.parent)!
    }
    return { id: page.id, title: page.title, parent: page.parent, category: category.id }
  })
  type Link = { source: string, target: string, kind: "link" | "parent" }
  const links = new Map<string, Link>()
  const connect = (source: string, target: string, kind: Link["kind"]) => {
    if (source === target || !byId.has(source) || !byId.has(target)) return
    const [left, right] = [source, target].sort()
    const key = `${left}\0${right}`
    if (!links.has(key) || kind === "link") links.set(key, { source: left, target: right, kind })
  }
  for (const page of pages) {
    if (page.parent) connect(page.id, page.parent, "parent")
  }
  return {
    collect(tree: ProcessedContent[0], source: string) {
      visit(tree, "element", (node: Element) => {
        if (node.tagName !== "a" || typeof node.properties.href !== "string") return
        const target = node.properties.href.match(/^#\/api\/([^/]+)(?:\/.*)?$/)?.[1]
        if (target) connect(source, target, "link")
      })
    },
    data() {
      return { nodes, links: [...links.entries()].sort(([a], [b]) => a.localeCompare(b)).map(([, link]) => link) }
    },
  }
}

export const ApiContent: QuartzEmitterPlugin = () => {
  const emit: ReturnType<QuartzEmitterPlugin>["emit"] = async function* ({ argv }, content) {
    const pages = orderedPages(content)
    const graph = apiGraph(pages)
    for (const item of content) {
      const page = metadata(item)
      rewriteLinks(item[0], page.source, pages)
      graph.collect(item[0], page.id)
      const output = path.join(argv.output, page.path)
      await mkdir(path.dirname(output), { recursive: true })
      const article: Element = {
        type: "element",
        tagName: "article",
        properties: { className: ["prose", "quartz-api"], "data-api-id": page.id },
        children: item[0].children,
      }
      await writeFile(output, toHtml(article, { allowDangerousHtml: true }) + "\n")
      yield output as FilePath
    }
    const manifest = path.join(argv.output, "manifest.json")
    await mkdir(argv.output, { recursive: true })
    await writeFile(manifest, JSON.stringify(pages, null, 2) + "\n")
    yield manifest as FilePath
    const graphPath = path.join(argv.output, "graph.json")
    await writeFile(graphPath, JSON.stringify(graph.data(), null, 2) + "\n")
    yield graphPath as FilePath
  }
  return { name: "ApiContent", emit, partialEmit: emit }
}
