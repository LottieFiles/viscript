#include "testSceneChecks.h"

struct SampleContext
{
    uint32_t calls = 0;
    float firstTime = -1.0f;
    float lastTime = -1.0f;
};

static bool _cellSample(float x, float y, float time, Color& color, void* data) noexcept
{
    auto context = static_cast<SampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>((x + 1.0f) * 80.0f),
        static_cast<uint8_t>(y * 80.0f),
        40,
        255};
    return true;
}

static bool _voxelSample(float x, float y, float z, float time, Color& color,
                         void* data) noexcept
{
    auto context = static_cast<SampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>((x + 1.0f) * 80.0f),
        static_cast<uint8_t>(y * 80.0f),
        static_cast<uint8_t>((z + 1.0f) * 80.0f),
        255};
    return true;
}

static bool _rejectVoxel(float, float, float, float, Color&, void*) noexcept
{
    return false;
}

static bool _solidCell(float, float, float, Color& color, void* data) noexcept
{
    color = *static_cast<Color*>(data);
    return true;
}

struct ReentrantVoxelContext
{
    Space* space = nullptr;
    Result nested = Result::Success;
    Result added = Result::Success;
};

static bool _reentrantVoxel(float, float, float, float, Color& color,
                            void* data) noexcept
{
    auto context = static_cast<ReentrantVoxelContext*>(data);
    Group* nested = nullptr;
    context->nested = context->space->voxel(_rejectVoxel, nested);
    auto child = Group::gen();
    context->added = context->space->add(child);
    if (context->added != Result::Success) delete child;
    color = Color::hex("#2563eb");
    return true;
}

void voxelSamples(Renderer* renderer)
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f},
                            {-1.0f, 1.0f, 1.0f});
    SampleContext context;
    Group* voxels = nullptr;
    CHECK(space != nullptr);
    if (!space) return;
    CHECK(space->voxel(_voxelSample, voxels, &context, CellMode::Padd, 0.1f)
          == Result::Success);
    CHECK(voxels != nullptr);
    CHECK(context.calls == 18u);
    CHECK(context.firstTime == 0.0f && context.lastTime == 0.0f);
    CHECK(space->count() == 1u);
    CHECK(voxels && voxels->parent() == space);
    CHECK(voxels && voxels->childCount() == 3u);
    if (voxels && voxels->childCount() == 3u) {
        auto first = static_cast<Cell*>(voxels->childAt(0));
        auto last = static_cast<Cell*>(voxels->childAt(2));
        CHECK(first->type() == Type::Cell);
        CHECK(first->columns() == 3u);
        CHECK(first->rows() == 2u);
        CHECK(same(first->colors()[0], Color{0, 0, 0, 255}));
        CHECK(same(first->colors()[5], Color{160, 80, 0, 255}));
        CHECK(same(last->colors()[0], Color{0, 0, 160, 255}));
        auto center = first->model * Vec3{0.5f, 0.5f, 0.5f};
        CHECK(near(center.x, -1.0f));
        CHECK(near(center.y, 0.0f));
        CHECK(near(center.z, -1.0f));
    }

    Group* rejected = nullptr;
    CHECK(space->voxel(_rejectVoxel, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    CHECK(space->count() == 1u);
    CHECK(space->voxel(nullptr, rejected) == Result::InvalidArguments);
    CHECK(space->voxel(_voxelSample, rejected, &context, CellMode::Padd, 0.5f)
          == Result::InvalidArguments);

    Config config;
    config.width = 320u;
    config.height = 180u;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {5.0f, 4.0f, 6.0f};
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);
    if (scene) {
        CHECK(scene->add(space) == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) > 1000u);
    } else {
        delete space;
    }
    delete scene;

    auto oversized = Space::gen({-100.0f, 100.0f, 1.0f},
                                {-100.0f, 100.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
    Group* output = nullptr;
    CHECK(oversized != nullptr);
    if (oversized) {
        CHECK(oversized->voxel(_voxelSample, output, &context)
              == Result::InsufficientCondition);
    }
    CHECK(output == nullptr);
    delete oversized;

    auto guarded = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                              {0.0f, 0.0f, 1.0f});
    auto reentrant = ReentrantVoxelContext{guarded};
    output = nullptr;
    CHECK(guarded != nullptr);
    if (guarded) {
        CHECK(guarded->voxel(_reentrantVoxel, output, &reentrant)
              == Result::Success);
        CHECK(reentrant.nested == Result::InsufficientCondition);
        CHECK(reentrant.added == Result::InsufficientCondition);
        CHECK(output && guarded->count() == 1u);
    }
    delete guarded;
}

void cellSamples()
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f},
                            {-100.0f, 100.0f, 1.0f});
    SampleContext context;
    Group* cells = nullptr;
    CHECK(space != nullptr);
    if (!space) return;
    CHECK(space->cell(_cellSample, cells, &context, CellMode::Padd, 0.1f)
          == Result::Success);
    CHECK(cells != nullptr);
    CHECK(context.calls == 6u);
    CHECK(context.firstTime == 0.0f && context.lastTime == 0.0f);
    CHECK(cells && cells->childCount() == 1u);
    if (cells && cells->childCount() == 1u) {
        auto cell = static_cast<Cell*>(cells->childAt(0));
        CHECK(cell->columns() == 3u);
        CHECK(cell->rows() == 2u);
        CHECK(same(cell->colors()[0], Color{0, 0, 40, 255}));
        CHECK(same(cell->colors()[5], Color{160, 80, 40, 255}));
        auto center = cell->model * Vec3{0.5f, 0.5f, 0.5f};
        CHECK(near(center.x, -1.0f));
        CHECK(near(center.y, 0.0f));
        CHECK(near(center.z, 0.0f));
    }
    Group* rejected = nullptr;
    CHECK(space->cell(nullptr, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    delete space;
}

struct RiemannProbe
{
    float xs[8] = {};
    uint32_t calls = 0u;
    // The call that returns false, and whether every value is NaN.
    uint32_t stop = UINT32_MAX;
    bool refuse = false;
};

// f(x) = x - 1, recording where it is read.
static bool _riemannLine(float x, float& value, void* data) noexcept
{
    auto probe = static_cast<RiemannProbe*>(data);
    auto call = probe->calls++;
    if (call < 8u) probe->xs[call] = x;
    if (call == probe->stop) return false;
    value = probe->refuse ? std::numeric_limits<float>::quiet_NaN() : x - 1.0f;
    return true;
}

static bool _riemannBar(const Object* object, float x, float y, float width, float height)
{
    if (!object || object->type() != Type::Rectangle) return false;
    auto bar = static_cast<const Rectangle*>(object);
    return near(bar->center.x, x) && near(bar->center.y, y) && near(bar->size.x, width)
           && near(bar->size.y, height);
}

void riemannSums()
{
    // Whole steps from min, the last one ending at max.
    auto whole = Range{0.0f, 2.0f, 0.5f};
    auto shortened = Range{0.0f, 1.2f, 0.5f};
    CHECK(whole.steps() == 4u && shortened.steps() == 3u);
    CHECK(near(shortened.stepAt(2u), 1.0f) && shortened.stepAt(3u) == 1.2f && shortened.stepAt(9u) == 1.2f);
    // Rounding leaves no sliver of a step at the end; the last step reaches max instead.
    auto sliver = Range{0.0f, 1.00004f, 0.5f};
    CHECK(sliver.steps() == 2u && sliver.stepAt(2u) == 1.00004f);
    auto empty = Range{1.0f, 1.0f, 0.5f};
    auto reversed = Range{2.0f, 1.0f, 0.5f};
    auto still = Range{0.0f, 1.0f, 0.0f};
    auto endless = Range{0.0f, std::numeric_limits<float>::infinity(), 1.0f};
    auto countless = Range{0.0f, 1.0e30f, 1.0e-30f};
    CHECK(empty.steps() == 0u && reversed.steps() == 0u && still.steps() == 0u);
    CHECK(endless.steps() == 0u && countless.steps() == 0u);

    // One rectangle for each height from the left, standing on y = 0.
    const float heights[] = {-1.0f, -0.5f, 0.0f, 0.5f};
    Group* sum = nullptr;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, heights, 4u, sum) == Result::Success);
    CHECK(sum && sum->type() == Type::Group && sum->childCount() == 4u);
    if (sum && sum->childCount() == 4u) {
        CHECK(_riemannBar(sum->childAt(0), 0.25f, -0.5f, 0.5f, 1.0f));
        CHECK(_riemannBar(sum->childAt(1), 0.75f, -0.25f, 0.5f, 0.5f));
        CHECK(_riemannBar(sum->childAt(3), 1.75f, 0.25f, 0.5f, 0.5f));
        // A height of zero leaves a flat rectangle in its place.
        auto flat = static_cast<const Rectangle*>(sum->childAt(2));
        CHECK(near(flat->center.x, 1.25f) && flat->size.y > 0.0f && flat->size.y < 1.0e-5f);
    }
    // The group is not attached yet: a scene takes it whole.
    auto scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene && sum) {
        CHECK(scene->add(sum) == Result::Success);
        CHECK(scene->object(sum->id()) == sum && sum->childAt(0)->id() == sum->id() + 1u);
        if (scene->object(sum->id()) != sum) delete sum;
    } else {
        delete sum;
    }
    delete scene;

    auto dummy = Group::gen();
    auto rejected = dummy;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, heights, 3u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, nullptr, 4u, rejected) == Result::InvalidArguments);
    const float infinite[] = {1.0f, std::numeric_limits<float>::infinity()};
    CHECK(Group::riemann({0.0f, 1.0f, 0.5f}, infinite, 2u, rejected) == Result::InvalidArguments);
    CHECK(Group::riemann({1.0f, 0.0f, 0.5f}, heights, 4u, rejected) == Result::InvalidArguments);
    CHECK(Group::riemann({1.0f, 1.0f, 0.5f}, heights, 4u, rejected) == Result::InsufficientCondition);
    CHECK(Group::riemann({0.0f, 4097.0f, 1.0f}, heights, 4u, rejected) == Result::InsufficientCondition);
    // Past 1e7 a tenth of a step no longer moves a float, so the first step has no width.
    float ones[10];
    for (auto& one : ones)
        one = 1.0f;
    CHECK(Group::riemann({1.0e7f, 1.0e7f + 1.0f, 0.1f}, ones, 10u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    delete dummy;

    // fn is read once for each step, from the left, where sample says.
    RiemannProbe probe;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, sum)
          == Result::Success);
    CHECK(probe.calls == 4u && probe.xs[0] == 0.0f && probe.xs[3] == 1.5f);
    CHECK(sum && _riemannBar(sum->childAt(0), 0.25f, -0.5f, 0.5f, 1.0f));
    delete sum;
    probe = {};
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Right, sum)
          == Result::Success);
    CHECK(probe.calls == 4u && probe.xs[0] == 0.5f && probe.xs[3] == 2.0f);
    CHECK(sum && _riemannBar(sum->childAt(3), 1.75f, 0.5f, 0.5f, 1.0f));
    delete sum;
    // The last step ends short at the end of the range and is read at its own midpoint.
    probe = {};
    CHECK(Group::riemann({0.0f, 1.2f, 0.5f}, _riemannLine, &probe, RiemannSample::Midpoint, sum)
          == Result::Success);
    CHECK(probe.calls == 3u && near(probe.xs[0], 0.25f) && near(probe.xs[2], 1.1f));
    CHECK(sum && _riemannBar(sum->childAt(2), 1.1f, 0.05f, 0.2f, 0.1f));
    delete sum;
    // The point lies a share of the step from its left end, in float: across zero that can be a
    // rounding away from the midpoint of the ends.
    probe = {};
    auto crossing = Range{-0.09177006f, 4.873922f, 0.37f};
    CHECK(Group::riemann(crossing, _riemannLine, &probe, RiemannSample::Midpoint, sum) == Result::Success);
    auto first = crossing.stepAt(0u);
    auto second = crossing.stepAt(1u);
    CHECK(probe.xs[0] == first + (second - first) * 0.5f);
    CHECK(probe.xs[0] != static_cast<float>((static_cast<double>(first) + second) * 0.5));
    delete sum;

    // fn stops the call by returning false or writing a value that is not finite.
    rejected = nullptr;
    probe = {};
    probe.stop = 2u;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(probe.calls == 3u && rejected == nullptr);
    probe = {};
    probe.refuse = true;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(probe.calls == 1u && rejected == nullptr);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, nullptr, nullptr, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, static_cast<RiemannSample>(3), rejected)
          == Result::InvalidArguments);
    CHECK(Group::riemann({0.0f, 5000.0f, 1.0f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InsufficientCondition);
    CHECK(rejected == nullptr);
}

struct FieldProbe
{
    uint32_t calls = 0u;
    // The call that returns false, and the x past which the field is NaN.
    uint32_t stop = UINT32_MAX;
    double edge = std::numeric_limits<double>::infinity();
};

// The field that turns about the origin, (-y, x).
static bool _fieldTurn(double x, double y, double& u, double& v, void* data) noexcept
{
    auto probe = static_cast<FieldProbe*>(data);
    if (probe->calls++ == probe->stop) return false;
    u = -y;
    v = x;
    return true;
}

// The field that runs right at unit speed, NaN past the probe's edge.
static bool _fieldFlow(double x, double, double& u, double& v, void* data) noexcept
{
    auto probe = static_cast<FieldProbe*>(data);
    if (probe->calls++ == probe->stop) return false;
    u = x > probe->edge ? std::numeric_limits<double>::quiet_NaN() : 1.0;
    v = 0.0;
    return true;
}

// The field that carries a point away from x = 0, faster the farther it is.
static bool _fieldSpread(double x, double, double& u, double& v, void*) noexcept
{
    u = x;
    v = 0.0;
    return true;
}

static bool _vectorAt(const Object* object, const Vec3& origin, const Vec3& value)
{
    if (!object || object->type() != Type::Vector) return false;
    auto arrow = static_cast<const Vector*>(object);
    return near(arrow->origin.x, origin.x) && near(arrow->origin.y, origin.y) && near(arrow->value.x, value.x)
           && near(arrow->value.y, value.y);
}

void vectorFields()
{
    // The samples from min, one every step up to max.
    auto grid = Range{-1.0f, 1.0f, 0.5f};
    auto uneven = Range{0.0f, 1.2f, 0.5f};
    auto rounded = Range{0.0f, 0.99996f, 0.5f};
    auto single = Range{2.0f, 2.0f, 1.0f};
    auto reversed = Range{1.0f, 0.0f, 1.0f};
    CHECK(grid.samples() == 5u && grid.sampleAt(4u) == 1.0f && grid.sampleAt(1u) == -0.5f);
    CHECK(uneven.samples() == 3u && uneven.sampleAt(2u) == 1.0f);
    // Rounding never drops a sample that max should have been.
    CHECK(rounded.samples() == 3u && rounded.sampleAt(2u) == 1.0f);
    CHECK(single.samples() == 1u && reversed.samples() == 0u);

    // An arrow at every sample but the origin, where the field vanishes, row by row from the
    // bottom left, approaching length as the field grows.
    FieldProbe probe;
    Group* field = nullptr;
    uint32_t indices[9] = {};
    auto axis = Range{-1.0f, 1.0f, 1.0f};
    VectorFieldOptions options;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options, indices) == Result::Success);
    CHECK(probe.calls == 9u && field && field->childCount() == 8u);
    CHECK(indices[0] == 0u && indices[3] == 3u && indices[4] == 5u && indices[7] == 8u);
    if (field && field->childCount() == 8u) {
        // The default length is 0.8 of the smaller step: tanh(1 / 0.8) * 0.8 along (0, 1) at (1, 0).
        auto edge = 0.8f * std::tanh(1.0f / 0.8f);
        CHECK(_vectorAt(field->childAt(4), {1.0f, 0.0f, 0.0f}, {0.0f, edge, 0.0f}));
        auto arrow = static_cast<const Vector*>(field->childAt(0));
        CHECK(arrow->tip == 10.0f && near(std::hypot(arrow->value.x, arrow->value.y), 0.8f * std::tanh(std::sqrt(2.0f) / 0.8f)));
    }
    delete field;

    // Scaled and centered arrows, colored by magnitude from the least to the greatest.
    const Color ramp[] = {{0, 0, 0, 255}, {255, 255, 255, 255}};
    options.scale = 0.5f;
    options.centered = true;
    options.tip = 4.0f;
    options.colors = ramp;
    options.colorCount = 2u;
    probe = {};
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options) == Result::Success);
    CHECK(field && field->childCount() == 8u);
    if (field && field->childCount() == 8u) {
        CHECK(_vectorAt(field->childAt(4), {1.0f, -0.25f, 0.0f}, {0.0f, 0.5f, 0.0f}));
        auto corner = static_cast<const Vector*>(field->childAt(0));
        auto side = static_cast<const Vector*>(field->childAt(1));
        CHECK(corner->tip == 4.0f && same(corner->style.stroke, {255, 255, 255, 255}));
        // A side has magnitude 1 of the corners' sqrt(2).
        auto gray = static_cast<uint8_t>(std::lround(255.0 / std::sqrt(2.0)));
        CHECK(same(side->style.stroke, {gray, gray, gray, 255}));
    }
    delete field;
    // low and high set the magnitudes the colors spread over.
    options.low = 1.0f;
    options.high = 2.0f;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options) == Result::Success);
    CHECK(field && field->childCount() == 8u && same(field->childAt(1)->style.stroke, {0, 0, 0, 255}));
    delete field;

    // The vectors given instead, one for each sample.
    const Vec2 given[] = {{1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 2.0f}};
    auto row = Range{0.0f, 2.0f, 1.0f};
    auto line = Range{0.0f, 0.0f, 1.0f};
    CHECK(Group::vectorField(row, line, given, 3u, field, {}, indices) == Result::Success);
    CHECK(field && field->childCount() == 2u && indices[0] == 0u && indices[1] == 2u);
    delete field;

    auto dummy = Group::gen();
    auto rejected = dummy;
    CHECK(Group::vectorField(row, line, given, 2u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    const Vec2 infinite[] = {{1.0f, 0.0f}, {0.0f, std::numeric_limits<float>::infinity()}, {0.0f, 2.0f}};
    CHECK(Group::vectorField(row, line, infinite, 3u, rejected) == Result::InvalidArguments);
    CHECK(Group::vectorField(row, line, static_cast<const Vec2*>(nullptr), 3u, rejected)
          == Result::InvalidArguments);
    CHECK(Group::vectorField(row, line, static_cast<FieldSampler>(nullptr), nullptr, rejected)
          == Result::InvalidArguments);
    VectorFieldOptions both;
    both.length = 1.0f;
    both.scale = 1.0f;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, both) == Result::InvalidArguments);
    VectorFieldOptions lone;
    lone.colors = ramp;
    lone.colorCount = 1u;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, lone) == Result::InvalidArguments);
    VectorFieldOptions tipless;
    tipless.tip = -1.0f;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, tipless) == Result::InvalidArguments);
    CHECK(Group::vectorField(reversed, line, given, 3u, rejected) == Result::InvalidArguments);
    auto wide = Range{0.0f, 5000.0f, 1.0f};
    auto square = Range{0.0f, 200.0f, 1.0f};
    CHECK(Group::vectorField(wide, line, _fieldTurn, &probe, rejected) == Result::InsufficientCondition);
    CHECK(Group::vectorField(square, square, _fieldTurn, &probe, rejected) == Result::InsufficientCondition);
    // fn stops the call by returning false or writing a vector that is not finite.
    probe = {};
    probe.stop = 3u;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, rejected) == Result::InvalidArguments);
    CHECK(probe.calls == 4u && rejected == nullptr);
    probe = {};
    probe.edge = 0.5;
    CHECK(Group::vectorField(axis, axis, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(probe.calls == 3u && rejected == nullptr);
    delete dummy;
}

static bool _plotFrom(const Object* object, uint32_t count, float first, float last)
{
    if (!object || object->type() != Type::Plot) return false;
    auto plot = static_cast<const Plot*>(object);
    return plot->count() == count && near(plot->points()[0].x, first) && near(plot->points()[count - 1u].x, last);
}

void streamLines()
{
    StreamLineOptions options;
    CHECK(options.steps() == 60u);
    options.time = 1.0f;
    options.dt = 0.3f;
    CHECK(options.steps() == 4u);
    options.time = 1.0e-9f;
    options.dt = 1.0f;
    CHECK(options.steps() == 1u);
    options.time = 0.0f;
    CHECK(options.steps() == 0u);

    // A line from each sample of the grid, which also bounds them: the last seed has nowhere
    // to go and draws no line.
    FieldProbe probe;
    Group* lines = nullptr;
    uint32_t indices[4] = {};
    options.time = 1.0f;
    options.dt = 0.5f;
    auto row = Range{0.0f, 2.0f, 1.0f};
    auto line = Range{0.0f, 0.0f, 1.0f};
    CHECK(Group::streamLines(row, line, _fieldFlow, &probe, lines, options, indices) == Result::Success);
    CHECK(lines && lines->childCount() == 2u && indices[0] == 0u && indices[1] == 1u);
    if (lines && lines->childCount() == 2u) {
        CHECK(_plotFrom(lines->childAt(0), 3u, 0.0f, 1.0f) && _plotFrom(lines->childAt(1), 3u, 1.0f, 2.0f));
    }
    delete lines;

    // Both ways from a seed, the backward part toward it and on past it, inside the region.
    const Vec2 seed[] = {{0.0f, 0.0f}};
    Bounds region;
    region.min = {-0.75f, -1.0f, 0.0f};
    region.max = {10.0f, 1.0f, 0.0f};
    options.direction = StreamDirection::Both;
    probe = {};
    CHECK(Group::streamLines(seed, 1u, &region, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 4u, -0.5f, 1.0f));
    delete lines;
    // Without a region nothing bounds it; backward runs against the field only.
    options.direction = StreamDirection::Backward;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 3u, -1.0f, 0.0f));
    delete lines;

    // A line ends where the field stops being finite, without failing the call.
    options.direction = StreamDirection::Forward;
    options.time = 2.0f;
    options.dt = 0.25f;
    probe = {};
    probe.edge = 0.6;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 3u, 0.0f, 0.5f));
    delete lines;

    // A line under a thousandth of the longest is left out, and the colors spread over the mean
    // speeds of the lines kept.
    const Vec2 spread[] = {{1.0f, 0.0f}, {1.0e-6f, 0.0f}, {0.0f, 0.0f}};
    const Color ramp[] = {{0, 0, 0, 255}, {255, 255, 255, 255}};
    options.time = 1.0f;
    options.dt = 0.1f;
    options.colors = ramp;
    options.colorCount = 2u;
    CHECK(Group::streamLines(spread, 3u, nullptr, _fieldSpread, nullptr, lines, options, indices)
          == Result::Success);
    CHECK(lines && lines->childCount() == 1u && indices[0] == 0u);
    CHECK(lines && same(lines->childAt(0)->style.stroke, {255, 255, 255, 255}));
    delete lines;
    options.colors = nullptr;
    options.colorCount = 0u;

    auto dummy = Group::gen();
    auto rejected = dummy;
    probe = {};
    probe.stop = 5u;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, options)
          == Result::InvalidArguments);
    CHECK(probe.calls == 6u && rejected == nullptr);
    CHECK(Group::streamLines(nullptr, 1u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(Group::streamLines(seed, 0u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(Group::streamLines(seed, 1u, nullptr, nullptr, nullptr, rejected) == Result::InvalidArguments);
    const Vec2 lost[] = {{std::numeric_limits<float>::quiet_NaN(), 0.0f}};
    CHECK(Group::streamLines(lost, 1u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    Bounds inverted;
    inverted.min = {1.0f, 0.0f, 0.0f};
    inverted.max = {0.0f, 1.0f, 0.0f};
    CHECK(Group::streamLines(seed, 1u, &inverted, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    StreamLineOptions still;
    still.dt = 0.0f;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, still) == Result::InvalidArguments);
    StreamLineOptions astray;
    astray.direction = static_cast<StreamDirection>(3);
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, astray)
          == Result::InvalidArguments);
    StreamLineOptions endless;
    endless.time = 1000.0f;
    endless.dt = 0.01f;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, endless)
          == Result::InsufficientCondition);
    auto wide = Range{0.0f, 5000.0f, 1.0f};
    CHECK(Group::streamLines(wide, line, _fieldFlow, &probe, rejected) == Result::InsufficientCondition);
    CHECK(rejected == nullptr);
    delete dummy;
}

struct TemporalSampleContext
{
    uint32_t calls = 0u;
    float firstTime = -1.0f;
    float lastTime = -1.0f;
};

static bool _temporalCell(float, float, float time, Color& color,
                          void* data) noexcept
{
    auto context = static_cast<TemporalSampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>(255.0f * (1.0f - time) + 0.5f),
        0u,
        static_cast<uint8_t>(255.0f * time + 0.5f),
        255u};
    return true;
}

static bool _temporalVoxel(float, float, float, float time, Color& color,
                           void* data) noexcept
{
    auto context = static_cast<TemporalSampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = time < 0.5f ? Color::hex("#2563eb") : Color::hex("#7c3aed");
    return true;
}

void sampleTime(Renderer* renderer)
{
    Config config;
    config.width = 80u;
    config.height = 80u;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto space = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                            {0.0f, 0.0f, 1.0f});
    CHECK(scene && space);
    if (!scene || !space) {
        delete scene;
        delete space;
        return;
    }
    space->style.stroke.a = 0u;
    space->axisX.a = 0u;
    space->axisY.a = 0u;
    space->axisZ.a = 0u;
    TemporalSampleContext context;
    Group* field = nullptr;
    CHECK(space->cell(_temporalCell, field, &context, CellMode::Full,
                      0.0f, {1.0f, 2u}) == Result::Success);
    CHECK(field != nullptr);
    CHECK(context.calls == 3u);
    CHECK(near(context.firstTime, 0.0f) && near(context.lastTime, 1.0f));
    CHECK(scene->add(space) == Result::Success);
    CHECK(near(scene->duration(), 1.0f));

    Surface surface;
    auto center = static_cast<size_t>(config.height / 2u) * config.width
                + config.width / 2u;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#ff0000")));
    CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#c00040")));
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#800080")));
    BBox bounds;
    CHECK(renderer->bounds(scene, field, 0.5f, bounds) == Result::Success);
    CHECK(bounds.width > 0.0f && bounds.height > 0.0f);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#0000ff")));
    CHECK(context.calls == 3u);

    auto blue = Color::hex("#2563eb");
    Group* target = nullptr;
    CHECK(space->cell(_solidCell, target, &blue, CellMode::Full)
          == Result::Success);
    CHECK(scene->play(Animation::morph(field, target), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::NonSupport);
    delete scene;

    auto volume = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                             {0.0f, 0.0f, 1.0f});
    context = {};
    field = nullptr;
    CHECK(volume != nullptr);
    if (volume) {
        CHECK(volume->voxel(_temporalVoxel, field, &context, CellMode::Padd,
                            0.05f, {0.5f, 2u}) == Result::Success);
        CHECK(field != nullptr && context.calls == 2u);
        CHECK(near(context.firstTime, 0.0f) && near(context.lastTime, 0.5f));
        Group* invalid = nullptr;
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {-1.0f, 30u}) == Result::InvalidArguments);
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {1.0f, 0u}) == Result::InvalidArguments);
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {200.0f, 30u}) == Result::InsufficientCondition);
        CHECK(invalid == nullptr);
    }
    delete volume;
}
