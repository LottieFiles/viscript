// What the CPU renderer and the GL module's renderer share: the retained paint tree,
// diagnostics and font loading.
#ifndef _VIS_TVG_RENDERER_H_
#define _VIS_TVG_RENDERER_H_

#include <thorvg.h>

#include "vis.h"

namespace vis
{

struct DrawCommand;

struct RetainedTree;

namespace renderer
{

using vis::RetainedTree;

// Why a renderer's most recent call failed, in plain words; empty when it did not.
struct TvgDiagnostic
{
    char message[256] = {};
    const char* text() const noexcept;
};

// Scopes one renderer call: clears the diagnostic as the call starts and, as it ends, keeps
// the reason a paint of the call could not be built, such as the font a text needs.
struct TvgDiagnosis
{
    explicit TvgDiagnosis(TvgDiagnostic& diagnostic) noexcept;
    ~TvgDiagnosis();
    TvgDiagnosis(const TvgDiagnosis&) = delete;
    TvgDiagnosis& operator=(const TvgDiagnosis&) = delete;

private:
    TvgDiagnostic& diagnostic;
};

// Loads a font file, or font data under a name, explaining a failure in the diagnostic.
Result tvgFont(TvgDiagnostic& diagnostic, const char* path) noexcept;
Result tvgFont(TvgDiagnostic& diagnostic, const char* name, const void* data, uint32_t size,
               const char* mime) noexcept;
bool tvgSuccess(tvg::Result result) noexcept;
// Updates canvas and draws it over a cleared target, waiting for the drawing; false when ThorVG
// fails a step.
bool tvgDraw(tvg::Canvas* canvas) noexcept;
void tvgRelease(tvg::Paint* paint) noexcept;
bool tvgAdd(tvg::Scene* scene, tvg::Paint* paint) noexcept;
tvg::Paint* tvgPaint(const DrawCommand& command) noexcept;
bool tvgVisible(const DrawCommand& command) noexcept;
// Retained paint tree shared by the CPU and GL renderers. It keeps one tvg::Paint per
// stable draw-command identity across frames, attached to the canvas once, and applies
// only the changes of each new DisplayList (see docs/architecture.md, "Retained rendering").
RetainedTree* tvgRetainedGen() noexcept;
void tvgRetainedFree(RetainedTree* tree) noexcept;
void tvgRetainedReset(RetainedTree* tree) noexcept;
// Brings the tree on canvas to scene at time, drawn scale pixels a unit, and draws the canvas.
Result tvgRetainedDraw(RetainedTree* tree, tvg::Canvas* canvas, const Scene* scene,
                       float time, float scale) noexcept;
bool tvgInit() noexcept;
void tvgTerm() noexcept;

}  // namespace renderer

}  // namespace vis

#endif
