#include "testSceneChecks.h"

static Result _strokeCopy(Object* copy, void* data) noexcept
{
    copy->stroke(*static_cast<const Color*>(data));
    return Result::Success;
}

static Result _shiftCopy(Object* copy, void* data) noexcept
{
    copy->shift(*static_cast<const Vec3*>(data));
    return Result::Success;
}

static Result _refuseCopy(Object*, void*) noexcept
{
    return Result::InvalidArguments;
}

static Result _growCopy(Object* copy, void*) noexcept
{
    auto extra = Circle::gen({}, 0.25f);
    if (!extra) return Result::OutOfMemory;
    if (copy->add(extra) == Result::Success) return Result::Success;
    delete extra;
    return Result::InvalidArguments;
}

static bool _sameModel(const Mat4& first, const Mat4& second)
{
    for (auto i = 0u; i < 16u; i++) {
        if (!near(first.e[i], second.e[i])) return false;
    }
    return true;
}

// Whether copy is a new object of the kind of source, in its place, with its colors.
static bool _copies(const Object* copy, const Object* source)
{
    return copy && copy != source && copy->type() == source->type() && _sameModel(copy->model, source->model)
           && same(copy->style.stroke, source->style.stroke) && same(copy->style.fill, source->style.fill)
           && near(copy->style.width, source->style.width) && near(copy->opacity, source->opacity)
           && near(copy->progress, source->progress) && copy->layer == source->layer;
}

static bool _tagged(const Object* object, const char* tag)
{
    return object && (tag ? object->tag() && std::strcmp(object->tag(), tag) == 0 : !object->tag());
}

void clonedObjects(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto scene = Scene::gen(config);
    auto dot = Circle::gen({-3.0f, 3.0f, 0.0f}, 0.5f);
    auto row = Group::gen();
    auto first = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto second = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, 0.2f);
    auto rule = Line::gen({-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f});
    auto word = Text::gen("cat", {3.0f, 3.0f, 0.0f});
    auto counter = Text::gen("", {3.0f, -3.0f, 0.0f});
    Vec3 corners[] = {{-4.0f, -4.0f, 0.0f}, {-2.0f, -4.0f, 0.0f}, {-3.0f, -2.5f, 0.0f}};
    auto triangle = Polygon::gen(corners, 3u);
    PathCommand commands[] = {PathCommand::move({0.0f, -4.0f, 0.0f}), PathCommand::line({1.0f, -3.0f, 0.0f}),
                              PathCommand::quadratic({1.5f, -4.5f, 0.0f}, {2.0f, -4.0f, 0.0f})};
    auto path = Path::gen(commands, 3u);
    auto curve = Curve::gen({-1.0f, 2.0f, 0.0f}, {-0.5f, 3.0f, 0.0f}, {0.5f, 3.0f, 0.0f}, {1.0f, 2.0f, 0.0f});
    auto arrow = Arrow::gen({-4.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f});
    Object* objects[] = {dot, row, first, second, rule, word, counter, triangle, path, curve, arrow};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    dot->tag("dot");
    row->tag("row");
    first->tag("row/first");
    second->tag("second");
    word->tag("word");
    word->slant = 0.25f;
    word->box = {1.5f, 0.5f};
    word->boxOffset = {0.1f, -0.2f};
    second->stroke(blue);
    arrow->tail = 4.0f;
    CHECK(counter->count(0.0f) == Result::Success);
    CHECK(row->add(first) == Result::Success && row->add(second) == Result::Success && row->add(rule) == Result::Success);
    Object* roots[] = {dot, row, word, counter, triangle, path, curve, arrow};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);

    // Every kind copies its geometry, style and place into a new object, without its id.
    Object* kinds[] = {triangle, path, curve, arrow};
    for (auto source : kinds) {
        Object* copy = nullptr;
        CHECK(scene->clone(source, copy) == Result::Success && _copies(copy, source) && _tagged(copy, nullptr));
        CHECK(copy && !copy->parent() && copy->id() != source->id());
    }
    auto count = scene->count();
    Object* copy = nullptr;
    CHECK(scene->clone(dot, copy) == Result::Success && _copies(copy, dot) && _tagged(copy, "dot:copy"));
    auto circle = static_cast<Circle*>(copy);
    CHECK(circle && near(circle->radius, 0.5f) && near(circle->center.x, -3.0f) && scene->count() == count + 1u);
    CHECK(scene->clone(dot, copy) == Result::Success && _tagged(copy, "dot:copy2"));
    CHECK(scene->clone(word, copy) == Result::Success && _copies(copy, word) && _tagged(copy, "word:copy"));
    auto text = static_cast<Text*>(copy);
    CHECK(text && std::strcmp(text->text(), "cat") == 0 && near(text->size, word->size) && text->font()
          && std::strcmp(text->font(), word->font()) == 0 && text->point.x == word->point.x);
    // A leaning text stays leaning, so a copy covers its source exactly.
    CHECK(text && text->slant == word->slant && text->box.x == word->box.x && text->box.y == word->box.y);
    CHECK(text && text->boxOffset.x == word->boxOffset.x && text->boxOffset.y == word->boxOffset.y);
    auto path2 = static_cast<Path*>(nullptr);
    CHECK(scene->clone(path, copy) == Result::Success);
    path2 = static_cast<Path*>(copy);
    CHECK(path2 && path2->count() == path->count() && !path2->closed()
          && std::memcmp(path2->points(), path->points(), sizeof(Vec3) * path->count()) == 0);
    CHECK(scene->clone(arrow, copy) == Result::Success && near(static_cast<Arrow*>(copy)->tail, 4.0f));

    // A group copies what is below it, and the ids below follow the copy's.
    CHECK(scene->clone(row, copy) == Result::Success && _tagged(copy, "row:copy") && copy->childCount() == 3u);
    auto rowCopy = copy;
    CHECK(_copies(rowCopy->childAt(0), first) && _tagged(rowCopy->childAt(0), "row:copy/first"));
    CHECK(_copies(rowCopy->childAt(1), second) && _tagged(rowCopy->childAt(1), "row:copy/second"));
    CHECK(_copies(rowCopy->childAt(2), rule) && _tagged(rowCopy->childAt(2), nullptr));
    CHECK(scene->clone(row, copy) == Result::Success && _tagged(copy, "row:copy2")
          && _tagged(copy->childAt(0), "row:copy2/first") && _tagged(copy->childAt(1), "row:copy2/second"));
    // A given id holds for the copy, and the ids below follow it.
    CloneOptions named;
    named.tag = "twin";
    CHECK(scene->clone(row, copy, named) == Result::Success && _tagged(copy, "twin")
          && _tagged(copy->childAt(0), "twin/first") && _tagged(copy->childAt(1), "twin/second"));
    count = scene->count();
    CHECK(scene->clone(row, copy, named) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "already uses this id"));
    // A copy without an id gives those below the number of their copy.
    auto bare = Group::gen();
    auto leaf = Circle::gen({0.0f, 4.0f, 0.0f}, 0.25f);
    CHECK(bare && leaf);
    if (bare && leaf) {
        leaf->tag("leaf");
        CHECK(bare->add(leaf) == Result::Success);
        CHECK(scene->add(bare) == Result::Success);
        CHECK(scene->clone(bare, copy) == Result::Success && _tagged(copy, nullptr)
              && _tagged(copy->childAt(0), "leaf:copy"));
        CHECK(scene->clone(bare, copy) == Result::Success && _tagged(copy->childAt(0), "leaf:copy2"));
    } else {
        delete bare;
        delete leaf;
    }

    // The copy goes beside the source, or below the given parent.
    CHECK(scene->clone(second, copy) == Result::Success && copy->parent() == row && row->childCount() == 4u
          && row->childAt(3) == copy && _tagged(copy, "second:copy"));
    CloneOptions placed;
    placed.parent = rowCopy;
    CHECK(scene->clone(dot, copy, placed) == Result::Success && copy->parent() == rowCopy
          && rowCopy->childCount() == 4u);
    auto stray = Circle::gen({}, 1.0f);
    placed.parent = stray;
    CHECK(scene->clone(dot, copy, placed) == Result::InvalidArguments && _says(scene, "parent is not part"));
    CHECK(scene->clone(stray, copy) == Result::InvalidArguments && _says(scene, "not part of this scene"));
    CHECK(!scene->shows(stray) && !scene->shows(nullptr));
    delete stray;

    // The edit changes the copy's own settings before it is attached, and a group passes
    // its colors to the copies below that take them from it.
    CloneOptions painted;
    painted.edit = _strokeCopy;
    painted.data = &red;
    CHECK(scene->clone(row, copy, painted) == Result::Success && same(copy->style.stroke, red));
    CHECK(same(copy->childAt(0)->style.stroke, red) && same(copy->childAt(2)->style.stroke, red));
    CHECK(same(copy->childAt(1)->style.stroke, blue) && !same(first->style.stroke, red));
    count = scene->count();
    painted.edit = _refuseCopy;
    CHECK(scene->clone(row, copy, painted) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "settings could not be changed"));
    painted.edit = _growCopy;
    CHECK(scene->clone(row, copy, painted) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "not the objects below it"));

    // A copy takes no color from the theme: the next shape takes the color the copy left.
    auto fresh = Scene::gen(config);
    auto a = Circle::gen({}, 1.0f);
    auto b = Circle::gen({}, 1.0f);
    CHECK(fresh && a && b);
    if (fresh && a && b) {
        CHECK(fresh->add(a) == Result::Success && fresh->clone(a, copy) == Result::Success);
        CHECK(fresh->add(b) == Result::Success);
        CHECK(same(copy->style.stroke, a->style.stroke) && same(b->style.stroke, fresh->theme().objects[1]));
    } else {
        delete a;
        delete b;
    }
    delete fresh;

    // Mid-timeline the copy takes the state at the cursor and shows from there on, with no
    // animation of its own.
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    CHECK(scene->play(AnimationTarget::from(dot).shift({2.0f, 0.0f, 0.0f}).opacity(0.5f), 2.0f, linear)
          == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(word).text("horse"), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(counter).value(100.0f), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    auto clips = scene->clipCount();
    CHECK(scene->clone(dot, copy) == Result::Success && near(copy->model.e[3], 0.5f) && near(copy->opacity, 0.875f));
    auto moving = copy;
    CHECK(scene->clone(word, copy) == Result::Success && std::strcmp(static_cast<Text*>(copy)->text(), "hots") == 0);
    CHECK(static_cast<Text*>(copy)->texts(nullptr, 0u) == 1u);
    CHECK(scene->clone(counter, copy) == Result::Success && static_cast<Text*>(copy)->counting());
    CHECK(near(static_cast<Text*>(copy)->value(), 50.0f) && std::strcmp(static_cast<Text*>(copy)->text(), "50") == 0);
    CHECK(scene->clipCount() == clips);
    BBox bounds;
    CHECK(renderer->bounds(scene, moving, 0.25f, bounds) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, moving, 0.5f, -2.5f, 3.0f));
    CHECK(_placed(renderer, scene, moving, 2.0f, -2.5f, 3.0f));
    CHECK(_placed(renderer, scene, dot, 2.0f, -1.0f, 3.0f));

    // A reveal played on the copy hides it until the reveal, as for any object added there.
    CHECK(scene->seek(2.0f) == Result::Success);
    Vec3 down = {0.0f, -6.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &down;
    CHECK(scene->clone(dot, copy, moved) == Result::Success && near(copy->model.e[7], -6.0f));
    auto revealed = copy;
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(scene->play(Animation::fadeIn(revealed), 1.0f, linear) == Result::Success);
    CHECK(renderer->bounds(scene, revealed, 2.25f, bounds) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, revealed, 3.5f, -1.0f, -3.0f));

    // What below the source no longer shows is copied, but never shows.
    CHECK(scene->remove(first) == Result::Success);
    CHECK(scene->clone(row, copy) == Result::Success && copy->childCount() == 4u);
    CHECK(renderer->bounds(scene, copy->childAt(0), 3.5f, bounds) == Result::InsufficientCondition);
    CHECK(renderer->bounds(scene, copy->childAt(1), 3.5f, bounds) == Result::Success);
    CHECK(!scene->shows(copy->childAt(0)) && scene->shows(copy->childAt(1)) && scene->shows(copy));
    CHECK(scene->shows(first));
    CHECK(scene->clone(first, copy) == Result::InsufficientCondition && _says(scene, "does not show at the cursor"));
    delete scene;
}

void clonedLinks(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto net = Group::gen();
    auto left = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto right = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto far = Circle::gen({0.0f, 4.0f, 0.0f}, 0.5f);
    auto inner = Connector::gen(left, right);
    auto outer = Connector::gen(left, far);
    auto chain = Group::gen();
    auto start = Circle::gen({-4.0f, 4.0f, 0.0f}, 0.25f);
    auto end = Circle::gen({-3.0f, 4.0f, 0.0f}, 0.25f);
    auto link = Connector::gen(start, end);
    auto chained = Connector::gen(link, far);
    auto window = Group::gen();
    auto stencil = Rectangle::gen({0.0f, -3.0f, 0.0f}, {2.0f, 1.0f});
    auto masked = Circle::gen({0.0f, -3.0f, 0.0f}, 1.0f);
    auto space = Space::gen();
    Object* objects[] = {net,   left, right,  far,   inner,   outer,  chain,
                         start, end,  link,   chained, window, stencil, masked, space};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    CHECK(net->add(left) == Result::Success && net->add(right) == Result::Success);
    CHECK(net->add(inner) == Result::Success && net->add(outer) == Result::Success);
    CHECK(scene->add(far) == Result::Success && scene->add(net) == Result::Success);
    CHECK(window->add(stencil) == Result::Success && window->add(masked) == Result::Success);
    CHECK(scene->add(window) == Result::Success && masked->mask(stencil) == Result::Success);
    CHECK(scene->add(space) == Result::Success);

    // A connector joins the copies of the endpoints it has below the source, and keeps the
    // others.
    Object* copy = nullptr;
    CHECK(scene->clone(net, copy) == Result::Success && copy->childCount() == 4u);
    auto innerCopy = static_cast<Connector*>(copy->childAt(2));
    auto outerCopy = static_cast<Connector*>(copy->childAt(3));
    CHECK(innerCopy->from() == copy->childAt(0) && innerCopy->to() == copy->childAt(1));
    CHECK(outerCopy->from() == copy->childAt(0) && outerCopy->to() == far);
    CHECK(scene->clone(inner, copy) == Result::Success && static_cast<Connector*>(copy)->from() == left
          && copy->parent() == net);
    // A connector that ends at another connector below the source is not copied yet; one that
    // ends at a connector elsewhere keeps it.
    CHECK(chain->add(start) == Result::Success && chain->add(end) == Result::Success);
    CHECK(chain->add(link) == Result::Success && chain->add(chained) == Result::Success);
    CHECK(scene->add(chain) == Result::Success);
    auto count = scene->count();
    CHECK(scene->clone(chain, copy) == Result::NonSupport && _says(scene, "ends at another connector"));
    CHECK(scene->count() == count);
    CHECK(scene->clone(chained, copy) == Result::Success && static_cast<Connector*>(copy)->from() == link);
    // A mask and what it masks are copied together.
    CHECK(scene->clone(window, copy) == Result::Success);
    CHECK(copy->childAt(1)->mask() == copy->childAt(0) && masked->mask() == stencil);
    CHECK(scene->clone(masked, copy) == Result::InvalidArguments && _says(scene, "cloned together"));
    CHECK(scene->clone(stencil, copy) == Result::InvalidArguments);
    // Kinds that hold more than their own configuration are not copied yet.
    CHECK(scene->clone(space, copy) == Result::NonSupport && _says(scene, "a space cannot be cloned yet"));
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);

    // A source on its way through a morph cannot be copied until the morph ends.
    Vec3 corners[] = {{-4.0f, -4.0f, 0.0f}, {-3.0f, -4.0f, 0.0f}, {-3.5f, -3.0f, 0.0f}};
    Vec3 others[] = {{3.0f, -4.0f, 0.0f}, {4.0f, -4.0f, 0.0f}, {3.5f, -3.0f, 0.0f}};
    auto from = Polygon::gen(corners, 3u);
    auto to = Polygon::gen(others, 3u);
    CHECK(from && to && scene->add(from) == Result::Success && scene->add(to) == Result::Success);
    CHECK(scene->play(Animation::morph(from, to), 1.0f) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->clone(from, copy) == Result::InsufficientCondition && _says(scene, "middle of a morph"));
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->clone(from, copy) == Result::InsufficientCondition && _says(scene, "does not show"));
    CHECK(scene->clone(to, copy) == Result::Success);
    delete scene;
}

// Matching carries the currently painted billboard size, including earlier model scaling.
// Repeated samples on either side of the handoff remain stable in one retained scene.
void matchedBillboard(Renderer* renderer)
{
    for (auto copying : {false, true}) {
        for (auto guided : {false, true}) {
            Config config;
            config.width = 960; config.height = 540;
            config.camera.orthoHeight = 540.0f;
            auto scene = Scene::gen(config);
            auto source = Text::gen("copy", {-200.0f, 0.0f, 0.0f});
            auto target = Text::gen("copy", {200.0f, 0.0f, 0.0f});
            CHECK(scene && source && target);
            if (!scene || !source || !target) { delete scene; delete source; delete target; continue; }
            source->size = target->size = 40.0f;
            CHECK(source->font("Pretendard") == Result::Success && target->font("Pretendard") == Result::Success);
            CHECK(scene->add(source) == Result::Success && scene->wait(0.1f) == Result::Success);
            CHECK(scene->play(Animation::rescale(source, {0.2f, 0.2f, 0.2f}), 0.2f, Easing::Linear) == Result::Success);
            CHECK(scene->wait(0.2f) == Result::Success && scene->add(target) == Result::Success);
            BBox before; BBox destination; BBox after;
            CHECK(renderer->bounds(scene, source, 0.499f, before) == Result::Success);
            CHECK(renderer->bounds(scene, target, 0.5f, destination) == Result::Success);
            MatchOptions options;
            options.copy = copying;
            if (guided) {
                Vec3 points[] = {{-200.0f, 0.0f, 0.0f}, {0.0f, 80.0f, 0.0f}, {200.0f, 0.0f, 0.0f}};
                auto path = DirectedRoute::gen(points, 3u);
                CHECK(path && scene->add(path) == Result::Success);
                options.path = path;
            }
            CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
                  == Result::Success);
            for (auto time : {1.5f, 0.5f, 0.499f, 0.5f}) {
                auto object = time == 0.499f ? source : target;
                CHECK(renderer->bounds(scene, object, time, after) == Result::Success);
                auto& expected = time == 1.5f ? destination : before;
                CHECK(near(after.x, expected.x, 0.01f) && near(after.y, expected.y, 0.01f)
                      && near(after.width, expected.width, 0.01f) && near(after.height, expected.height, 0.01f));
            }
            if (copying) {
                CHECK(renderer->bounds(scene, source, 1.5f, after) == Result::Success);
                CHECK(near(after.width, before.width, 0.01f) && near(after.height, before.height, 0.01f));
            }
            delete scene;
        }
    }
}

// A match guide contributes the same world-space detour to every piece, while each keeps
// its own endpoints. Nonuniform parents do not reparameterize that shared path.
void matchedPath(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto guideGroup = Group::gen();
    auto a = Circle::gen({-2.0f, -1.0f, 0.0f}, 0.2f);
    auto b = Circle::gen({0.0f, -1.0f, 0.0f}, 0.2f);
    auto c = Circle::gen({0.0f, 1.0f, 0.0f}, 0.2f);
    auto d = Circle::gen({1.0f, 1.0f, 0.0f}, 0.2f);
    Vec3 points[] = {{0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}};
    auto guide = DirectedRoute::gen(points, 3u);
    CHECK(scene && from && to && guideGroup && a && b && c && d && guide);
    if (!scene || !from || !to || !guideGroup || !a || !b || !c || !d || !guide) {
        delete scene; delete from; delete to; delete guideGroup;
        delete a; delete b; delete c; delete d; delete guide;
        return;
    }
    to->model = Mat4::scale({2.0f, 1.0f, 1.0f});
    guideGroup->model = Mat4::translate({100000000.0f, 0.0f, 0.0f}) * to->model;
    guide->opacity = 0.0f;
    CHECK(from->add(a) == Result::Success && from->add(b) == Result::Success);
    CHECK(to->add(c) == Result::Success && to->add(d) == Result::Success);
    CHECK(guideGroup->add(guide) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->add(guideGroup) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success && scene->add(to) == Result::Success);
    MatchOptions options;
    options.copy = true;
    options.shapes = true;
    options.path = guide;
    auto count = scene->count();
    auto clips = scene->clipCount();
    options.arc = 1.0f;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::InvalidArguments);
    CHECK(scene->count() == count && scene->clipCount() == clips && near(scene->cursor(), 1.0f));
    options.arc = 0.0f;
    options.path = from;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::InvalidArguments);
    CHECK(scene->count() == count && scene->clipCount() == clips && near(scene->cursor(), 1.0f));
    options.path = guide;
    CHECK(scene->matchTransform(from, to, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
          == Result::Success);
    // Moving the guide later cannot change the retained match; sample out of order.
    CHECK(scene->play(Animation::shift(guideGroup, {1.0f, -1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(_placed(renderer, scene, c, 2.0f, 0.0f, 1.0f) && _placed(renderer, scene, d, 2.0f, 2.0f, 1.0f));
    CHECK(_placed(renderer, scene, c, 1.5f, -2.0f, 1.0f) && _placed(renderer, scene, d, 1.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, c, 1.0f, -2.0f, -1.0f) && _placed(renderer, scene, d, 1.0f, 0.0f, -1.0f));
    CHECK(_placed(renderer, scene, c, 1.25f, -2.0f, 0.0f) && _placed(renderer, scene, d, 1.75f, 1.0f, 1.0f));
    CHECK(_placed(renderer, scene, a, 1.5f, -2.0f, -1.0f) && _placed(renderer, scene, b, 3.0f, 0.0f, -1.0f));
    delete scene;
}

// Copied transforms animate a cursor-time clone and leave the source in place.
void copiedTransforms(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    Vec3 corners[] = {{-4.0f, 2.0f, 0.0f}, {-2.0f, 2.0f, 0.0f}, {-2.0f, 4.0f, 0.0f}, {-4.0f, 4.0f, 0.0f}};
    auto square = Polygon::gen(corners, 4u);
    auto disc = Circle::gen({3.0f, 3.0f, 0.0f}, 1.0f);
    auto spare = Circle::gen({3.0f, -1.0f, 0.0f}, 1.0f);
    auto left = Circle::gen({-3.0f, -3.0f, 0.0f}, 0.5f);
    auto right = Circle::gen({3.0f, -3.0f, 0.0f}, 0.5f);
    auto gone = Circle::gen({0.0f, -4.0f, 0.0f}, 0.5f);
    auto into = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto other = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    auto from = Group::gen();
    auto to = Group::gen();
    auto moving = Group::gen();
    auto p = Circle::gen({-3.0f, -2.0f, 0.0f}, 0.5f);
    auto a = Text::gen("a", {-1.0f, -2.0f, 0.0f});
    auto q = Circle::gen({3.0f, -4.0f, 0.0f}, 0.5f);
    auto a2 = Text::gen("a", {1.0f, -4.0f, 0.0f});
    auto d = Text::gen("a", {0.0f, 4.0f, 0.0f});
    Object* objects[] = {square, disc, spare, left, right, gone, into, other, from, to, moving, p, a, q, a2, d};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    square->tag("square");
    from->tag("from");
    CHECK(scene->add(square) == Result::Success && scene->add(spare) == Result::Success);
    CHECK(scene->add(disc) == Result::Success);
    auto morph = Animation::morph(square, disc);
    morph.copy = true;
    CHECK(scene->play(morph, 1.0f) == Result::Success && scene->wait(1.0f) == Result::Success);
    auto copy = scene->object("square:copy");
    CHECK(copy && copy->type() == Type::Polygon && copy->parent() == square->parent());
    // The square stays; its copy turns into the disc and leaves as the disc arrives.
    BBox bounds;
    CHECK(_placed(renderer, scene, square, 0.5f, -3.0f, 3.0f) && _placed(renderer, scene, square, 1.5f, -3.0f, 3.0f));
    CHECK(renderer->bounds(scene, copy, 0.5f, bounds) == Result::Success);
    CHECK(bounds.center().x > 60.0f && bounds.center().x < 140.0f);
    CHECK(renderer->bounds(scene, copy, 1.5f, bounds) != Result::Success);
    CHECK(renderer->bounds(scene, disc, 0.5f, bounds) != Result::Success);
    CHECK(_placed(renderer, scene, disc, 1.5f, 3.0f, 3.0f));

    // Only a morph plays from a copy, and a play that fails leaves no copy behind.
    auto count = scene->count();
    auto fade = Animation::fade(square, 0.5f);
    fade.copy = true;
    CHECK(scene->play(fade, 1.0f) == Result::InvalidArguments && _says(scene, "only a morph can play from a copy"));
    auto stale = Animation::morph(square, spare);
    stale.copy = true;
    CHECK(scene->play(stale, 1.0f) == Result::InsufficientCondition && _says(scene, "right before the morph"));
    CHECK(scene->count() == count && !scene->object("square:copy2"));
    // One object may give a copy to each of several morphs at once.
    CHECK(scene->add(left) == Result::Success && scene->add(right) == Result::Success);
    Animation both[] = {Animation::morph(square, left), Animation::morph(square, right)};
    both[0].copy = both[1].copy = true;
    CHECK(scene->play(both, 2u, 1.0f, AnimCurve{}, 0.0f) == Result::Success);
    CHECK(scene->object("square:copy2") && scene->object("square:copy3"));
    // When a later copy cannot be made, the earlier ones go again.
    CHECK(scene->add(gone) == Result::Success && scene->wait(0.5f) == Result::Success);
    CHECK(scene->remove(gone) == Result::Success);
    CHECK(scene->add(into) == Result::Success && scene->add(other) == Result::Success);
    count = scene->count();
    Animation broken[] = {Animation::morph(square, into), Animation::morph(gone, other)};
    broken[0].copy = broken[1].copy = true;
    CHECK(scene->play(broken, 2u, 1.0f, AnimCurve{}, 0.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "does not show at the cursor"));
    CHECK(scene->count() == count && !scene->object("square:copy4"));

    // A match from a copy leaves its source in place; pairs still name pieces of the source,
    // so the target circle starts where the source circle stands and travels to its own place.
    CHECK(from->add(p) == Result::Success && from->add(a) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
    CHECK(to->add(q) == Result::Success && to->add(a2) == Result::Success && scene->add(to) == Result::Success);
    auto start = scene->cursor();
    MatchOptions options;
    options.copy = true;
    Object* pairs[] = {p, q};
    options.pairs = pairs;
    options.pairCount = 1u;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success);
    auto fromCopy = scene->object("from:copy");
    CHECK(fromCopy && fromCopy->childCount() == 2u && fromCopy->parent() == from->parent());
    if (fromCopy && fromCopy->childCount() == 2u) {
        auto middle = start + 0.5f;
        auto after = start + 1.5f;
        auto copied = fromCopy->childAt(0);
        CHECK(_placed(renderer, scene, p, middle, -3.0f, -2.0f) && _placed(renderer, scene, p, after, -3.0f, -2.0f));
        CHECK(_placed(renderer, scene, q, middle, 0.0f, -3.0f) && _placed(renderer, scene, q, after, 3.0f, -4.0f));
        // The copy of a circle that pairs with a circle leaves at once, as the circle takes its place.
        CHECK(renderer->bounds(scene, copied, middle, bounds) != Result::Success);
        // The source meets the copy and the target on the way, as the copy meets the target.
        CHECK(scene->handoff(a, a2, middle) && scene->handoff(p, copied, middle) && scene->handoff(copied, q, middle));
        CHECK(!scene->handoff(a, q, middle) && !scene->handoff(p, a2, middle));
        CHECK(!scene->handoff(a, copied, middle) && !scene->handoff(p, fromCopy->childAt(1), middle));
        CHECK(!scene->handoff(p, q, after) && !scene->handoff(p, copied, after));
        // The pieces the match lets go of at once never show, so the audit passes over them.
        CHECK(!scene->shows(copied) && !scene->shows(fromCopy->childAt(1)));
        CHECK(scene->shows(fromCopy) && scene->shows(p) && scene->shows(a) && scene->shows(a2));
    }
    // A copied shape may split into several targets. Each extra meets the corresponding
    // original, but unrelated siblings and labels retain ordinary collision checks.
    auto splitFrom = Group::gen();
    auto splitTo = Group::gen();
    auto seed = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto first = Circle::gen({1.0f, 1.0f, 0.0f}, 0.5f);
    auto extra = Circle::gen({1.0f, -1.0f, 0.0f}, 0.5f);
    auto sibling = Text::gen("unrelated", {0.0f, 2.0f, 0.0f});
    CHECK(splitFrom && splitTo && seed && first && extra && sibling);
    CHECK(splitFrom->add(seed) == Result::Success && splitFrom->add(sibling) == Result::Success);
    CHECK(splitTo->add(first) == Result::Success && splitTo->add(extra) == Result::Success);
    CHECK(scene->add(splitFrom) == Result::Success && scene->add(splitTo) == Result::Success);
    auto splitTime = scene->cursor();
    MatchOptions split;
    split.copy = true;
    split.shapes = true;
    split.strings = false;
    CHECK(scene->matchTransform(splitFrom, splitTo, 1.0f, {}, split) == Result::Success);
    CHECK(scene->handoff(seed, first, splitTime + 0.5f) && scene->handoff(seed, extra, splitTime + 0.5f));
    CHECK(!scene->handoff(sibling, first, splitTime + 0.5f) && !scene->handoff(sibling, extra, splitTime + 0.5f));
    // The source may be moving, since its copy starts still; a failed match leaves no copy.
    CHECK(scene->play(Animation::shift(from, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(scene->seek(scene->cursor() - 0.5f) == Result::Success);
    CHECK(moving->add(d) == Result::Success && scene->add(moving) == Result::Success);
    CHECK(scene->matchTransform(from, moving, 1.0f) == Result::InsufficientCondition && _says(scene, "at rest"));
    MatchOptions copying;
    copying.copy = true;
    count = scene->count();
    CHECK(scene->matchTransform(from, to, 1.0f, {}, copying) == Result::InsufficientCondition);
    CHECK(_says(scene, "right before the match") && scene->count() == count && !scene->object("from:copy2"));
    CHECK(scene->matchTransform(from, moving, 1.0f, {}, copying) == Result::Success && scene->object("from:copy2"));
    delete scene;
}

// A warped clone retains sampled geometry; later warps start from that snapshot.
void clonedWarps(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto line = Line::gen({-1.5f, 2.0f, 0.0f}, {1.5f, 2.0f, 0.0f});
    auto pair = Group::gen();
    auto ring = Circle::gen({0.0f, -2.0f, 0.0f}, 1.0f);
    auto rider = Point::gen({-4.0f, -4.0f, 0.0f});
    auto rule = Line::gen({-4.0f, -3.0f, 0.0f}, {-2.0f, -3.0f, 0.0f});
    Object* objects[] = {line, pair, ring, rider, rule};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->tag("line");
    line->stroke(Color::hex("#ffffff"), 1.0f);
    CHECK(pair->add(ring) == Result::Success);
    CHECK(scene->add(line) == Result::Success && scene->add(pair) == Result::Success);
    CHECK(scene->add(rider) == Result::Success);
    Object* bent[] = {line, pair};
    CHECK(scene->warp(bent, 2u, _lifted, nullptr, 1.0f, linear) == Result::Success);

    // Three units right of the source, the copy of the line is the parabola y = 2 + x^2 moved
    // with it, and the copy of the group holds the lifted circle, which spans -3 to -0.75.
    Vec3 right = {3.0f, 0.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &right;
    Object* copy = nullptr;
    CHECK(scene->clone(line, copy, moved) == Result::Success);
    auto curve = copy;
    CHECK(scene->clone(pair, copy, moved) == Result::Success && copy && copy->childCount() == 1u);
    auto lifted = copy && copy->childCount() == 1u ? copy->childAt(0) : nullptr;
    if (!curve || !lifted) {
        delete rule;
        delete scene;
        return;
    }
    Bounds box;
    CHECK(curve->bounds(box) && near(box.min.x, 1.5f) && near(box.max.x, 4.5f) && near(box.min.y, 2.0f)
          && near(box.max.y, 4.25f));
    CHECK(lifted->bounds(box) && near(box.min.y, -3.0f, 2.0e-3f) && near(box.max.y, -0.75f, 2.0e-3f));
    // It draws the images from the moment it is attached, while the source still shows its
    // own flat line before its warp.
    BBox drawn;
    CHECK(renderer->bounds(scene, curve, 0.5f, drawn) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, curve, 1.0f, 3.0f, 3.125f) && _placed(renderer, scene, lifted, 1.0f, 3.0f, -1.875f));
    CHECK(renderer->bounds(scene, curve, 1.0f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));
    CHECK(renderer->bounds(scene, line, 0.0f, drawn) == Result::Success && drawn.height < 3.0f);

    // A warp of the copy goes on from the images, and one of the source's group leaves the
    // copy of its circle where it is.
    auto down = Vec3{0.0f, -1.0f, 0.0f};
    Object* again[] = {curve, pair};
    CHECK(scene->warp(again, 2u, _moved, &down, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, curve, 1.5f, 3.0f, 2.625f) && _placed(renderer, scene, curve, 2.0f, 3.0f, 2.125f));
    CHECK(_placed(renderer, scene, ring, 2.0f, 0.0f, -2.875f) && _placed(renderer, scene, lifted, 2.0f, 3.0f, -1.875f));
    // Its outline is the images too: a dot moved along it ends where the moved parabola ends.
    CHECK(scene->play(Animation::moveAlong(rider, curve), 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, rider, 3.0f, 4.5f, 3.25f));
    // The copied circle keeps the outline its warp gave, so its radius no longer plays, and a
    // copy of the copy takes the images of its latest warp.
    CHECK(scene->play(AnimationTarget::from(lifted).radius(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave"));
    CHECK(scene->clone(curve, copy) == Result::Success && copy->bounds(box) && near(box.max.y, 3.25f));

    // A morph from a copy of the line starts from the parabola, in the line's place.
    CHECK(scene->add(rule) == Result::Success);
    auto morph = Animation::morph(line, rule);
    morph.copy = true;
    auto start = scene->cursor();
    CHECK(scene->play(morph, 1.0f, linear) == Result::Success);
    auto morphed = scene->object("line:copy2");
    CHECK(morphed && _placed(renderer, scene, morphed, start, 0.0f, 3.125f));
    CHECK(morphed && renderer->bounds(scene, morphed, start, drawn) == Result::Success
          && near(drawn.height, 45.0f, 2.0f));
    // A morph into a copy of the line made at the cursor ends in the parabola, which the copy
    // then shows.
    auto flat = Line::gen({-1.5f, -4.0f, 0.0f}, {1.5f, -4.0f, 0.0f});
    CHECK(flat && scene->add(flat) == Result::Success && scene->clone(line, copy) == Result::Success);
    start = scene->cursor();
    CHECK(scene->play(Animation::morph(flat, copy), 1.0f, linear) == Result::Success);
    CHECK(renderer->bounds(scene, flat, start + 0.999f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));
    CHECK(_placed(renderer, scene, copy, start + 1.0f, 0.0f, 3.125f));
    CHECK(renderer->bounds(scene, copy, start + 1.0f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));

    // While a warp runs, neither its object nor a group above it is copied, and a refused clone
    // attaches nothing.
    CHECK(scene->seek(1.5f) == Result::Success);
    auto count = scene->count();
    CHECK(scene->clone(curve, copy) == Result::InsufficientCondition && _says(scene, "middle of a warp"));
    CHECK(scene->clone(pair, copy) == Result::InsufficientCondition && _says(scene, "middle of a warp"));
    CHECK(scene->count() == count);
    delete scene;
}

// How many pixels of row y, from x0 up to x1, have a red channel above 64.
static uint32_t _litRun(const Surface& frame, uint32_t y, uint32_t x0, uint32_t x1)
{
    auto lit = 0u;
    for (auto x = x0; x < x1; x++) lit += framePixel(frame, x, y).r > 64u ? 1u : 0u;
    return lit;
}

// A copy starts from what its source shows at the cursor, also the parts no field of it holds:
// how much of its fill shows, the scale of its pixel sizes, and how its outline draws. A layout
// that moves it before it animates, and a match into it, read its fields into its starting state
// again and keep those parts.
void rebasedCopies(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({-3.0f, 3.0f, 0.0f}, {2.0f, 1.0f});
    Vec3 corners[] = {{-4.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}, {-4.0f, 1.0f, 0.0f}};
    auto square = Polygon::gen(corners, 4u);
    auto ring = Circle::gen({3.0f, 3.0f, 0.0f}, 1.5f);
    auto seed = Circle::gen({-3.0f, -4.0f, 0.0f}, 0.5f);
    Object* objects[] = {box, square, ring, seed};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    box->fill(Color::hex("#ff0000"));
    square->fill(Color::hex("#0000ff"));
    ring->stroke(Color::hex("#ffffff"), 8.0f);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);
    Animation animations[] = {Animation::fillReveal(box), Animation::write(square, DrawDirection::Reverse),
                              Animation::growFromCenter(ring)};
    CHECK(scene->play(animations, 3u, 2.0f, linear, 0.0f) == Result::Success);

    // Halfway, a copy of the box shows half its fill, and one of the square the half written
    // backwards from its first corner, filled as Write fills it. Arranged side by side, they
    // grow into their places as they are.
    CHECK(scene->seek(1.0f) == Result::Success);
    Object* twin = nullptr;
    Object* scribe = nullptr;
    CHECK(scene->clone(box, twin) == Result::Success && scene->clone(square, scribe) == Result::Success);
    Object* items[] = {twin, scribe};
    CHECK(twin && scribe
          && scene->arrange(items, 2u, {1.0f, 0.0f, 0.0f}, 1.0f, {2.0f, 0.0f, 0.0f}, 0.5f, linear)
                 == Result::Success);
    // Matched into, a copy of the circle half grown keeps the stroke it has at that size, half
    // its own 8 pixels.
    CHECK(scene->seek(1.0f) == Result::Success);
    Vec3 down = {0.0f, -6.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &down;
    Object* hoop = nullptr;
    CHECK(scene->clone(ring, hoop, moved) == Result::Success);
    CHECK(hoop && scene->matchTransform(seed, hoop, 0.5f, linear) == Result::Success);

    Surface frame;
    CHECK(renderer->render(scene, 2.5f, frame) == Result::Success);
    if (frame.data() && twin && scribe && hoop) {
        CHECK(_placed(renderer, scene, twin, 2.5f, 0.5f, 0.0f));
        CHECK(_placed(renderer, scene, scribe, 2.5f, 3.5f, 0.0f));
        CHECK(close(framePixel(frame, 110u, 100u), {128, 0, 0, 255}, 2));
        CHECK(close(framePixel(frame, 156u, 86u), {0, 0, 255, 255}, 0));
        CHECK(close(framePixel(frame, 184u, 114u), {0, 0, 0, 255}, 0));
        auto stroke = _litRun(frame, 160u, 135u, 155u);
        CHECK(stroke >= 3u && stroke <= 5u);
        // The sources played on, unchanged.
        CHECK(close(framePixel(frame, 40u, 40u), {255, 0, 0, 255}, 0));
        CHECK(_litRun(frame, 40u, 120u, 145u) >= 7u);
    }
    delete scene;
}

// The points a geometry clip moves meet clones, warps and views: a copy keeps the points its
// source shows as its own, a warp maps the points the clips end at and waits for a clip that
// runs, a warped shape takes no new points, and a view draws them on their way.
void geometryCopies(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto line = Line::gen({-4.0f, -2.0f, 0.0f}, {-2.0f, -2.0f, 0.0f});
    auto curve = Curve::gen({1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, -2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}, 30u);
    auto rule = Line::gen({1.0f, 3.0f, 0.0f}, {4.0f, 3.0f, 0.0f});
    Object* objects[] = {line, curve, rule};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->tag("line");
    line->strokeWidth(1.0f);
    curve->tag("curve");
    curve->strokeWidth(1.0f);
    CHECK(scene->add(line) == Result::Success && scene->add(curve) == Result::Success);

    // Over the first second the line rises from y = -2 to y = 2 and the curve bends up.
    const Vec3 raised[] = {{-4.0f, 2.0f, 0.0f}, {-2.0f, 2.0f, 0.0f}};
    const Vec3 bent[] = {{1.0f, -2.0f, 0.0f}, {2.0f, 2.0f, 0.0f}, {3.0f, 2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}};
    AnimationTarget targets[] = {
        AnimationTarget::from(line).geometry(raised, 2u),
        AnimationTarget::from(curve).geometry(bent, 4u),
    };
    CHECK(scene->play(targets, 2u, 1.0f, linear) == Result::Success);

    // Halfway, a copy of each takes the points its source shows as its own: the line at y = 0
    // and the curve with its controls at y = 0, whose middle point lies at y = -0.5.
    CHECK(scene->seek(0.5f) == Result::Success);
    Object* copy = nullptr;
    CHECK(scene->clone(line, copy) == Result::Success);
    auto level = copy && copy->type() == Type::Line ? static_cast<Line*>(copy) : nullptr;
    copy = nullptr;
    CHECK(scene->clone(curve, copy) == Result::Success);
    auto arch = copy && copy->type() == Type::Curve ? static_cast<Curve*>(copy) : nullptr;
    if (!level || !arch) {
        delete rule;
        delete scene;
        return;
    }
    CHECK(near(level->from.x, -4.0f) && near(level->from.y, 0.0f) && near(level->to.y, 0.0f));
    CHECK(near(arch->controls()[0].y, -2.0f) && near(arch->controls()[1].y, 0.0f));
    CHECK(near(arch->controls()[2].x, 3.0f) && near(arch->controls()[2].y, 0.0f));
    CHECK(arch->count() == 31u && near(arch->points()[15].x, 2.5f) && near(arch->points()[15].y, -0.5f));
    // The copies stay there while the sources go on.
    CHECK(_placed(renderer, scene, level, 0.5f, -3.0f, 0.0f) && _placed(renderer, scene, line, 0.5f, -3.0f, 0.0f));
    CHECK(_placed(renderer, scene, level, 1.0f, -3.0f, 0.0f) && _placed(renderer, scene, line, 1.0f, -3.0f, 2.0f));
    CHECK(_placed(renderer, scene, arch, 1.0f, 2.5f, -1.25f) && _placed(renderer, scene, curve, 1.0f, 2.5f, -0.5f));
    CHECK(scene->save(level) == Result::Success);
    // A copy changes its points from those it was given: the level line sinks to y = -4.
    CHECK(scene->seek(1.0f) == Result::Success);
    const Vec3 sunk[] = {{-4.0f, -4.0f, 0.0f}, {-2.0f, -4.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(level).geometry(sunk, 2u), 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, level, 1.5f, -3.0f, -2.0f) && _placed(renderer, scene, level, 2.0f, -3.0f, -4.0f));

    // A warp maps the points the line's clip ended at, and lifts the line from y = 2 to y = 3.
    auto up = Vec3{0.0f, 1.0f, 0.0f};
    Object* lifting[] = {line};
    CHECK(scene->warp(lifting, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, line, 2.0f, -3.0f, 2.0f) && _placed(renderer, scene, line, 2.5f, -3.0f, 2.5f));
    CHECK(_placed(renderer, scene, line, 3.0f, -3.0f, 3.0f));
    // A warped shape keeps the outline its warp gave, so neither it nor a copy of it takes new
    // points.
    CHECK(scene->play(AnimationTarget::from(line).geometry(sunk, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave, so the points that define it no longer change"));
    CHECK(scene->update(AnimationTarget::from(line).geometry(sunk, 2u)) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave"));
    copy = nullptr;
    CHECK(scene->clone(line, copy) == Result::Success && copy);
    if (copy) {
        CHECK(scene->play(AnimationTarget::from(copy).geometry(sunk, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(_placed(renderer, scene, copy, 3.0f, -3.0f, 3.0f));
    }

    // A warp waits for a geometry clip that runs: the curve flattens again from 3 s to 4 s.
    const Vec3 flat[] = {{1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, -2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(curve).geometry(flat, 4u), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(3.5f) == Result::Success);
    Object* bending[] = {curve};
    auto clips = scene->clipCount();
    CHECK(scene->warp(bending, 1u, _moved, &up) == Result::InsufficientCondition);
    CHECK(_says(scene, "'curve' already animates its geometry until 4.00 s"));
    CHECK(scene->clipCount() == clips && near(scene->cursor(), 3.5f));
    // A morph from a copy starts from the points the curve shows then, halfway flat again.
    CHECK(scene->add(rule) == Result::Success);
    auto morph = Animation::morph(curve, rule);
    morph.copy = true;
    CHECK(scene->play(morph, 0.5f, linear) == Result::Success);
    auto morphed = scene->object("curve:copy2");
    CHECK(morphed && _placed(renderer, scene, morphed, 3.5f, 2.5f, -1.25f));
    CHECK(near(scene->cursor(), 4.0f));
    CHECK(scene->warp(bending, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, curve, 4.0f, 2.5f, -2.0f) && _placed(renderer, scene, curve, 5.0f, 2.5f, -1.0f));

    // A restore leaves the images of a warp, as for other shapes: the level line, saved at y = 0,
    // stays where the warp takes it.
    Object* sinking[] = {level};
    CHECK(scene->warp(sinking, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(scene->restore(level, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, level, 6.0f, -3.0f, -3.0f) && _placed(renderer, scene, level, 7.0f, -3.0f, -3.0f));
    delete scene;

    // A view draws the points on their way: a line that rises through y = 0 at 0.5 s, shown twice
    // as close in the lower right quarter, where the view's center is (-2.5, 0).
    auto viewed = Scene::gen(config);
    auto beam = Line::gen({-3.5f, -1.0f, 0.0f}, {-1.5f, -1.0f, 0.0f});
    CHECK(viewed && beam);
    if (!viewed || !beam) {
        delete beam;
        delete viewed;
        return;
    }
    auto white = Color::hex("#ffffff");
    auto blue = Color::hex("#0000ff");
    beam->stroke(white, 4.0f);
    CHECK(viewed->add(beam) == Result::Success);
    SceneView view;
    view.viewport = {0.5f, 0.5f, 0.5f, 0.5f};
    view.camera.eye = {-2.5f, 0.0f, 10.0f};
    view.camera.target = {-2.5f, 0.0f, 0.0f};
    view.camera.orthoHeight = 2.5f;
    view.optical = false;
    view.background = blue;
    CHECK(viewed->view(view) == Result::Success);
    const Vec3 high[] = {{-3.5f, 1.0f, 0.0f}, {-1.5f, 1.0f, 0.0f}};
    CHECK(viewed->play(AnimationTarget::from(beam).geometry(high, 2u), 1.0f, linear) == Result::Success);
    Surface surface;
    // 40 pixels per unit in the view: the beam at y = -1, 0 and 1 crosses rows 190, 150 and 110.
    CHECK(renderer->render(viewed, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 190u) == pixel(white) && _pixelAt(surface, 150u, 150u) == pixel(blue));
    CHECK(renderer->render(viewed, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 150u) == pixel(white) && _pixelAt(surface, 150u, 190u) == pixel(blue));
    CHECK(_pixelAt(surface, 115u, 150u) == pixel(white) && _pixelAt(surface, 50u, 100u) == pixel(white));
    CHECK(renderer->render(viewed, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 110u) == pixel(white) && _pixelAt(surface, 150u, 150u) == pixel(blue));
    delete viewed;
}

// The layer an object of the scene has in a layout report at time, or INT32_MIN without one.
static int32_t _reportedLayer(Renderer* renderer, Scene* scene, const Object* object, float time)
{
    LayoutReport report;
    if (renderer->layout(scene, time, report) != Result::Success) return INT32_MIN;
    for (auto i = 0u; i < report.objectCount(); i++) {
        auto item = report.objectAt(i);
        if (item && item->object == object) return item->layer;
    }
    return INT32_MIN;
}

void drawLayers(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto green = Color::hex("#00ff00");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};

    // 20 pixels per unit with the origin at pixel (80, 40): low and high overlap at (50, 40),
    // the shape in the group and the cover at (130, 40).
    auto scene = Scene::gen(config);
    auto low = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto high = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto group = Group::gen();
    auto inner = Rectangle::gen({2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto cover = Rectangle::gen({3.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    Object* objects[] = {low, high, group, inner, cover};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    low->stroke(none).fill(red);
    high->stroke(none).fill(blue);
    inner->stroke(none).fill(green);
    cover->stroke(none).fill(white);
    cover->layer = 1;
    CHECK(group->add(inner) == Result::Success);
    Object* roots[] = {low, high, group, cover};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));

    // Before the timeline starts a group's new layer moves the starting layers below it.
    CHECK(scene->update(AnimationTarget::from(group).layer(2)) == Result::Success);
    CHECK(scene->clipCount() == 0u && group->layer == 2 && inner->layer == 2);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));

    // A higher layer applies as the animation begins.
    CHECK(scene->play(AnimationTarget::from(low).layer(1), 1.0f) == Result::Success);
    CHECK(low->layer == 1);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(red));
    // Afterwards a group's layer moves the objects below it while the group holds it.
    CHECK(scene->update(AnimationTarget::from(group).layer(0)) == Result::Success);
    CHECK(group->layer == 0 && inner->layer == 2);
    CHECK(renderer->render(scene, 0.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));
    // A lower layer applies as the animation ends.
    CHECK(scene->play(AnimationTarget::from(low).layer(-1), 1.0f) == Result::Success);
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(red));
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(blue));
    CHECK(low->layer == -1);
    // The layout report gives the layer each object draws at then.
    CHECK(_reportedLayer(renderer, scene, low, 0.5f) == 1);
    CHECK(_reportedLayer(renderer, scene, low, 2.0f) == -1);
    CHECK(_reportedLayer(renderer, scene, inner, 0.5f) == 2);
    CHECK(_reportedLayer(renderer, scene, inner, 1.5f) == 0);
    // An isolated group draws as one layer at the layer it has then.
    CHECK(group->isolate() == Result::Success);
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));

    // A layer is a channel of its own: a move may run beside it, a second change waits.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(low).layer(3)) == Result::InsufficientCondition);
    CHECK(_says(scene, "already animates its layer until 2.00 s"));
    CHECK(scene->play(AnimationTarget::from(low).shift({0.0f, 0.5f, 0.0f}), 0.5f) == Result::Success);
    delete scene;

    // The layers below a group must stay 32-bit integers.
    auto other = Scene::gen(config);
    auto outer = Group::gen();
    auto deep = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(other && outer && deep);
    if (!other || !outer || !deep) {
        delete other;
        delete outer;
        delete deep;
        return;
    }
    deep->layer = INT32_MAX;
    CHECK(outer->add(deep) == Result::Success);
    CHECK(other->add(outer) == Result::Success);
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::InvalidArguments);
    CHECK(_says(other, "32-bit integer"));
    CHECK(outer->layer == 0 && deep->layer == INT32_MAX);
    // And each must still be in its starting state: a direct edit needs update() first.
    deep->layer = 0;
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::InvalidArguments);
    CHECK(_says(other, "below the target was edited directly"));
    CHECK(other->update(deep) == Result::Success);
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::Success);
    CHECK(outer->layer == 1 && deep->layer == 1);
    delete other;
}

// The center of an object's bounds in pixels at time, or (-1, -1) without them.
static Vec2 _centerAt(Renderer* renderer, const Scene* scene, const Object* object, float time)
{
    BBox bounds;
    if (renderer->bounds(scene, object, time, bounds) != Result::Success) return {-1.0f, -1.0f};
    return bounds.center();
}

void saveRestore(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto green = Color::hex("#00ff00");
    auto blue = Color::hex("#0000ff");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    // 20 pixels per unit with the origin at pixel (80, 40): the card at (40, 40), the dot at
    // (120, 40).
    auto scene = Scene::gen(config);
    auto card = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto dot = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto title = Text::gen("cat", {0.0f, 1.5f, 0.0f});
    auto total = Text::gen("");
    Object* objects[] = {card, dot, title, total};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    card->stroke(none).fill(red).tag("card");
    dot->stroke(none).fill(blue);
    CHECK(total->count(5.0f) == Result::Success);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);

    // A save records the state at the cursor and adds nothing to the timeline. Before the
    // timeline it keeps its own copy of a string, which a text may still replace.
    CHECK(scene->save(objects, 4u) == Result::Success);
    CHECK(scene->clipCount() == 0u && near(scene->cursor(), 0.0f));
    CHECK(scene->update(AnimationTarget::from(title).text("dog")) == Result::Success);
    CHECK(scene->clipCount() == 0u && std::strcmp(title->text(), "dog") == 0);

    // A move, a recolor, a shape tween, text and number changes, then a fade that shifts and
    // shrinks the dot.
    AnimationTarget changes[] = {
        AnimationTarget::from(card).shift({1.0f, 1.0f, 0.0f}).fill(green).size(Vec2{2.0f, 1.0f}).corner(0.25f),
        AnimationTarget::from(title).text("horse"),
        AnimationTarget::from(total).value(100.0f),
    };
    CHECK(scene->play(changes, 3u, 1.0f, linear) == Result::Success);
    CHECK(scene->play(Animation::fadeOut(dot, {0.0f, 1.0f, 0.0f}, 0.5f), 1.0f) == Result::Success);
    Surface surface;
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(config.background));

    // Restoring brings every value back over the duration.
    CHECK(scene->restore(objects, 4u, 1.0f, linear) == Result::Success);
    CHECK(near(scene->cursor(), 3.0f));
    auto center = _centerAt(renderer, scene, card, 2.5f);
    CHECK(near(center.x, 50.0f, 1.0f) && near(center.y, 30.0f, 1.0f));
    CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 52u, 40u) == pixel(config.background));
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(blue));
    center = _centerAt(renderer, scene, dot, 3.0f);
    CHECK(near(center.x, 120.0f, 1.0f) && near(center.y, 40.0f, 1.0f));
    CHECK(std::strcmp(_shown(scene, title, 2.0f), "horse") == 0);
    CHECK(std::strcmp(_shown(scene, title, 3.0f), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, total, 2.0f), "100") == 0);
    CHECK(std::strcmp(_shown(scene, total, 3.0f), "5") == 0);
    // The saved values are the objects' latest state again.
    CHECK(near(card->size.x, 1.0f) && near(card->size.y, 1.0f) && near(card->corner, 0.0f));
    CHECK(same(card->style.fill, red) && near(card->model.e[3], 0.0f) && near(card->model.e[7], 0.0f));
    CHECK(near(dot->opacity, 1.0f) && std::strcmp(title->text(), "cat") == 0 && near(total->value(), 5.0f));

    // A restore takes the save, so a second one has nothing to bring back.
    CHECK(scene->restore(card) == Result::InsufficientCondition);
    CHECK(_says(scene, "'card' has no saved state; save it before restoring"));
    CHECK(scene->restore(dot, 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object has no saved state"));

    // Saves nest, and each restore takes the newest state of its target.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).fill(blue), 1.0f) == Result::Success);
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).shift({0.0f, 1.0f, 0.0f}).opacity(0.5f), 1.0f) == Result::Success);
    auto clips = scene->clipCount();
    // Without a duration the values switch at the cursor.
    CHECK(scene->restore(card) == Result::Success);
    CHECK(near(scene->cursor(), 5.0f) && scene->clipCount() == clips + 1u);
    center = _centerAt(renderer, scene, card, 4.99f);
    CHECK(near(center.y, 20.0f, 1.0f));
    CHECK(renderer->render(scene, 5.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(blue));
    CHECK(scene->restore(card, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 6.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(scene->restore(card) == Result::InsufficientCondition);
    // When nothing differs, a restore waits for its duration.
    CHECK(scene->save(card) == Result::Success);
    clips = scene->clipCount();
    CHECK(scene->restore(card, 0.5f) == Result::Success);
    CHECK(scene->clipCount() == clips && near(scene->cursor(), 6.5f));

    // A change of place alone adds to a motion that still runs, as a shift does.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).shift({2.0f, 0.0f, 0.0f}), 2.0f, linear) == Result::Success);
    CHECK(scene->seek(7.5f) == Result::Success);
    CHECK(scene->restore(card, 1.0f, linear) == Result::Success);
    CHECK(near(_centerAt(renderer, scene, card, 7.5f).x, 60.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, card, 8.0f).x, 50.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, card, 8.5f).x, 40.0f, 1.0f));
    CHECK(near(card->model.e[3], 0.0f));
    // A turn must end first, and a failed restore keeps the save.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(Animation::rotate(card, 1.0f), 1.0f) == Result::Success);
    CHECK(scene->seek(9.0f) == Result::Success);
    CHECK(scene->restore(card, 0.25f) == Result::InsufficientCondition);
    CHECK(_says(scene, "'card' already animates its transform until 9.50 s; begin at that time or later"));
    CHECK(scene->seek(9.5f) == Result::Success);
    CHECK(scene->restore(card, 0.5f) == Result::Success);
    CHECK(near(card->model.e[0], 1.0f) && near(card->model.e[1], 0.0f));

    // Later targets begin later by the lag, as in play().
    Object* pair[] = {card, dot};
    CHECK(scene->save(pair, 2u) == Result::Success);
    AnimationTarget away[] = {AnimationTarget::from(card).shift({0.0f, 1.0f, 0.0f}),
                              AnimationTarget::from(dot).shift({0.0f, 1.0f, 0.0f})};
    CHECK(scene->play(away, 2u, 1.0f) == Result::Success);
    CHECK(scene->restore(pair, 2u, 1.0f, linear, 0.5f) == Result::Success);
    CHECK(near(scene->cursor(), 12.5f));
    CHECK(near(_centerAt(renderer, scene, card, 11.5f).y, 30.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, dot, 11.5f).y, 20.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, dot, 12.5f).y, 40.0f, 1.0f));

    // Errors name the problem and change nothing.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->restore(card, -1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "zero or a positive number of seconds"));
    CHECK(scene->restore(nullptr) == Result::InvalidArguments);
    CHECK(_says(scene, "not part of this scene"));
    CHECK(scene->restore(objects, 0u) == Result::InvalidArguments);
    CHECK(_says(scene, "nothing to restore"));
    Object* twice[] = {card, card};
    CHECK(scene->save(twice, 2u) == Result::InvalidArguments);
    CHECK(_says(scene, "only once in one call"));
    CHECK(scene->restore(twice, 2u) == Result::InvalidArguments);
    CHECK(scene->save(nullptr) == Result::InvalidArguments);
    CHECK(scene->remove(dot) == Result::Success);
    CHECK(scene->save(dot) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    CHECK(scene->restore(card) == Result::Success);

    // A string saved on the timeline is one the text keeps, so a restore makes no copy of it,
    // even after more changes than a text looks back through for a string that reads the same.
    CHECK(scene->save(title) == Result::Success);
    char word[8];
    for (auto i = 0u; i < 17u; i++) {
        std::snprintf(word, sizeof(word), "w%u", i);
        CHECK(scene->play(AnimationTarget::from(title).text(word), 0.1f) == Result::Success);
    }
    auto strings = title->texts(nullptr, 0u);
    CHECK(scene->restore(title) == Result::Success);
    CHECK(title->texts(nullptr, 0u) == strings && std::strcmp(title->text(), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, title, scene->cursor()), "cat") == 0);
    delete scene;

    // A group comes back with the objects below it, each in the layer that keeps the order it
    // had: inner draws over under (layer -1) and below cover (layer 1). Pixel (110, 40) shows
    // inner or under, pixel (130, 40) cover or inner.
    auto other = Scene::gen(config);
    auto group = Group::gen();
    auto inner = Rectangle::gen({2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto gone = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto late = Circle::gen({-2.0f, 1.0f, 0.0f}, 0.25f);
    auto under = Rectangle::gen({1.5f, 0.0f, 0.0f}, {1.0f, 2.0f});
    auto cover = Rectangle::gen({3.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    Object* parts[] = {group, inner, gone, late, under, cover};
    created = other != nullptr;
    for (auto object : parts) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : parts) delete object;
        delete other;
        return;
    }
    inner->stroke(none).fill(green);
    under->stroke(none).fill(red);
    under->layer = -1;
    cover->stroke(none).fill(white);
    cover->layer = 1;
    CHECK(group->add(inner) == Result::Success && group->add(gone) == Result::Success);
    CHECK(other->add(group) == Result::Success && other->add(under) == Result::Success
          && other->add(cover) == Result::Success);
    CHECK(other->save(group) == Result::Success);
    // Objects added below the group since the save stay as they are.
    CHECK(group->add(late) == Result::Success);
    // Before the timeline the group's new layer moves the starting layers below it.
    CHECK(other->update(AnimationTarget::from(group).layer(2)) == Result::Success);
    CHECK(group->layer == 2 && inner->layer == 2);
    AnimationTarget raised[] = {AnimationTarget::from(inner).fill(blue).layer(4),
                                AnimationTarget::from(late).fill(blue)};
    CHECK(other->play(raised, 2u, 1.0f) == Result::Success);
    CHECK(renderer->render(other, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) != pixel(white));
    // Objects removed since the save stay removed.
    CHECK(other->remove(gone) == Result::Success);
    CHECK(other->restore(group, 1.0f) == Result::Success);
    // The higher layer holds until the restore ends, then the saved order is back.
    CHECK(_reportedLayer(renderer, other, inner, 1.5f) == 4);
    CHECK(_reportedLayer(renderer, other, inner, 2.0f) == 0);
    CHECK(renderer->render(other, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) != pixel(white));
    CHECK(renderer->render(other, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 40u) == pixel(green));
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));
    CHECK(group->layer == 0 && inner->layer == 2 && same(inner->style.fill, green));
    CHECK(same(late->style.fill, blue));

    // An object below the target edited directly must be updated first.
    CHECK(other->save(group) == Result::Success);
    inner->opacity = 0.25f;
    CHECK(other->restore(group) == Result::InvalidArguments);
    CHECK(_says(other, "below the target was edited directly"));
    CHECK(other->save(group) == Result::InvalidArguments);
    inner->opacity = 1.0f;
    CHECK(other->restore(group) == Result::Success);
    delete other;
}
