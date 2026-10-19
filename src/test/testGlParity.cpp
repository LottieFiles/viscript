// CPU vs GL parity: render the same scenes with both engines and compare RGBA.
// Set VIS_GL_PARITY_DIR to write <scene>-cpu.png, <scene>-gl.png, <scene>-diff.png.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "testGlContext.h"
#include "vis.h"
#include "vis_gl.h"
#include "testCheck.h"

using namespace vis;

static constexpr uint32_t Width = 320;
static constexpr uint32_t Height = 180;

struct Case
{
    const char* name;
    Scene* (*build)(float& time);
    float meanLimit;     // mean absolute channel difference (0..255)
    float outlierLimit;  // percent of pixels whose max channel difference exceeds 32
};

static Config config()
{
    Config cfg;
    cfg.width = Width;
    cfg.height = Height;
    cfg.background = Color::hex("#101820");
    cfg.camera.orthoHeight = 6.0f;
    return cfg;
}

static Scene* shapes(float& time)
{
    auto scene = Scene::gen(config());
    auto circle = Circle::gen({-3.2f, 0.6f, 0.0f}, 1.2f);
    auto rect = Rectangle::gen({0.0f, 0.6f, 0.0f}, {2.4f, 1.8f}, 0.3f);
    Vec3 tri[] = {{2.0f, -0.4f, 0.0f}, {4.4f, -0.4f, 0.0f}, {3.2f, 1.8f, 0.0f}};
    auto polygon = Polygon::gen(tri, 3);
    auto line = Line::gen({-4.5f, -1.8f, 0.0f}, {4.5f, -1.8f, 0.0f});
    auto arrow = Arrow::gen({-4.0f, -2.5f, 0.0f}, {4.0f, -2.5f, 0.0f});
    circle->stroke(Color::hex("#ffd166"), 4.0f).fill(Color::hex("#4cc9f0"));
    rect->stroke(Color::hex("#f72585"), 3.0f).fill(Color::hex("#3a0ca380"));
    polygon->stroke(Color::hex("#80ffdb"), 2.0f).fill(Color::hex("#2b9348"));
    line->stroke(Color::hex("#ffffff"), 3.0f);
    float pattern[] = {0.4f, 0.2f};
    line->dash(pattern, 2);
    arrow->stroke(Color::hex("#ff9f1c"), 2.5f);
    scene->add(circle);
    scene->add(rect);
    scene->add(polygon);
    scene->add(line);
    scene->add(arrow);
    time = 0.0f;
    return scene;
}

static Scene* text(float& time)
{
    auto scene = Scene::gen(config());
    auto title = Text::gen("viscript GL parity", {0.0f, 1.0f, 0.0f});
    auto body = Text::gen("Pretendard 0123", {0.0f, -1.2f, 0.0f});
    title->font("Pretendard");
    body->font("Pretendard");
    title->size = 34.0f;
    body->size = 28.0f;
    title->fill(Color::hex("#f1f5f9"));
    body->fill(Color::hex("#ffd166"));
    scene->add(title);
    scene->add(body);
    time = 0.0f;
    return scene;
}

static Gradient ramp(GradientType type)
{
    Gradient gradient;
    gradient.type = type;
    gradient.stopCount = 3;
    gradient.stops[0] = {0.0f, Color::hex("#f72585")};
    gradient.stops[1] = {0.5f, Color::hex("#4cc9f0")};
    gradient.stops[2] = {1.0f, Color::hex("#ffd166")};
    return gradient;
}

static Scene* gradients(float& time, GradientType type)
{
    auto scene = Scene::gen(config());
    auto rect = Rectangle::gen({-2.6f, 0.0f, 0.0f}, {4.0f, 4.0f});
    auto circle = Circle::gen({2.6f, 0.0f, 0.0f}, 2.0f);
    for (Object* object : {static_cast<Object*>(rect), static_cast<Object*>(circle)}) {
        object->fill(Color::hex("#ffffff")).stroke(Color::hex("#ffffff"), 0.0f);
        object->style.stroke.a = 0;
        object->gradient(ramp(type));
        scene->add(object);
    }
    time = 0.0f;
    return scene;
}

static Scene* linear(float& time) { return gradients(time, GradientType::Linear); }
static Scene* radial(float& time) { return gradients(time, GradientType::Radial); }
static Scene* conic(float& time) { return gradients(time, GradientType::Conic); }

static std::vector<char> readFile(const char* path)
{
    std::vector<char> bytes;
    auto file = std::fopen(path, "rb");
    if (!file) return bytes;
    char chunk[4096];
    size_t count;
    while ((count = std::fread(chunk, 1, sizeof(chunk), file)) > 0) bytes.insert(bytes.end(), chunk, chunk + count);
    std::fclose(file);
    return bytes;
}

static Asset* svgAsset = nullptr;
static Asset* pixelAsset = nullptr;

static Scene* pictures(float& time, ImageFilter filter)
{
    auto scene = Scene::gen(config());
    if (svgAsset) {
        auto svg = Picture::gen(svgAsset, {-2.6f, 0.0f, 0.0f}, 4.0f);
        scene->add(svg);
    }
    if (pixelAsset) {
        auto raw = Picture::gen(pixelAsset, {2.6f, 0.0f, 0.0f}, 4.0f);
        raw->filter = filter;
        scene->add(raw);
    }
    time = 0.0f;
    return scene;
}

static Scene* picture(float& time) { return pictures(time, ImageFilter::Bilinear); }
static Scene* pictureNearest(float& time) { return pictures(time, ImageFilter::Nearest); }

static Scene* mesh(float& time)
{
    auto cfg = config();
    cfg.cameraView = CameraView::ThreeD;
    cfg.camera.eye = {4.0f, 3.4f, 5.0f};
    cfg.camera.target = {};
    cfg.camera.projection = Projection::Perspective;
    cfg.camera.near = 0.1f;
    cfg.camera.far = 100.0f;
    auto scene = Scene::gen(cfg);
    static constexpr uint32_t N = 9;
    Vec3 points[N * N];
    for (uint32_t r = 0; r < N; r++) {
        for (uint32_t c = 0; c < N; c++) {
            auto x = -2.0f + 4.0f * c / (N - 1);
            auto z = -2.0f + 4.0f * r / (N - 1);
            points[r * N + c] = {x, 0.6f * std::sin(x) * std::cos(z), z};
        }
    }
    auto surface = SurfaceMesh::gen(points, N, N);
    surface->mode = SurfaceMode::SolidMesh;
    surface->fill(Color::hex("#4cc9f0")).stroke(Color::hex("#f1f5f9"), 1.0f);
    scene->add(surface);
    time = 0.0f;
    return scene;
}

static Scene* viewport(float& time)
{
    auto scene = Scene::gen(config());
    auto back = Circle::gen({-3.0f, 0.0f, 0.0f}, 1.5f);
    back->fill(Color::hex("#f72585"));
    scene->add(back);
    auto childConfig = config();
    childConfig.background = Color::hex("#1d3557");
    auto child = Scene::gen(childConfig);
    auto box = Rectangle::gen({}, {5.0f, 3.0f}, 0.4f);
    box->fill(Color::hex("#40d8a0")).stroke(Color::hex("#ffd166"), 4.0f);
    child->add(box);
    scene->viewport(child, {0.45f, 0.15f, 0.5f, 0.7f});
    time = 0.0f;
    return scene;
}

static Scene* fade(float& time)
{
    auto scene = Scene::gen(config());
    auto circle = Circle::gen({-2.0f, 0.0f, 0.0f}, 1.8f);
    auto rect = Rectangle::gen({2.0f, 0.0f, 0.0f}, {3.0f, 3.0f});
    circle->fill(Color::hex("#ffd166")).stroke(Color::hex("#f72585"), 4.0f);
    rect->fill(Color::hex("#4cc9f0"));
    scene->add(rect);
    scene->add(circle);
    scene->play(Animation::fadeIn(circle, {1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear);
    scene->play(Animation::fade(rect, 0.2f), 1.0f, Easing::Linear);
    time = 1.5f;  // circle fully in, rect halfway to 0.2 opacity
    return scene;
}

struct Metrics
{
    float mean = 0.0f;
    float outliers = 0.0f;
};

static Metrics compare(const uint8_t* a, const uint8_t* b, uint8_t* diff)
{
    uint64_t sum = 0;
    uint32_t outliers = 0;
    for (uint32_t i = 0; i < Width * Height; i++) {
        auto peak = 0;
        for (auto c = 0; c < 4; c++) {
            auto d = std::abs(int(a[i * 4 + c]) - int(b[i * 4 + c]));
            sum += d;
            if (d > peak) peak = d;
        }
        if (peak > 32) outliers++;
        auto v = static_cast<uint8_t>(std::min(255, peak * 4));
        diff[i * 4 + 0] = v;
        diff[i * 4 + 1] = peak > 32 ? 0 : v;
        diff[i * 4 + 2] = peak > 32 ? 0 : v;
        diff[i * 4 + 3] = 255;
    }
    return {float(sum) / (Width * Height * 4.0f), 100.0f * outliers / (Width * Height)};
}

static void save(const char* dir, const char* name, const char* kind, const uint8_t* rgba)
{
    Surface surface;
    if (surface.resize(Width, Height) != Result::Success) return;
    for (uint32_t y = 0; y < Height; y++) {
        std::memcpy(surface.data() + y * surface.stride(), rgba + y * Width * 4, Width * 4);
    }
    auto path = std::string(dir) + "/" + name + "-" + kind + ".png";
    if (Saver::png(surface, path.c_str()) != Result::Success) std::fprintf(stderr, "failed to save %s\n", path.c_str());
}

int main()
{
    GlTestContext context;
    context.width = Width;
    context.height = Height;
    auto ready = context.init();
    CHECK(ready);
    if (!ready) return 1;

    auto svg = readFile(VIS_TEST_SVG);
    CHECK(!svg.empty());
    if (!svg.empty()) {
        CHECK(AssetLoader::load(svg.data(), uint32_t(svg.size()), "svg", svgAsset) == Result::Success);
    }
    uint32_t checker[16 * 16];
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            auto on = ((x / 4) + (y / 4)) % 2;
            // ABGR8888 (R in the low byte), straight alpha.
            checker[y * 16 + x] = on ? 0xff50c8ffu : 0xff8040f0u;
        }
    }
    CHECK(AssetLoader::load(checker, 16, 16, pixelAsset) == Result::Success);

    CGLSetCurrentContext(context.context);
    auto cpu = Renderer::gen();
    auto glRenderer = gl::Renderer::gen();
    CHECK(cpu && glRenderer);
    if (!cpu || !glRenderer) return 1;
    CHECK(cpu->font(VIS_TEST_FONT) == Result::Success);
    CHECK(cpu->font(VIS_TEST_MONO_FONT) == Result::Success);
    CHECK(glRenderer->font(VIS_TEST_FONT) == Result::Success);
    CHECK(glRenderer->font(VIS_TEST_MONO_FONT) == Result::Success);

    // Limits come from measured macOS GL output (mean / outlier%) with ~2x margin.
    // Remaining differences are edge antialiasing (GL MSAA vs CPU analytic coverage).
    const Case cases[] = {
        {"shapes", shapes, 2.0f, 2.5f},             // measured 1.06 / 1.16%
        {"text", text, 1.0f, 1.0f},                 // measured 0.50 / 0.48%
        {"linear-gradient", linear, 0.5f, 0.2f},    // measured 0.24 / 0.06%
        {"radial-gradient", radial, 0.5f, 0.2f},    // measured 0.24 / 0.07%
        {"conic-gradient", conic, 0.5f, 0.2f},      // measured 0.24 / 0.08%
        {"picture", picture, 0.5f, 0.2f},           // measured 0.24 / 0.00% (SVG + raw RGBA, bilinear)
        {"surface-mesh", mesh, 1.1f, 2.0f},         // measured 0.55 / 1.02%
        {"viewport", viewport, 0.4f, 0.3f},         // measured 0.16 / 0.13%
        {"fade", fade, 0.5f, 0.4f},                 // measured 0.26 / 0.17%
        // Known ThorVG difference: the SW engine's nearest-neighbour upscaler samples
        // with a half-texel offset (cell edges shift ~4px at 7.5x), while GL lands
        // texel edges exactly. viscript passes FilterMethod::Nearest through to both
        // engines unchanged, so this only guards against gross breakage.
        {"picture-nearest", pictureNearest, 4.0f, 8.0f},  // measured 2.24 / 4.50%
    };

    auto dir = std::getenv("VIS_GL_PARITY_DIR");
    std::vector<uint8_t> glPixels(Width * Height * 4), flipped(Width * Height * 4), cpuPixels(Width * Height * 4),
        diff(Width * Height * 4);

    std::printf("%-18s %10s %12s\n", "scene", "mean", "outliers%");
    for (auto& entry : cases) {
        float time = 0.0f;
        auto scene = entry.build(time);
        CHECK(scene != nullptr);
        if (!scene) continue;

        Surface surface;
        auto cpuResult = cpu->render(scene, time, surface);
        CHECK(cpuResult == Result::Success);
        for (uint32_t y = 0; y < Height; y++) {
            std::memcpy(cpuPixels.data() + y * Width * 4, surface.data() + y * surface.stride(), Width * 4);
        }

        gl::Target target;
        target.context = context.context;
        target.id = static_cast<int32_t>(context.fbo);
        target.width = Width;
        target.height = Height;
        auto glResult = glRenderer->render(scene, time, target);
        CHECK(glResult == Result::Success);
        context.bindFramebuffer(GL_FRAMEBUFFER, context.fbo);
        context.readPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, glPixels.data());
        // glReadPixels returns rows bottom-up.
        for (uint32_t y = 0; y < Height; y++) {
            std::memcpy(flipped.data() + y * Width * 4, glPixels.data() + (Height - 1 - y) * Width * 4, Width * 4);
        }

        auto metrics = compare(cpuPixels.data(), flipped.data(), diff.data());
        std::printf("%-18s %10.3f %11.3f%%\n", entry.name, metrics.mean, metrics.outliers);
        CHECK(cpuResult != Result::Success || glResult != Result::Success
              || (metrics.mean <= entry.meanLimit && metrics.outliers <= entry.outlierLimit));
        if (dir) {
            save(dir, entry.name, "cpu", cpuPixels.data());
            save(dir, entry.name, "gl", flipped.data());
            save(dir, entry.name, "diff", diff.data());
        }
        delete scene;
    }

    delete glRenderer;
    delete cpu;
    delete svgAsset;
    delete pixelAsset;
    return report();
}
