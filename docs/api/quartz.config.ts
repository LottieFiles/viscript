import type { QuartzConfig } from "./quartz/cfg"
import * as Plugin from "./quartz/plugins"
import rehypeSlug from "rehype-slug"
import { Viscript } from "./plugins/viscript"
import { ApiContent } from "./render-content"

// Quartz renders only the article body. The existing VISCRIPT site owns the
// navigation, page tree, appearance and player lifecycle.
const config: QuartzConfig = {
  configuration: {
    pageTitle: "VISCRIPT API",
    enableSPA: false,
    enablePopovers: false,
    analytics: null,
    locale: "en-US",
    ignorePatterns: [".obsidian", ".trash", "templates", "private"],
    defaultDateType: "modified",
    theme: {
      fontOrigin: "local",
      cdnCaching: false,
      typography: { header: "Arial", body: "Arial", code: "Menlo" },
      colors: {
        lightMode: {
          light: "#ffffff", lightgray: "#e4e7ec", gray: "#667085",
          darkgray: "#344054", dark: "#101828", secondary: "#e0561f",
          tertiary: "#e0561f", highlight: "rgba(224,86,31,.08)", textHighlight: "#ff7a4533",
        },
        darkMode: {
          light: "#0b0c0f", lightgray: "#22262f", gray: "#667085",
          darkgray: "#c4c9d4", dark: "#f5f6f8", secondary: "#ff7a45",
          tertiary: "#ff7a45", highlight: "rgba(255,122,69,.12)", textHighlight: "#ff7a4533",
        },
      },
    },
  },
  plugins: {
    transformers: [
      Plugin.FrontMatter(),
      Viscript(),
      Plugin.ObsidianFlavoredMarkdown({ enableInHtmlEmbed: false }),
      Plugin.GitHubFlavoredMarkdown({ linkHeadings: false }),
      { name: "ApiHeadingIds", htmlPlugins: () => [rehypeSlug] },
      Plugin.SyntaxHighlighting({
        theme: { light: "github-light", dark: "github-dark" },
        keepBackground: false,
      }),
      Plugin.TableOfContents({ maxDepth: 2 }),
      Plugin.Description(),
    ],
    filters: [Plugin.RemoveDrafts()],
    emitters: [ApiContent()],
  },
}

export default config
