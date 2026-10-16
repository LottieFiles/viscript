// Migrated from the supplied ThorVG scenes and gitpage's source-backed renderer notes.
// Original trace revisions and literal pixel colors remain in the Lua files;
// structural colors follow the same light/dark theme as the other examples.
const episode = ({file, ...details}) => ({
  ...details, id: `thorvg-${file}`, src: new URL(`./thorvg/${file}.lua`, import.meta.url),
});

export const THORVG = {
  id: "thorvg",
  title: "ThorVG",
  tags: "CPU · OpenGL · renderer internals",
  summary: "Source-backed visual traces of ThorVG's rasterization, task scheduling, fill rules, and stencil tests.",
  episodes: [
    {file: "shape-stroke", title: "Fill + stroke to pixels", category: "CPU", loop: true,
     blurb: "Follow shapestroke5.lua from path commands through fill and stroke outlines, RLE spans, coverage, and the final surface. Span arrays and pixel colors come from the original ThorVG trace."},
    {file: "threads", title: "SW / GL thread scheduling", category: "Scheduling", loop: true,
     blurb: "Compare caller and worker activity across update(), draw(), and sync(). SW workers prepare shapes; the GL caller records tasks and later issues drawing commands. Horizontal spacing shows event order."},
    {file: "cpu-task", title: "CPU: SwShapeTask", category: "CPU", loop: true,
     blurb: "A star becomes an outline and shape.rle on a worker. The caller waits for task->done(), rasterizes the spans, and keeps render data available for the next frame."},
    {file: "gl-fillrule", title: "GL: FillRule", category: "OpenGL", loop: true,
     blurb: "Accumulate stencil values across a self-crossing path, then compare FillNonZero and FillEvenOdd. The same center samples are covered under the nonzero rule and skipped under the even-odd rule."},
    {file: "gl-stencil-test", title: "GL: Stencil test", category: "OpenGL",
     blurb: "Follow a circle, triangle, and rectangle through the fragment shader, stencil test, stencil operation, and color write. Compare ALWAYS, EQUAL, and NOTEQUAL against the same stencil buffer."},
  ].map(episode),
};

export const THORVG_INTERACTION = {
  id: "thorvg-interaction",
  title: "ThorVG & Interaction",
  tags: "OpenGL · selectable stages · playback ranges",
  summary: "Select a renderer stage or draw input to replay the corresponding source-backed animation.",
  episodes: [
    {file: "gl-stencil-cover", title: "GlStencilCoverTask", category: "OpenGL",
     blurb: "Follow the same ten-point star as CPU: SwShapeTask through fan triangles, stencil values, the color cover, and the final color buffer. Select a stage header to replay it; all 32 × 32 samples are computed from the path.",
     clips: ["RT_Stencil", "depthStencilBuffer", "RT_Color cover", "colorBuffer"].map((label, k) => ({
       label, bounds: [(20 + k * 193) / 800, 84 / 720, 181 / 800, 46 / 720],
       range: [k === 0 ? 0 : k * 6.4 + 0.2, k * 6.4 + 6.2],
     }))},
    {file: "gl-draw-path", title: "GPU Draw Path Selection", category: "OpenGL",
     blurb: "Select a shape card to trace renderShape() to GlSolidBatch, GlRenderTask, or GlStencilCoverTask. Compare solid fill, opaque stroke, and gradient branches.",
     clips: ["back fill Color", "star fill Color", "star stroke a=255", "star LinearGradient"].map((label, k) => ({
       label, bounds: [20 / 800, (84 + k * 154) / 720, 210 / 800, 142 / 720],
       range: [k * 0.6 + 0.2, k * 0.6 + 0.56],
     }))},
  ].map(episode),
};
