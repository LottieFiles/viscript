// Fails the allocations of a scene call one at a time, as a system out of memory would, and
// checks that each call that gives up for want of memory says so in its diagnostic.
#include <cstdlib>
#include <cstring>
#include <new>

#include "vis.h"
#include "testCheck.h"

using namespace vis;

// The nothrow allocations, the ones the library makes, to let through before one fails, or -1 to
// fail none; and whether one failed since it was last set. They come from malloc(), as the plain
// ones do, which ThorVG's operator delete frees.
static long _left = -1;
static bool _failed = false;

static void* _allocate(std::size_t size) noexcept
{
    if (_left == 0) {
        _left = -1;
        _failed = true;
        return nullptr;
    }
    if (_left > 0) _left--;
    return std::malloc(size ? size : 1u);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return _allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return _allocate(size);
}

// Lets index allocations through, then fails one.
static void _arm(long index)
{
    _left = index;
    _failed = false;
}

static void _disarm()
{
    _left = -1;
}

// Checks that call, run with allocation index failing, says it ran out of memory if it did.
static void _says(Result result, const Scene* scene, const char* call, long index)
{
    if (result != Result::OutOfMemory) return;
    auto reason = scene->diagnostic();
    auto said = reason && std::strstr(reason, "memory");
    if (!said) std::fprintf(stderr, "%s, failing allocation %ld, said \"%s\"\n", call, index, reason ? reason : "");
    CHECK(said);
}

// Runs attempt with its first allocation failing, then its second and so on, until it makes them
// all; it then succeeds.
template<typename Attempt>
static void _sweep(Attempt attempt)
{
    for (long index = 0; index < 100000; index++) {
        auto result = attempt(index);
        if (_failed) continue;
        CHECK(result == Result::Success);
        return;
    }
    CHECK(false);
}

// Adding a group with a text, which takes the font of its role from the theme.
static void _add()
{
    _sweep([](long index) {
        auto scene = Scene::gen();
        auto group = Group::gen();
        auto label = Text::gen("label", {});
        auto dot = Circle::gen({}, 0.5f);
        if (!scene || !group || !label || !dot || group->add(label) != Result::Success) {
            delete dot;
            delete label;
            delete group;
            delete scene;
            return Result::InsufficientCondition;
        }
        if (group->add(dot) != Result::Success) delete dot;
        _arm(index);
        auto result = scene->add(group);
        _disarm();
        _says(result, scene, "add", index);
        if (result != Result::Success) delete group;
        delete scene;
        return result;
    });
}

// A move along a circle, which samples the circle into a path.
static void _moveAlong()
{
    _sweep([](long index) {
        auto scene = Scene::gen();
        auto rider = Circle::gen({2.0f, 0.0f, 0.0f}, 0.2f);
        auto track = Circle::gen({}, 2.0f);
        if (!scene || !rider || !track || scene->add(rider) != Result::Success) {
            delete track;
            delete rider;
            delete scene;
            return Result::InsufficientCondition;
        }
        if (scene->add(track) != Result::Success) delete track;
        _arm(index);
        auto result = scene->play(Animation::moveAlong(rider, track), 1.0f);
        _disarm();
        _says(result, scene, "moveAlong", index);
        delete scene;
        return result;
    });
}

// A match of two shapes along a path.
static void _match()
{
    _sweep([](long index) {
        auto scene = Scene::gen();
        auto from = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
        auto to = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 0.8f});
        auto track = Line::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f});
        if (!scene || !from || !to || !track || scene->add(from) != Result::Success) {
            delete track;
            delete to;
            delete from;
            delete scene;
            return Result::InsufficientCondition;
        }
        if (scene->add(track) != Result::Success) delete track;
        if (scene->add(to) != Result::Success) delete to;
        MatchOptions options;
        options.path = track;
        _arm(index);
        auto result = scene->matchTransform(from, to, 1.0f, {}, options);
        _disarm();
        _says(result, scene, "matchTransform", index);
        delete scene;
        return result;
    });
}

// A transition between two scenes that match a tagged shape.
static void _transition()
{
    _sweep([](long index) {
        auto root = Scene::gen();
        auto first = Scene::gen();
        auto second = Scene::gen();
        auto circle = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
        auto square = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
        auto added = root && first && second && circle && square;
        if (added) {
            circle->tag("hero");
            square->tag("hero");
            if (first->add(circle) == Result::Success) circle = nullptr;
            if (second->add(square) == Result::Success) square = nullptr;
            added = !circle && !square;
        }
        if (!added) {
            delete square;
            delete circle;
            delete second;
            delete first;
            delete root;
            return Result::InsufficientCondition;
        }
        Scene* stages[] = {first, second};
        _arm(index);
        auto result = root->transition(stages, 2u, {}, 1.0f, 0.25f);
        _disarm();
        _says(result, root, "transition", index);
        if (result != Result::Success) {
            delete second;
            delete first;
        }
        delete root;
        return result;
    });
}

int main()
{
    _add();
    _moveAlong();
    _match();
    _transition();
    return report();
}
