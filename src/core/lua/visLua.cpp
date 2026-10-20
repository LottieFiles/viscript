// The Lua binding of the scene API: the vis.* library, the factories that make objects, and the
// methods of scenes, objects, views and trackers, held to the budgets one script may spend.
// Sections, each marked "// ---------- name ----------":
//  Budgets and handles / Styles, charges and attaching / Lines, arrows, points and text / Spans
//  / Code blocks / Matrices, tables, groups and spaces / Circles, arcs, angles and polygons
//  / Plots, areas and regions / Riemann sums, contours and fields
//  / Routes, paths, curves and surfaces / Pictures, cells, connectors and samples / Layout methods
//  / Timing and motion options / Braces / Playing animations and targets
//  / Reveal, transform and warp animations / Fades, emphasis and morphs
//  / Code edits, highlights and group transitions / Object styles and queries / Cameras
//  / Waiting, removing and cloning / Timeline control and viewports / Scene transitions / Trackers
//  / Focus and views / Themes / Scene creation, method tables and opening

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <locale.h>
#include <limits>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis.h"
#include "../common/visConstants.h"
#include "../common/visFinite.h"
#include "../common/visSort.h"
#include "visCode.h"
#include "visLuaExtension.h"
#include "visLuaApi.h"
#include "visLuaValues.h"
#include "visLuaStyle.h"
#include "visLuaTracker.h"
#include "visScene.h"

namespace vis
{

// ---------- Budgets and handles ----------

// Budgets cover every live draft in one load. Retained tracker states charge geometry
// and 512 bytes/clip; pending samples flush without reducing their total cost.
// Frame sampling preserves cadence and fails on exhaustion instead of resampling.
static constexpr size_t LUA_NATIVE_MEMORY_LIMIT = 64u * 1024u * 1024u;
static constexpr int LUA_INSTRUCTION_LIMIT = 5000000;
static constexpr uint32_t LUA_OBJECT_LIMIT = 4096;
static constexpr uint32_t LUA_CLIP_LIMIT = 65536;
static constexpr uint32_t LUA_GROUP_LIMIT = 1024;
static constexpr uint32_t LUA_POINT_LIMIT = 1000000;
static constexpr uint32_t LUA_PATH_SAMPLE_LIMIT = 1024;
// The clips a frame may read to place every object, as Scene::sampleWork() counts them.
static constexpr uint64_t LUA_SAMPLE_WORK_LIMIT = 4000000;
static constexpr size_t LUA_TEXT_LIMIT = 1024u * 1024u;
static constexpr size_t LUA_ID_LIMIT = 256;
static constexpr uint32_t LUA_PIXEL_LIMIT = 67108864;
static constexpr uint32_t LUA_VIEWPORT_LIMIT = 64;
static constexpr uint32_t LUA_SCENE_LIMIT = LUA_VIEWPORT_LIMIT + 1u;
// Each view draws its scene's objects once more, so a script keeps fewer than the core allows.
static constexpr uint32_t LUA_VIEW_LIMIT = 8;
static constexpr uint32_t LUA_VIEW_HIDDEN_LIMIT = 256;
static constexpr uint32_t LUA_REGION_SHAPES = 64;
static constexpr size_t LUA_SCENE_CHARGE = 512u;
static constexpr size_t LUA_VIEWPORT_CHARGE = 64u;
static constexpr size_t LUA_TRANSITION_CHARGE = 64u;
static constexpr size_t LUA_VIEW_CHARGE = 512u;
static constexpr size_t LUA_CLIP_CHARGE = 512u;
static constexpr size_t LUA_SAVE_CHARGE = 512u;
class NumericLocale
{
public:
    NumericLocale() noexcept
    {
#if defined(__EMSCRIPTEN__)
        ready = true;
#elif defined(_WIN32)
        mode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        if (mode == -1) return;
        auto current = setlocale(LC_NUMERIC, nullptr);
        if (!current || std::strlen(current) >= sizeof(previous)) {
            _configthreadlocale(mode);
            return;
        }
        std::memcpy(previous, current, std::strlen(current) + 1);
        if (!setlocale(LC_NUMERIC, "C")) {
            _configthreadlocale(mode);
            return;
        }
        ready = true;
#else
        numeric = newlocale(LC_NUMERIC_MASK, "C", nullptr);
        if (!numeric) return;
        previous = uselocale(numeric);
        if (!previous) {
            freelocale(numeric);
            numeric = nullptr;
            return;
        }
        ready = true;
#endif
    }

    ~NumericLocale()
    {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
        if (ready) setlocale(LC_NUMERIC, previous);
        if (mode != -1) _configthreadlocale(mode);
#elif !defined(__EMSCRIPTEN__)
        if (ready) uselocale(previous);
        if (numeric) freelocale(numeric);
#endif
    }

    bool available() const noexcept
    {
        return ready;
    }

private:
    bool ready = false;
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int mode = -1;
    char previous[128] = {};
#elif !defined(__EMSCRIPTEN__)
    locale_t numeric = nullptr;
    locale_t previous = nullptr;
#endif
};

struct LuaMemory
{
    size_t used = 0;
    size_t nativeUsed = 0;
    uint32_t scenes = 0;
    uint32_t objects = 0;
    uint32_t clips = 0;
    uint32_t points = 0;
    Lua::AssetResolver resolver = nullptr;
    void* resolverData = nullptr;
    const char* sourcePath = nullptr;
    size_t sourceRoot = 0;
    Theme adaptiveTheme;
    bool hasAdaptiveTheme = false;
    bool adaptiveThemeUsed = false;
    bool authoring = true;
    // Tracks wait for a flush to play their segments (see TRACKER_PRELUDE); a scene call
    // flushes them first unless one is already playing them.
    bool tracksPending = false;
    bool tracksFlushing = false;
};

struct LuaScene
{
    Scene* scene = nullptr;
    size_t nativeUsed = 0;
    uint32_t objects = 0;
    uint32_t clips = 0;
    uint32_t points = 0;
    uint32_t scenes = 1;
    LuaMemory* memory = nullptr;
    Lua::AssetResolver resolver = nullptr;
    void* resolverData = nullptr;
    bool owner = true;
    bool sampleActive = false;  // while _sampleCall runs a function the scene samples
    bool cameraMoved = false;  // homeCamera and homeView hold the camera before the first move
    Camera homeCamera;
    CameraView homeView = CameraView::TwoD;
};

struct LuaObject
{
    LuaScene* scene = nullptr;
    Object* object = nullptr;
    bool owner = false;
};

// A view of a scene made by scene:view; the handle keeps its scene as user value 1.
struct LuaView
{
    LuaScene* scene = nullptr;
    uint32_t index = 0;
    Camera opening;  // the camera the view opened with, to which focus() returns
    CameraView openingView = CameraView::TwoD;
};

static void* _allocate(void* data, void* ptr, size_t oldSize, size_t size)
{
    auto memory = static_cast<LuaMemory*>(data);
    if (!size) {
        if (ptr) memory->used -= oldSize;
        std::free(ptr);
        return nullptr;
    }

    auto previous = ptr ? oldSize : 0;
    if (size > previous && size - previous > LUA_MEMORY_LIMIT - memory->used) return nullptr;
    auto resized = std::realloc(ptr, size);
    if (!resized) return nullptr;
    if (size > previous) memory->used += size - previous;
    else memory->used -= previous - size;
    return resized;
}

static void _close(lua_State* lua, bool memoryOwned) noexcept
{
    if (!lua) return;
    void* data = nullptr;
    lua_getallocf(lua, &data);
    lua_close(lua);
    if (memoryOwned) delete static_cast<LuaMemory*>(data);
}

static void _releaseSceneBudget(LuaScene* scene)
{
    if (!scene || !scene->memory) return;
    auto memory = scene->memory;
    memory->nativeUsed -= scene->nativeUsed;
    memory->scenes -= scene->scenes;
    memory->objects -= scene->objects;
    memory->clips -= scene->clips;
    memory->points -= scene->points;
    scene->memory = nullptr;
}

// Fails the script once a frame of the scene would read more clips than the limit to place its
// objects. A call that adds objects or clips checks after it has charged them, as the scene
// then holds them, and the scene is discarded with the script.
static int _evaluate(lua_State* lua, const LuaScene* scene)
{
    if (scene->scene->sampleWork() > LUA_SAMPLE_WORK_LIMIT) {
        return luaL_error(lua, "scene evaluation limit exceeded");
    }
    return 0;
}

// What a call adds to the budgets of its scene and of the script. The bytes are 64-bit, so a
// charge counted before its check cannot wrap.
struct LuaCharge
{
    uint64_t native = 0;
    uint32_t objects = 0;
    uint32_t points = 0;
    uint32_t clips = 0;
};

// What the scene and the script may both still spend.
static LuaCharge _room(const LuaScene* box)
{
    auto memory = box->memory;
    return {std::min(LUA_NATIVE_MEMORY_LIMIT - box->nativeUsed, LUA_NATIVE_MEMORY_LIMIT - memory->nativeUsed),
            std::min(LUA_OBJECT_LIMIT - box->objects, LUA_OBJECT_LIMIT - memory->objects),
            std::min(LUA_POINT_LIMIT - box->points, LUA_POINT_LIMIT - memory->points),
            std::min(LUA_CLIP_LIMIT - box->clips, LUA_CLIP_LIMIT - memory->clips)};
}

// Stops the script unless charge fits what the scene and the script may still spend, naming the
// first of clips, objects, points and bytes it exceeds; note, if any, follows the bytes message.
static void _fit(lua_State* lua, const LuaScene* box, const LuaCharge& charge, const char* note = nullptr)
{
    auto room = _room(box);
    if (charge.clips > room.clips) luaL_error(lua, "scene animation limit exceeded");
    if (charge.objects > room.objects) luaL_error(lua, "scene object limit exceeded");
    if (charge.points > room.points) luaL_error(lua, "scene point limit exceeded");
    if (charge.native > room.native) {
        luaL_error(lua, "scene native memory limit exceeded%s%s", note ? ": " : "", note ? note : "");
    }
}

// Counts a charge that fit against the scene and the script, once the scene holds what it buys.
static void _spend(LuaScene* box, const LuaCharge& charge)
{
    auto spend = [&](auto& budget) {
        budget.nativeUsed += static_cast<size_t>(charge.native);
        budget.objects += charge.objects;
        budget.points += charge.points;
        budget.clips += charge.clips;
    };
    spend(*box);
    spend(*box->memory);
}

// What count clips cost.
static LuaCharge _clipCharge(uint32_t count)
{
    return {uint64_t{count} * LUA_CLIP_CHARGE, 0, 0, count};
}

static void _limit(lua_State* lua, lua_Debug* debug)
{
    if (lua_getinfo(lua, "Sl", debug)) {
        luaL_error(lua, "%s:%d: instruction limit exceeded", debug->short_src, debug->currentline);
    }
    luaL_error(lua, "instruction limit exceeded");
}

static void _message(char* error, uint32_t size, const char* message)
{
    if (!error || !size) return;
    if (!message) message = "unknown Lua error";
    auto length = std::strlen(message);
    if (length >= size) length = size - 1;
    std::memcpy(error, message, length);
    error[length] = '\0';
}

// Stops a call that authors box once loading is over, once box belongs to a viewport or while
// a sampling callback runs; what names the handle of the call, "scene" or "object".
static void _authoring(lua_State* lua, const LuaScene* box, const char* what)
{
    if (!detail::luaAuthoring(lua)) luaL_error(lua, "%s authoring is unavailable after loading", what);
    if (!box->owner) luaL_error(lua, "scene already belongs to a viewport");
    if (box->sampleActive) luaL_error(lua, "scene authoring is unavailable inside a sampling callback");
}

// Calls the function below the arguments as a sampling callback of box, which cannot author the
// scene while it runs, and returns the status of the call, as lua_pcall does; the flag then goes
// back to what it was.
static int _sampleCall(lua_State* lua, LuaScene* box, int arguments, int results)
{
    auto active = box->sampleActive;
    box->sampleActive = true;
    auto status = lua_pcall(lua, arguments, results, 0);
    box->sampleActive = active;
    return status;
}

// The scene at index for a call that only moves or reads the authoring time, or that a track
// makes itself: the tracks that wait to play stay waiting.
static LuaScene* _sceneQuiet(lua_State* lua, int index = 1)
{
    auto box = static_cast<LuaScene*>(luaL_checkudata(lua, index, SCENE_METATABLE));
    if (!box->scene) luaL_error(lua, "scene is no longer available");
    _authoring(lua, box, "scene");
    return box;
}

// Plays the segments of every track that waits, in every scene of the script, so that a call
// which adds clips or reads them finds the timeline the tracks make. A failure names the track.
static void _flushTracks(lua_State* lua)
{
    void* data = nullptr;
    lua_getallocf(lua, &data);
    auto memory = static_cast<LuaMemory*>(data);
    if (!memory || !memory->tracksPending || memory->tracksFlushing) return;
    memory->tracksPending = false;
    memory->tracksFlushing = true;
    lua_getfield(lua, LUA_REGISTRYINDEX, TRACKER_FLUSH);
    auto status = lua_pcall(lua, 0, 0, 0);
    memory->tracksFlushing = false;
    if (status != LUA_OK) lua_error(lua);
}

static LuaScene* _scene(lua_State* lua, int index = 1)
{
    auto box = _sceneQuiet(lua, index);
    _flushTracks(lua);
    return box;
}

struct LuaSpace
{
    LuaScene* scene = nullptr;
    Object* parent = nullptr;
};

// The object handle at index, of an object, a space or a group, or nullptr.
static LuaObject* _testHandle(lua_State* lua, int index)
{
    for (auto metatable : {OBJECT_METATABLE, SPACE_METATABLE, GROUP_METATABLE}) {
        if (auto handle = static_cast<LuaObject*>(luaL_testudata(lua, index, metatable))) return handle;
    }
    return nullptr;
}

static LuaObject* _handle(lua_State* lua, int index)
{
    auto handle = _testHandle(lua, index);
    if (!handle) {
        if (lua_istable(lua, index)) luaL_error(lua, "expected one object handle, got a table; animate each object");
        luaL_error(lua, "expected an object handle, got %s", luaL_typename(lua, index));
    }
    if (!handle->scene || !handle->scene->scene || !handle->object) {
        luaL_error(lua, "object handle is no longer available");
    }
    return handle;
}

// Pushes the scene that the scene or handle at index belongs to: a handle keeps as its user
// value the scene, or the handle it was made from, which leads there.
static void _pushScene(lua_State* lua, int index)
{
    lua_pushvalue(lua, index);
    while (!luaL_testudata(lua, -1, SCENE_METATABLE)) {
        if (lua_type(lua, -1) != LUA_TUSERDATA || lua_getiuservalue(lua, -1, 1) == LUA_TNONE) {
            luaL_error(lua, "object handle is no longer available");
        }
        lua_remove(lua, -2);
    }
}

// Pushes what the registry table name keeps for the scene of the scene or handle at index, a
// table made when make is set, or nil.
static void _sceneTable(lua_State* lua, const char* name, int index, bool make)
{
    _pushScene(lua, index);
    lua_getfield(lua, LUA_REGISTRYINDEX, name);
    lua_pushvalue(lua, -2);
    lua_rawget(lua, -2);
    if (lua_isnil(lua, -1) && make) {
        lua_pop(lua, 1);
        lua_createtable(lua, 0, 1);
        lua_pushvalue(lua, -3);
        lua_pushvalue(lua, -2);
        lua_rawset(lua, -4);
    }
    lua_replace(lua, -3);
    lua_pop(lua, 1);
}

// The scene, or the object handle and its scene, at index 1 of a call that adds objects there.
static LuaSpace _space(lua_State* lua)
{
    if (luaL_testudata(lua, 1, SCENE_METATABLE)) return {_sceneQuiet(lua), nullptr};
    auto handle = _handle(lua, 1);
    _authoring(lua, handle->scene, "object");
    return {handle->scene, handle->object};
}

// Opens a factory: the scene or parent it is called on, after checking that the table of options
// at 2 holds only fields and, unless style is false, the shared styles.
static LuaSpace _factory(lua_State* lua, std::initializer_list<const char*> fields, bool style = true)
{
    auto space = _space(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    _options(lua, 2, fields, style);
    return space;
}

// Whether the options argument at index is given: nil leaves it out, and a table may name only
// fields, and the shared styles with style.
static bool _optionTable(lua_State* lua, int index, std::initializer_list<const char*> fields, bool style = false)
{
    if (lua_isnoneornil(lua, index)) return false;
    luaL_checktype(lua, index, LUA_TTABLE);
    _options(lua, index, fields, style);
    return true;
}

// Stops a factory whose options at 2 name shape, a style the object it makes cannot take; why
// says so, with %s for the style.
static void _refuse(lua_State* lua, const char* shape, const char* why)
{
    if (_has(lua, 2, shape)) luaL_error(lua, why, shape);
}

// The object handle at index 1 of a layout method, while its scene can still be authored.
static LuaObject* _layoutHandle(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    _authoring(lua, handle->scene, "object");
    return handle;
}

// The metatable of a handle of object: spaces and groups have methods of their own.
static const char* _handleMetatable(const Object* object)
{
    if (object && object->type() == Type::Space) return SPACE_METATABLE;
    if (object && object->type() == Type::Group) return GROUP_METATABLE;
    return OBJECT_METATABLE;
}

static LuaObject* _objectHandle(lua_State* lua, LuaScene* scene, int sceneIndex,
                               Object* object, bool owner)
{
    sceneIndex = lua_absindex(lua, sceneIndex);
    auto handle = new (lua_newuserdatauv(lua, sizeof(LuaObject), 1)) LuaObject;
    handle->scene = scene;
    handle->object = object;
    handle->owner = owner;
    luaL_getmetatable(lua, _handleMetatable(object));
    lua_setmetatable(lua, -2);
    lua_pushvalue(lua, sceneIndex);
    lua_setiuservalue(lua, -2, 1);
    return handle;
}

static LuaObject* _owner(lua_State* lua, LuaScene* scene)
{
    return _objectHandle(lua, scene, 1, nullptr, true);
}

// Hands the object a factory made to its owner handle, which frees it if the script stops before
// it attaches. The owner comes first, as making it can stop the script too.
template<typename T>
static T* _adopt(LuaObject* owner, T* object)
{
    owner->object = object;
    return object;
}

// Records the handle at index, whose object is now attached, as the handle of that object.
static void _remember(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    auto handle = static_cast<LuaObject*>(lua_touserdata(lua, index));
    if (!handle || !handle->object) return;
    lua_getfield(lua, LUA_REGISTRYINDEX, HANDLE_REGISTRY);
    lua_pushlightuserdata(lua, handle->object);
    lua_pushvalue(lua, index);
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}

// Pushes the handle of an attached object: the one already given out while it lives, so a
// lookup returns the very handle that created the object, or a new one.
static LuaObject* _knownHandle(lua_State* lua, LuaScene* scene, int sceneIndex, Object* object)
{
    sceneIndex = lua_absindex(lua, sceneIndex);
    lua_getfield(lua, LUA_REGISTRYINDEX, HANDLE_REGISTRY);
    lua_pushlightuserdata(lua, object);
    lua_rawget(lua, -2);
    lua_remove(lua, -2);
    auto known = static_cast<LuaObject*>(lua_touserdata(lua, -1));
    if (known && known->scene == scene && known->object == object) return known;
    lua_pop(lua, 1);
    auto handle = _objectHandle(lua, scene, sceneIndex, object, false);
    _remember(lua, -1);
    return handle;
}

// Makes room for one more item in the userdata buffer at slot, which holds count items in room
// for capacity: a full buffer is copied to one twice as large, or of 16 items at first, but of
// at most limit, which the caller keeps count below; the copy takes the slot. Returns where the
// items are.
template<typename T>
static T* _grow(lua_State* lua, int slot, T* items, uint32_t count, uint32_t& capacity, uint32_t limit = UINT32_MAX)
{
    if (count < capacity) return items;
    capacity = std::min(limit, capacity ? 2u * capacity : 16u);
    auto grown = static_cast<T*>(lua_newuserdatauv(lua, sizeof(T) * capacity, 0));
    std::memcpy(static_cast<void*>(grown), items, sizeof(T) * count);
    lua_replace(lua, slot);
    return grown;
}

// ---------- Styles, charges and attaching ----------

// The id of the options at index, which no object of scene has yet, or nullptr. An id is left on
// the stack for the caller to pop once it is copied, since an id given as a number becomes a
// string that only the stack holds.
static const char* _fieldId(lua_State* lua, Scene* scene, int index)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "id");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return nullptr;
    }
    size_t size;
    auto id = _checkString(lua, -1, "id", &size);
    if (!size || size > LUA_ID_LIMIT || std::strlen(id) != size) {
        luaL_error(lua, "id must be 1 to %d bytes without zero bytes", static_cast<int>(LUA_ID_LIMIT));
    }
    for (auto i = 0u; i < size; i++) {
        auto c = id[i];
        auto allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/' || c == ':';
        if (!allowed) luaL_error(lua, "id may contain only ASCII letters, digits, _, -, ., / and :");
    }
    if (scene->object(id)) luaL_error(lua, "duplicate id '%s'", id);
    return id;
}

// Applies the style options at index other than the id, leaving what they do not name.
static void _looks(lua_State* lua, Scene* scene, Object* object, int index);

static void _style(lua_State* lua, Scene* scene, Object* object, int index)
{
    if (auto id = _fieldId(lua, scene, index)) {
        object->tag(id);
        lua_pop(lua, 1);
        if (!object->tag()) luaL_error(lua, "out of memory while copying id");
    }
    _looks(lua, scene, object, index);
}

static void _looks(lua_State* lua, Scene* scene, Object* object, int index)
{
    index = lua_absindex(lua, index);
    Color color;
    auto& theme = scene->theme();
    if (_fieldColor(lua, index, "color", color, &theme)) {
        if (object->style.stroke.a) object->stroke(color);
        if (object->style.fill.a) object->fill(color);
    }
    if (_fieldColor(lua, index, "stroke", color, &theme)) object->stroke(color);
    if (_fieldColor(lua, index, "fill", color, &theme)) object->fill(color);

    lua_getfield(lua, index, "gradient");
    if (!lua_isnil(lua, -1)) {
        if (!_gradientShape(object->type())) {
            luaL_error(lua, "gradient is supported only by vector Shapes");
        }
        if (lua_isboolean(lua, -1)) {
            object->gradient(lua_toboolean(lua, -1));
        } else if (lua_type(lua, -1) == LUA_TSTRING) {
            object->gradient(_color(lua, -1, "gradient", &theme));
        } else if (lua_istable(lua, -1)) {
            auto gradient = _gradient(lua, -1, &theme);
            if (object->gradient(gradient) != Result::Success) {
                luaL_error(lua, "gradient is invalid");
            }
        } else {
            luaL_error(lua, "gradient must be a boolean, end-stop color or gradient table");
        }
    }
    lua_pop(lua, 1);

    if (_has(lua, index, "width")) {
        auto width = _fieldNumber(lua, index, "width", object->style.width);
        if (width < 0.0f) luaL_error(lua, "width cannot be negative");
        // A picture's width is its size, which its factory reads; it has no stroke.
        if (object->type() != Type::Picture) object->strokeWidth(width);
    }
    object->style.radius = _fieldNumber(lua, index, "radius", object->style.radius);
    auto dashOffset = _fieldNumber(lua, index, "dash_offset", 0.0f);
    lua_getfield(lua, index, "dash");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        if (_has(lua, index, "dash_offset")) luaL_error(lua, "dash_offset requires dash");
    } else {
        _checkTable(lua, -1, "dash");
        auto count = lua_rawlen(lua, -1);
        if (!count || count > Style::DashLimit) {
            luaL_error(lua, "dash must contain 1 to %d values", static_cast<int>(Style::DashLimit));
        }
        float pattern[Style::DashLimit];
        for (size_t i = 0; i < count; i++) {
            lua_rawgeti(lua, -1, i + 1u);
            pattern[i] = _number(lua, -1, "dash value");
            lua_pop(lua, 1);
        }
        lua_pop(lua, 1);
        if (object->dash(pattern, static_cast<uint32_t>(count), dashOffset)
            != Result::Success) {
            luaL_error(lua, "dash values must be non-negative with at least one positive value");
        }
    }
    object->opacity = _fieldNumber(lua, index, "opacity", object->opacity);
    object->progress = _fieldNumber(lua, index, "progress", object->progress);
    auto layer = _fieldInteger(lua, index, "layer", object->layer);
    if (object->style.radius <= 0.0f) luaL_error(lua, "radius must be positive");
    if (object->opacity < 0.0f || object->opacity > 1.0f) luaL_error(lua, "opacity must be between 0 and 1");
    if (object->progress < 0.0f || object->progress > 1.0f) luaL_error(lua, "progress must be between 0 and 1");
    if (layer < INT32_MIN || layer > INT32_MAX) luaL_error(lua, "layer is out of range");
    object->layer = static_cast<int32_t>(layer);
    if (_has(lua, index, "isolate")) object->isolate(_fieldBool(lua, index, "isolate", false));
    lua_getfield(lua, index, "blend");
    if (!lua_isnil(lua, -1)) object->blend(_blendMode(lua, -1));
    lua_pop(lua, 1);
    lua_getfield(lua, index, "blur");
    if (!lua_isnil(lua, -1)) object->blur = _blur(lua, -1, "blur");
    lua_pop(lua, 1);
    lua_getfield(lua, index, "shadow");
    if (!lua_isnil(lua, -1)) {
        // A card shadow unless the table says otherwise.
        Shadow card = {{0, 0, 0, 102}, {0.0f, -4.0f}, 8.0f};
        object->shadow = _shadow(lua, -1, &theme, card);
    }
    lua_pop(lua, 1);
}

static size_t _nativeSize(const Object* object, uint32_t& points)
{
    auto size = size_t{512};
    points = 0;
    if (object->tag()) size += std::strlen(object->tag()) + 1;
    switch (object->type()) {
        case Type::Text: {
            auto text = static_cast<const Text*>(object);
            size += std::strlen(text->text()) + 1;
            if (text->font()) size += std::strlen(text->font()) + 1;
            break;
        }
        case Type::Polygon: points = static_cast<const Polygon*>(object)->count(); break;
        case Type::Plot: {
            auto plot = static_cast<const Plot*>(object);
            points = plot->count();
            size += static_cast<size_t>(plot->samples().count) * sizeof(Vec3);
            if (plot->contours() > 1u) size += static_cast<size_t>(plot->contours()) * sizeof(uint32_t);
            break;
        }
        case Type::Route: points = static_cast<const DirectedRoute*>(object)->count(); break;
        case Type::Path: points = static_cast<const Path*>(object)->count(); break;
        case Type::Curve: points = static_cast<const Curve*>(object)->count(); break;
        case Type::SurfaceMesh: {
            auto surface = static_cast<const SurfaceMesh*>(object);
            points = surface->columns() * surface->rows();
            if (surface->colors()) size += static_cast<size_t>(points) * sizeof(Color);
            break;
        }
        case Type::Picture: {
            auto picture = static_cast<const Picture*>(object);
            if (picture->encoded()) size += picture->encodedSize() + std::strlen(picture->mime()) + 1u;
            else size += static_cast<size_t>(picture->pixelWidth()) * picture->pixelHeight() * sizeof(uint32_t);
            break;
        }
        case Type::Cell: {
            auto cell = static_cast<const Cell*>(object);
            size += static_cast<size_t>(cell->columns()) * cell->rows() * sizeof(Color);
            break;
        }
        default: break;
    }
    if (points > (SIZE_MAX - size) / sizeof(Vec3)) return SIZE_MAX;
    return size + static_cast<size_t>(points) * sizeof(Vec3);
}

// Plain words for a result code when nothing more specific is known.
static const char* _explain(Result result)
{
    switch (result) {
        case Result::Success: return "success";
        case Result::InvalidArguments: return "the arguments are invalid";
        case Result::InsufficientCondition: return "the call is not allowed at this point";
        case Result::NonSupport: return "this build does not support it";
        case Result::OutOfMemory: return "out of memory";
        case Result::IoError: return "a file could not be read or written";
        case Result::ScriptError: return "the script failed";
        default: return "an unknown error occurred";
    }
}

// Prefers the scene's explanation of a failed call over a generic one.
static const char* _reason(const Scene* scene, Result result)
{
    auto reason = scene->diagnostic();
    return reason ? reason : _explain(result);
}

// Unless result is a success, stops the script with what format and the arguments after it say
// could not be done, followed by the reason scene gives, as "could not save: <reason>".
static void _check(lua_State* lua, const Scene* scene, Result result, const char* format, ...)
{
    if (result == Result::Success) return;
    auto reason = _reason(scene, result);
    va_list arguments;
    va_start(arguments, format);
    luaL_where(lua, 1);
    lua_pushvfstring(lua, format, arguments);
    va_end(arguments);
    lua_pushfstring(lua, ": %s", reason);
    lua_concat(lua, 3);
    lua_error(lua);
}

static MaskMode _maskMode(lua_State* lua, int index)
{
    static constexpr LuaName<MaskMode> MODES[] = {{"alpha", MaskMode::Alpha}, {"inverse_alpha", MaskMode::InverseAlpha},
                                                  {"luma", MaskMode::Luma}, {"inverse_luma", MaskMode::InverseLuma}};
    return _enum(lua, index, "mask mode", MODES);
}

static Object* _object(lua_State* lua, Scene* scene, int index);

// Applies the mask option of the options at index, a target handle or {target = handle,
// mode = name}, to object once it is attached.
static void _mask(lua_State* lua, Scene* scene, Object* object, int index)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "mask");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    auto mode = MaskMode::Alpha;
    Object* target;
    if (lua_istable(lua, -1)) {
        _options(lua, -1, {"target", "mode"}, false);
        lua_getfield(lua, -1, "target");
        if (lua_isnil(lua, -1)) luaL_error(lua, "mask requires a target");
        target = _object(lua, scene, -1);
        lua_pop(lua, 1);
        lua_getfield(lua, -1, "mode");
        if (!lua_isnil(lua, -1)) mode = _maskMode(lua, -1);
        lua_pop(lua, 1);
    } else {
        target = _object(lua, scene, -1);
    }
    lua_pop(lua, 1);
    _check(lua, scene, object->mask(target, mode), "could not mask object");
}

static bool _treeCharge(const Object* object, LuaCharge& charge, uint32_t depth = 0u) noexcept
{
    if (!object || depth > 256u || charge.objects == UINT32_MAX) return false;
    uint32_t points = 0;
    auto native = _nativeSize(object, points);
    if (native == SIZE_MAX || native > SIZE_MAX - charge.native || points > UINT32_MAX - charge.points) return false;
    charge.native += native;
    charge.objects++;
    charge.points += points;
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_treeCharge(object->childAt(i), charge, depth + 1u)) return false;
    }
    return true;
}

// Attaches object, styled already with everything below it, where space adds objects, charging
// the whole tree to the scene budget, then applies the mask option of the options at index and
// records the handle on top of the stack, which owner is, as the object's handle.
static int _attach(lua_State* lua, const LuaSpace& space, LuaObject* owner, Object* object,
                   int index, const char* name, bool semanticLabel = false)
{
    LuaCharge charge;
    if (!_treeCharge(object, charge)) return luaL_error(lua, "%s is too deep or large", name);
    _fit(lua, space.scene, charge);
    Result result;
    if (semanticLabel) {
        if (!space.parent || object->type() != Type::Text) {
            return luaL_error(lua, "a label requires an Object parent and Text child");
        }
        result = space.parent->label(static_cast<Text*>(object));
    } else {
        result = space.parent ? space.parent->add(object) : space.scene->scene->add(object);
    }
    _check(lua, space.scene->scene, result, "could not add %s", name);
    _spend(space.scene, charge);
    owner->owner = false;
    _mask(lua, space.scene->scene, object, index);
    luaL_getmetatable(lua, _handleMetatable(object));
    lua_setmetatable(lua, -2);
    _remember(lua, -1);
    _evaluate(lua, space.scene);
    return 1;
}

static int _add(lua_State* lua, const LuaSpace& space, LuaObject* owner, Object* object,
                int index, const char* name, bool semanticLabel = false)
{
    owner->object = object;
    if (!object) return luaL_error(lua, "could not create %s: invalid options or out of memory", name);
    _style(lua, space.scene->scene, object, index);
    return _attach(lua, space, owner, object, index, name, semanticLabel);
}

int detail::luaAdoptObject(lua_State* lua, int sceneIndex, Object* root,
                           const char* kind, LuaObjectTransfer transfer,
                           void* transferData, size_t extraNativeBytes)
{
    sceneIndex = lua_absindex(lua, sceneIndex);
    auto scene = _scene(lua, sceneIndex);
    if (!root) return luaL_error(lua, "could not create %s", kind ? kind : "object tree");
    auto owner = _objectHandle(lua, scene, sceneIndex, root, true);
    if (transfer) transfer(transferData);

    LuaCharge charge;
    if (!_treeCharge(root, charge) || extraNativeBytes > SIZE_MAX - charge.native) {
        return luaL_error(lua, "%s is too deep or large", kind ? kind : "object tree");
    }
    charge.native += extraNativeBytes;
    _fit(lua, scene, charge);

    auto result = scene->scene->add(root);
    _check(lua, scene->scene, result, "could not add %s", kind ? kind : "object tree");
    _spend(scene, charge);
    owner->owner = false;
    _remember(lua, -1);
    _evaluate(lua, scene);
    return 1;
}

int detail::luaPushObject(lua_State* lua, int sceneIndex, Object* object)
{
    sceneIndex = lua_absindex(lua, sceneIndex);
    auto scene = _scene(lua, sceneIndex);
    if (!object || object->id() == 0u || scene->scene->object(object->id()) != object) {
        return luaL_error(lua, "object is not attached to this scene");
    }
    _knownHandle(lua, scene, sceneIndex, object);
    return 1;
}

int detail::luaAddObject(lua_State* lua, Object* root, int styleIndex, const char* kind,
                         LuaObjectTransfer transfer, void* transferData)
{
    styleIndex = lua_absindex(lua, styleIndex);
    auto space = _space(lua);
    auto owner = _owner(lua, space.scene);
    owner->object = root;
    if (!kind) kind = "object tree";
    if (!root) return luaL_error(lua, "could not create %s", kind);
    if (transfer) transfer(transferData);
    _style(lua, space.scene->scene, root, styleIndex);
    return _attach(lua, space, owner, root, styleIndex, kind);
}

// The metatables of the handles a module method belongs to.
static uint32_t _methodTables(detail::LuaHandle handle, const char* (&names)[3])
{
    switch (handle) {
        case detail::LuaHandle::Scene: names[0] = SCENE_METATABLE; return 1u;
        case detail::LuaHandle::Group: names[0] = GROUP_METATABLE; return 1u;
        default: break;
    }
    names[0] = OBJECT_METATABLE;
    names[1] = SPACE_METATABLE;
    names[2] = GROUP_METATABLE;
    return 3u;
}

void detail::luaSetMethod(lua_State* lua, LuaHandle handle, const char* name)
{
    const char* names[3] = {};
    auto count = _methodTables(handle, names);
    for (auto i = 0u; i < count; i++) {
        luaL_getmetatable(lua, names[i]);
        lua_getfield(lua, -1, "__index");
        lua_pushvalue(lua, -3);
        lua_setfield(lua, -2, name);
        lua_pop(lua, 2);
    }
    lua_pop(lua, 1);
}

void detail::luaGetMethod(lua_State* lua, LuaHandle handle, const char* name)
{
    const char* names[3] = {};
    _methodTables(handle, names);
    luaL_getmetatable(lua, names[0]);
    lua_getfield(lua, -1, "__index");
    lua_getfield(lua, -1, name);
    lua_replace(lua, -3);
    lua_pop(lua, 1);
}

// Raises the error on top of the stack, as lua_error does, from a method the script called,
// with the line of that call before its message, as luaL_error puts it: an error raised in a
// function that C called names no line of the script, so the method raises it again here.
static int _raise(lua_State* lua)
{
    if (lua_type(lua, -1) == LUA_TSTRING) {
        luaL_where(lua, 1);
        lua_insert(lua, -2);
        lua_concat(lua, 2);
    }
    return lua_error(lua);
}

void detail::luaCall(lua_State* lua, int arguments, int results)
{
    if (lua_pcall(lua, arguments, results, 0) != LUA_OK) _raise(lua);
}

int detail::luaCallWrapped(lua_State* lua, int index)
{
    // A C function without upvalues runs in this call, so its errors are those the script would
    // see from it; any other function is called as luaCall calls.
    auto function = lua_tocfunction(lua, index);
    if (function) {
        if (!lua_getupvalue(lua, index, 1)) return function(lua);
        lua_pop(lua, 1);
    }
    auto arguments = lua_gettop(lua);
    lua_pushvalue(lua, index);
    lua_insert(lua, 1);
    luaCall(lua, arguments, LUA_MULTRET);
    return lua_gettop(lua);
}

void detail::luaSetSpanPart(lua_State* lua, const char* key)
{
    lua_getfield(lua, LUA_REGISTRYINDEX, SPAN_PART_REGISTRY);
    lua_pushstring(lua, key);
    auto known = lua_rawget(lua, -2) != LUA_TNIL;
    lua_pop(lua, 1);
    if (!known) {
        lua_pushstring(lua, key);
        lua_rawseti(lua, -2, static_cast<lua_Integer>(lua_rawlen(lua, -2)) + 1);
    }
    lua_pushstring(lua, key);
    lua_pushvalue(lua, -3);
    lua_rawset(lua, -3);
    lua_pop(lua, 2);
}

static int _sceneGc(lua_State* lua)
{
    auto box = static_cast<LuaScene*>(luaL_checkudata(lua, 1, SCENE_METATABLE));
    if (box->owner && box->scene) {
        _releaseSceneBudget(box);
        delete box->scene;
    }
    box->scene = nullptr;
    box->owner = false;
    box->memory = nullptr;
    return 0;
}

static int _objectGc(lua_State* lua)
{
    auto owner = static_cast<LuaObject*>(lua_touserdata(lua, 1));
    if (owner->owner) delete owner->object;
    owner->object = nullptr;
    owner->scene = nullptr;
    owner->owner = false;
    return 0;
}

static Vec2 _trimDistance(lua_State* lua, int index)
{
    Vec2 value;
    lua_getfield(lua, index, "trim_distance");
    if (lua_type(lua, -1) == LUA_TNUMBER) {
        value.x = value.y = _number(lua, -1, "trim_distance");
    } else if (lua_istable(lua, -1)) {
        if (lua_rawlen(lua, -1) != 2u) luaL_error(lua, "trim_distance must be a number or {start, end}");
        for (auto i = 1; i <= 2; i++) {
            lua_rawgeti(lua, -1, i);
            auto distance = _number(lua, -1, "trim_distance");
            if (i == 1) value.x = distance;
            else value.y = distance;
            lua_pop(lua, 1);
        }
    } else if (!lua_isnil(lua, -1)) {
        luaL_error(lua, "trim_distance must be a number or {start, end}");
    }
    lua_pop(lua, 1);
    if (value.x < 0.0f || value.y < 0.0f) luaL_error(lua, "trim_distance cannot be negative");
    return value;
}

// ---------- Lines, arrows, points and text ----------

static int _line(lua_State* lua)
{
    auto space = _factory(lua, {"from", "to", "trim_distance"});
    auto from = _fieldWorld(lua, 2, "from");
    auto to = _fieldWorld(lua, 2, "to");
    auto owner = _owner(lua, space.scene);
    auto trimDistance = _trimDistance(lua, 2);
    auto object = _adopt(owner, Line::gen(from, to));
    if (object) object->trimDistance = trimDistance;
    return _add(lua, space, owner, object, 2, "line");
}

static int _arrow(lua_State* lua)
{
    auto space = _factory(lua, {"from", "to", "tail", "tip", "trim_distance"});
    auto from = _fieldWorld(lua, 2, "from");
    auto to = _fieldWorld(lua, 2, "to");
    auto tail = _fieldNumber(lua, 2, "tail", 0.0f);
    auto tip = _fieldNumber(lua, 2, "tip", 16.0f);
    if (tail < 0.0f || tip < 0.0f) return luaL_error(lua, "tail and tip must be non-negative");
    auto trimDistance = _trimDistance(lua, 2);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Arrow::gen(from, to));
    if (object) {
        object->trimDistance = trimDistance;
        object->tail = tail;
        object->tip = tip;
    }
    return _add(lua, space, owner, object, 2, "arrow");
}

static int _point(lua_State* lua)
{
    auto space = _factory(lua, {"point"});
    auto point = _fieldWorld(lua, 2, "point");
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Point::gen(point), 2, "point");
}

// Measures a text in world units as the scene sets it, or stops the script with the reason.
static TextExtent _measured(lua_State* lua, Scene* scene, const Text* text)
{
    TextExtent extent;
    auto result = scene->measure(text, extent);
    if (result == Result::InsufficientCondition) {
        luaL_error(lua, "could not measure text: %s, as with viscript render --font", _reason(scene, result));
    }
    _check(lua, scene, result, "could not measure text");
    return extent;
}

// Measures a text as it is made: measure gives it the box of its glyphs, and baseline sets
// align.y so that its point lies on its first baseline, which the box, or else the text as
// measured, pins there.
static void _measureText(lua_State* lua, Scene* scene, Text* text, bool measure, bool baseline)
{
    if (!measure && !baseline) return;
    // The box is part of the text as it attaches, like every other option.
    auto extent = _measured(lua, scene, text);
    if (measure) text->box = {extent.width, extent.height};
    if (baseline) text->align.y = extent.height > 0.0f ? extent.ascent / extent.height : 0.0f;
}

static void _reserveText(lua_State* lua, Scene* scene, Text* text)
{
    lua_getfield(lua, 2, "reserve");
    if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "reserve must be a string");
        size_t length;
        auto sample = lua_tolstring(lua, -1, &length);
        if (std::strlen(sample) != length) luaL_error(lua, "reserve cannot contain zero bytes");
        if (length > LUA_TEXT_LIMIT) luaL_error(lua, "reserve exceeds the 1 MiB limit");
        _check(lua, scene, scene->reserve(text, sample), "could not reserve text");
    }
    lua_pop(lua, 1);
}

static int _textObject(lua_State* lua, bool semanticLabel)
{
    auto space = _factory(lua, {"text", "point", "size", "align", "font", "orientation", "role", "slant", "measure",
                                "baseline", "reserve", "tabular"});
    for (auto shape : {"gradient"}) _refuse(lua, shape, "%s is supported only by vector Shapes");
    size_t size;
    auto value = _fieldString(lua, 2, "text", &size);
    if (size > LUA_TEXT_LIMIT) return luaL_error(lua, "text exceeds the 1 MiB limit");
    auto point = _fieldWorld(lua, 2, "point", {});
    auto measure = _fieldBool(lua, 2, "measure", false);
    auto baseline = _fieldBool(lua, 2, "baseline", false);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Text::gen(value, point));
    _text(lua, 2, object);
    if (object) {
        _measureText(lua, space.scene->scene, object, measure, baseline);
        _reserveText(lua, space.scene->scene, object);
    }
    return _add(lua, space, owner, object, 2, semanticLabel ? "label" : "text", semanticLabel);
}

static int _textWorld(lua_State* lua)
{
    return _textObject(lua, false);
}

static int _label(lua_State* lua)
{
    return _textObject(lua, true);
}

// Pushes an optional string option, which may be empty but holds no zero bytes, and returns
// it, or nullptr when it is nil.
static const char* _fieldText(lua_State* lua, int index, const char* name)
{
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) return nullptr;
    size_t length;
    auto value = _checkString(lua, -1, name, &length);
    if (std::strlen(value) != length) luaL_error(lua, "%s cannot contain zero bytes", name);
    if (length > LUA_TEXT_LIMIT) luaL_error(lua, "%s exceeds the 1 MiB limit", name);
    return value;
}

// scene:number{...}: a text that writes a number, which play, set and trackers animate as
// value, so it counts through every number on the way.
static int _numberText(lua_State* lua)
{
    auto space = _factory(lua, {"value", "decimals", "sign", "separator", "prefix", "suffix", "point", "size", "align",
                                "font", "orientation", "role", "slant", "measure", "baseline", "reserve", "tabular"});
    for (auto shape : {"gradient"}) _refuse(lua, shape, "%s is supported only by vector Shapes");
    auto value = _fieldNumber(lua, 2, "value", 0.0f);
    auto decimals = _fieldNumber(lua, 2, "decimals", 0.0f);
    if (decimals < 0.0f || decimals > 6.0f || decimals != std::floor(decimals)) {
        return luaL_error(lua, "decimals must be a whole number from 0 to 6");
    }
    NumberFormat format;
    format.decimals = static_cast<uint8_t>(decimals);
    format.sign = _fieldBool(lua, 2, "sign", false);
    auto separator = _fieldText(lua, 2, "separator");
    if (separator) {
        if (std::strlen(separator) != 1u || separator[0] < ' ' || separator[0] > '~') {
            return luaL_error(lua, "separator must be one printable ASCII character, such as ',' or ' '");
        }
        format.separator = separator[0];
    }
    lua_pop(lua, 1);
    auto point = _fieldWorld(lua, 2, "point", {});
    auto measure = _fieldBool(lua, 2, "measure", false);
    auto baseline = _fieldBool(lua, 2, "baseline", false);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Text::gen("", point));
    if (object) {
        // The strings stay on the stack until the text copies them.
        auto prefix = _fieldText(lua, 2, "prefix");
        auto suffix = _fieldText(lua, 2, "suffix");
        if (object->count(value, format, prefix, suffix) != Result::Success) {
            return luaL_error(lua, "could not create number: out of memory");
        }
        lua_pop(lua, 2);
        _text(lua, 2, object);
        _measureText(lua, space.scene->scene, object, measure, baseline);
        _reserveText(lua, space.scene->scene, object);
    }
    return _add(lua, space, owner, object, 2, "number");
}

// ---------- Spans ----------

// How spans cuts its strings into texts.
enum struct LuaSplit : uint8_t
{
    None,
    Words,
    Letters
};

// One text of a spans row, or a box a module draws, measured in world units before it
// attaches.
struct LuaSpan
{
    Text* text = nullptr;
    TextExtent extent;
    // How far the baseline rises, and the blank advance before the text.
    float rise = 0.0f;
    float gap = 0.0f;
    // Line feeds before the text.
    uint32_t breaks = 0;
    uint32_t line = 0;
    float x = 0.0f;
    // A box a module draws, and the left end of its baseline once the lines are set.
    bool box = false;
    Vec3 at;
    // The text starts or ends with a blank, where a line may break beside a box.
    bool lead = false;
    bool trail = false;
};

// The lines of a spans row, in world units.
struct LuaSpanLine
{
    float width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    // The ink of the line's texts and of its boxes, as heights above its baseline, once the
    // line holds any; leading reads them to keep the lines apart.
    float textTop = 0.0f;
    float textBottom = 0.0f;
    float boxTop = 0.0f;
    float boxBottom = 0.0f;
    bool texts = false;
    bool boxes = false;
    // With leading, how far the baseline lies below the baseline of the line above.
    float pitch = 0.0f;
};

// With leading, how far below the baseline of upper the baseline of lower lies: pitch, or as
// much more as keeps the ink of lower from reaching into the ink of upper. A box, such as a
// formula, also keeps clearance from the ink of the other line, while texts may touch.
static float _spanPitch(const LuaSpanLine& upper, const LuaSpanLine& lower, float pitch, float clearance)
{
    if (upper.texts && lower.texts) pitch = std::max(pitch, lower.textTop - upper.textBottom);
    if (!(upper.texts || upper.boxes) || !(lower.texts || lower.boxes)) return pitch;
    auto top = lower.texts ? (lower.boxes ? std::max(lower.textTop, lower.boxTop) : lower.textTop) : lower.boxTop;
    auto bottom = upper.texts ? (upper.boxes ? std::min(upper.textBottom, upper.boxBottom) : upper.textBottom)
                              : upper.boxBottom;
    if (upper.boxes) pitch = std::max(pitch, top - upper.boxBottom + clearance);
    if (lower.boxes) pitch = std::max(pitch, lower.boxTop - bottom + clearance);
    return pitch;
}

static constexpr const char* SPAN_STYLES[] = {
    "id", "color", "fill", "opacity", "progress", "layer", "isolate", "mask", "blend", "blur", "shadow"};
static constexpr const char* ROW_STYLES[] = {
    "id", "opacity", "progress", "layer", "isolate", "mask", "blend", "blur", "shadow"};

// Pushes a table of the options at 2 that names lists, as the style a part attaches with, and
// returns its index.
template<size_t N>
static int _pickOptions(lua_State* lua, const char* const (&names)[N])
{
    lua_createtable(lua, 0, static_cast<int>(N));
    auto style = lua_gettop(lua);
    for (auto name : names) {
        lua_getfield(lua, 2, name);
        lua_setfield(lua, style, name);
    }
    return style;
}

// Pushes the id option at 2 and returns it, or nullptr when it is not a string; the stack keeps
// it while the parts of a group take their names from it.
static const char* _pushId(lua_State* lua)
{
    lua_getfield(lua, 2, "id");
    return lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : nullptr;
}

// Names the style at index after the group whose id is id: id/first, or id/first/second when
// second is set. Without an id the style stays unnamed.
static void _childId(lua_State* lua, int index, const char* id, uint32_t first, uint32_t second = 0)
{
    if (!id) return;
    index = lua_absindex(lua, index);
    if (second) lua_pushfstring(lua, "%s/%d/%d", id, static_cast<int>(first), static_cast<int>(second));
    else lua_pushfstring(lua, "%s/%d", id, static_cast<int>(first));
    lua_setfield(lua, index, "id");
}

// Attaches the typeset group of a spans row, code block, matrix or table, with the row styles of
// the options at 2 and model, unless null, as its transform. Leaves its handle on the stack.
static Group* _typeset(lua_State* lua, const LuaSpace& space, const char* name, const Mat4* model = nullptr)
{
    auto style = _pickOptions(lua, ROW_STYLES);
    auto owner = _owner(lua, space.scene);
    auto group = Group::gen();
    if (group) {
        group->typeset = true;
        if (model) group->model = *model;
    }
    _add(lua, space, owner, group, style, name);
    return group;
}

// Spans hold only texts, which take a color but no stroke or outline.
static void _spanOption(lua_State* lua, const char* name)
{
    for (auto shape : {"stroke", "gradient", "width", "radius", "dash", "dash_offset"}) {
        if (std::strcmp(name, shape) == 0) luaL_error(lua, "spans hold only text, so %s is unsupported", name);
    }
}

// The parts an optional module adds to spans, by the key that marks them. Until the module
// gives them their maker, as in a build without it, such a part stops the script with the
// build option.
struct LuaModulePart
{
    const char* key;
    const char* module;
};

static constexpr LuaModulePart MODULE_PARTS[] = {
    {"tex", "tex"},
};

// Stops the script on a part of the table at index that a module would make, but which no
// module gave a maker.
static void _modulePart(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    for (auto& part : MODULE_PARTS) {
        lua_pushstring(lua, part.key);
        auto held = lua_rawget(lua, index) != LUA_TNIL;
        lua_pop(lua, 1);
        if (!held) continue;
        lua_getfield(lua, LUA_REGISTRYINDEX, SPAN_PART_REGISTRY);
        lua_pushstring(lua, part.key);
        auto made = lua_rawget(lua, -2) != LUA_TNIL;
        lua_pop(lua, 2);
        if (!made) {
            luaL_error(lua, "a %s part needs the %s module: build viscript with -Dmodules=%s", part.key, part.module,
                       part.module);
        }
    }
}

static void _rowOptions(lua_State* lua, int index, size_t count)
{
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) == LUA_TNUMBER) {
            auto key = lua_isinteger(lua, -2) ? lua_tointeger(lua, -2) : 0;
            if (key < 1 || static_cast<size_t>(key) > count) luaL_error(lua, "spans parts must form a list");
            auto type = lua_type(lua, -1);
            if (type != LUA_TSTRING && type != LUA_TTABLE) {
                luaL_error(lua, "a spans part must be a string or a table, got %s", luaL_typename(lua, -1));
            }
            if (type == LUA_TTABLE) _modulePart(lua, -1);
        } else if (lua_type(lua, -2) == LUA_TSTRING) {
            auto name = lua_tostring(lua, -2);
            _spanOption(lua, name);
            if (!_option(name,
                         {"point", "align", "size", "font", "role", "orientation", "split", "wrap", "leading",
                          "baseline"},
                         true)) {
                luaL_error(lua, "unknown option '%s'", name);
            }
        } else {
            luaL_error(lua, "option names must be strings");
        }
        lua_pop(lua, 1);
    }
}

// Pushes the string of a part table, given first or as text.
static void _partString(lua_State* lua, int part)
{
    lua_pushnil(lua);
    while (lua_next(lua, part)) {
        if (lua_type(lua, -2) == LUA_TNUMBER) {
            if (!lua_isinteger(lua, -2) || lua_tointeger(lua, -2) != 1) {
                luaL_error(lua, "a spans part holds one string, first or as text");
            }
        } else if (lua_type(lua, -2) != LUA_TSTRING) {
            luaL_error(lua, "option names must be strings");
        } else {
            auto name = lua_tostring(lua, -2);
            _spanOption(lua, name);
            if (!_option(name, {"text", "size", "font", "role", "rise"}, true)) {
                luaL_error(lua, "unknown option '%s'", name);
            }
        }
        lua_pop(lua, 1);
    }
    lua_rawgeti(lua, part, 1);
    lua_getfield(lua, part, "text");
    if (!lua_isnil(lua, -1) && !lua_isnil(lua, -2)) {
        luaL_error(lua, "a spans part takes its string first or as text, not both");
    }
    if (lua_isnil(lua, -1)) lua_pop(lua, 1);
    else lua_remove(lua, -2);
    if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "a spans part needs a string, first or as text");
}

// Pushes a field of the part, or of the row when the part leaves it out.
static int _spanField(lua_State* lua, int row, int part, const char* name)
{
    if (part) {
        lua_getfield(lua, part, name);
        if (!lua_isnil(lua, -1)) return lua_type(lua, -1);
        lua_pop(lua, 1);
    }
    lua_getfield(lua, row, name);
    return lua_type(lua, -1);
}

// Creates a detached text of a spans row with the size, font and role of its part, or else
// of the row, and the orientation of the row. Leaves the handle that owns it on the stack.
static Text* _spanText(lua_State* lua, LuaScene* box, int row, int part, const char* value,
                       size_t length, TextOrientation orientation)
{
    lua_pushlstring(lua, value, length);
    auto owner = _owner(lua, box);
    auto text = _adopt(owner, Text::gen(lua_tostring(lua, -2), {}));
    lua_remove(lua, -2);
    if (!text) luaL_error(lua, "could not create span: out of memory");
    text->orientation = orientation;
    if (_spanField(lua, row, part, "size") != LUA_TNIL) {
        text->size = _number(lua, -1, "size");
        if (text->size <= 0.0f) luaL_error(lua, "size must be positive");
    }
    lua_pop(lua, 1);
    if (_spanField(lua, row, part, "font") != LUA_TNIL) _textFont(lua, -1, text);
    lua_pop(lua, 1);
    if (_spanField(lua, row, part, "role") != LUA_TNIL) {
        text->role = _enum(lua, -1, "text role", TEXT_ROLES);
    }
    lua_pop(lua, 1);
    return text;
}

// A blank in the style of a row, block or grid: the advance of a space and the height of a line,
// and the role and size the options give its texts.
struct LuaBlank
{
    TextExtent extent;
    TextRole role = TextRole::Text;
    float size = 0.0f;
};

// Measures a blank in the style of the options at 2, in the code role when code is set, as the
// tokens of a code block take it.
static LuaBlank _blank(lua_State* lua, LuaScene* box, TextOrientation orientation, bool code = false)
{
    auto text = _spanText(lua, box, 2, 0, " ", 1, orientation);
    if (code) text->role = TextRole::Code;
    LuaBlank blank{_measured(lua, box->scene, text), text->role, text->size};
    lua_pop(lua, 1);
    return blank;
}

// The style table a span attaches with: the part's style options, and the row's color and
// draw layer where the part sets none of its own. A group's layer orders no drawings below it.
static void _spanStyle(lua_State* lua, int row, int part)
{
    lua_createtable(lua, 0, 4);
    auto style = lua_gettop(lua);
    auto colored = false;
    auto layered = false;
    if (part) {
        for (auto name : SPAN_STYLES) {
            lua_getfield(lua, part, name);
            if (!lua_isnil(lua, -1)) {
                if (std::strcmp(name, "color") == 0 || std::strcmp(name, "fill") == 0) colored = true;
                if (std::strcmp(name, "layer") == 0) layered = true;
            }
            lua_setfield(lua, style, name);
        }
    }
    if (!colored) {
        for (auto name : {"color", "fill"}) {
            lua_getfield(lua, row, name);
            lua_setfield(lua, style, name);
        }
    }
    if (!layered) {
        lua_getfield(lua, row, "layer");
        lua_setfield(lua, style, "layer");
    }
}

// Pushes the maker a module gave the parts that hold a key of the part table at index, such as
// tex, and returns that key; pushes nothing and returns nullptr for a part of text.
static const char* _spanMaker(lua_State* lua, int part)
{
    part = lua_absindex(lua, part);
    lua_getfield(lua, LUA_REGISTRYINDEX, SPAN_PART_REGISTRY);
    auto makers = lua_gettop(lua);
    auto found = 0;
    auto count = static_cast<lua_Integer>(lua_rawlen(lua, makers));
    for (lua_Integer i = 1; i <= count; i++) {
        lua_rawgeti(lua, makers, i);
        lua_pushvalue(lua, -1);
        auto held = lua_rawget(lua, part) != LUA_TNIL;
        lua_pop(lua, 1);
        if (!held) {
            lua_pop(lua, 1);
            continue;
        }
        if (found) {
            luaL_error(lua, "a spans part sets one box, so it cannot hold both %s and %s", lua_tostring(lua, found),
                       lua_tostring(lua, -1));
        }
        found = lua_gettop(lua);
    }
    if (!found) {
        lua_settop(lua, makers - 1);
        return nullptr;
    }
    // The registry keeps the key, so the string outlives its place on the stack.
    auto key = lua_tostring(lua, found);
    lua_pushvalue(lua, found);
    lua_rawget(lua, makers);
    lua_replace(lua, makers);
    lua_settop(lua, makers);
    return key;
}

// Pushes the setting a module part takes: the size, font and role of the part, or else of the
// row, and the row's orientation.
static void _spanSetting(lua_State* lua, int row, int part, TextOrientation orientation)
{
    lua_createtable(lua, 0, 4);
    for (auto name : {"size", "font", "role"}) {
        _spanField(lua, row, part, name);
        lua_setfield(lua, -2, name);
    }
    lua_pushstring(lua, orientation == TextOrientation::Plane ? "plane" : "billboard");
    lua_setfield(lua, -2, "orientation");
}

// Reads the box a module's maker measured into extent as one line: the width, ascent, descent,
// top and bottom above at, then place. Returns whether the width, ascent and descent are finite,
// neither the width nor the line is negative, and place is a function.
static bool _makerBox(lua_State* lua, int at, TextExtent& extent)
{
    auto number = [&](int offset) {
        auto index = at + offset;
        return lua_type(lua, index) == LUA_TNUMBER ? static_cast<float>(lua_tonumber(lua, index)) : NAN;
    };
    extent.width = number(1);
    extent.ascent = number(2);
    extent.descent = number(3);
    extent.top = number(4);
    extent.bottom = number(5);
    extent.height = extent.ascent + extent.descent;
    extent.right = extent.width;
    extent.lines = 1u;
    return std::isfinite(extent.width) && std::isfinite(extent.ascent) && std::isfinite(extent.descent)
           && !(extent.width < 0.0f) && !(extent.height < 0.0f) && lua_isfunction(lua, at + 6);
}

// Calls place(row, style, x, y, z), on top of the stack, which a module's maker returned to draw
// its box at at and attach it below the row whose handle is at index; what names the box in the
// message when it attaches other than one object.
static void _place(lua_State* lua, Group* row, int index, int style, const Vec3& at, const char* what)
{
    auto children = row->childCount();
    lua_pushvalue(lua, index);
    lua_pushvalue(lua, style);
    lua_pushnumber(lua, at.x);
    lua_pushnumber(lua, at.y);
    lua_pushnumber(lua, at.z);
    detail::luaCall(lua, 5, 1);
    if (row->childCount() != children + 1u) luaL_error(lua, "%s must attach one object below its row", what);
}

// Sets text with the left end of its baseline at at, in a box as wide as its advance and as tall
// as its line, so that bounds and layout see the line.
static void _setOnBaseline(Text* text, const TextExtent& extent, const Vec3& at)
{
    auto height = extent.ascent + extent.descent;
    text->point = at;
    text->align = {0.0f, height > 0.0f ? extent.ascent / height : 0.0f};
    text->box = {extent.width, height};
}

// Whether no line may break between the span at i and the one before it: a box holds on to
// the text or box beside it unless a line feed or a blank lies between them.
static bool _spanGlued(const LuaSpan* spans, uint32_t i)
{
    if (!i) return false;
    auto& before = spans[i - 1u];
    auto& span = spans[i];
    if (!before.box && !span.box) return false;
    return !span.breaks && !(span.gap > 0.0f) && !before.trail && !span.lead;
}

// scene:spans{...}: a group of texts set side by side on shared baselines, each measured in
// its font, so that every word or part is a handle of its own. A part a module makes, such as
// a formula, is a box the module draws on the baseline.
static int _spans(lua_State* lua)
{
    auto space = _space(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto count = lua_rawlen(lua, 2);
    _rowOptions(lua, 2, count);
    if (!count) return luaL_error(lua, "spans need at least one string or part");
    auto scene = space.scene->scene;
    auto point = _fieldWorld(lua, 2, "point", {});
    auto align = _fieldVec2(lua, 2, "align", {0.5f, 0.5f});
    auto wrap = _fieldNumber(lua, 2, "wrap", 0.0f);
    if (wrap < 0.0f) return luaL_error(lua, "wrap must be a non-negative width");
    // Without leading, each line takes the height its texts and boxes need, as before.
    auto leading = 0.0f;
    if (lua_getfield(lua, 2, "leading") != LUA_TNIL) {
        leading = _number(lua, -1, "leading");
        if (leading <= 0.0f) {
            return luaL_error(lua, "leading must be a positive multiple of the line height");
        }
    }
    lua_pop(lua, 1);
    // With baseline, the point lies on the first baseline, and align places the rows only
    // along it.
    auto pinned = _fieldBool(lua, 2, "baseline", false);
    static constexpr LuaName<LuaSplit> SPLITS[] = {
        {"none", LuaSplit::None}, {"words", LuaSplit::Words}, {"letters", LuaSplit::Letters}};
    auto split = _fieldEnum(lua, 2, "split", LuaSplit::None, SPLITS);
    auto orientation = _textOrientation(lua, 2);

    // The blank a space leaves, and the line a blank line takes, in the row's own style.
    auto blank = _blank(lua, space.scene, orientation).extent;

    uint32_t capacity = 16;
    auto spans = static_cast<LuaSpan*>(lua_newuserdatauv(lua, sizeof(LuaSpan) * capacity, 0));
    auto spanStore = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto handles = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto styles = lua_gettop(lua);
    uint32_t spanCount = 0;
    uint32_t breaks = 0;
    uint32_t feeds = 0;
    auto gap = 0.0f;
    auto rowId = _pushId(lua);
    auto next = [&]() -> LuaSpan& {
        if (spanCount == LUA_OBJECT_LIMIT) {
            luaL_error(lua, "spans would make more than %d texts", static_cast<int>(LUA_OBJECT_LIMIT));
        }
        spans = _grow(lua, spanStore, spans, spanCount, capacity);
        return *new (spans + spanCount) LuaSpan();
    };
    // Gives the span its style, which names it after the row unless it has an id of its own.
    auto style = [&](int part) {
        _spanStyle(lua, 2, part);
        lua_getfield(lua, -1, "id");
        auto named = !lua_isnil(lua, -1);
        lua_pop(lua, 1);
        if (!named) _childId(lua, -1, rowId, spanCount + 1);
        lua_rawseti(lua, styles, spanCount + 1);
    };
    // Counts the span with its rise and the blanks and line feeds before it.
    auto tally = [&](LuaSpan& span, int part) {
        if (part) span.rise = _fieldNumber(lua, part, "rise", 0.0f) * (blank.ascent + blank.descent);
        if (!std::isfinite(span.rise)) luaL_error(lua, "rise must be a finite fraction of the line height");
        span.gap = gap;
        span.breaks = breaks;
        feeds += breaks;
        gap = 0.0f;
        breaks = 0;
        spanCount++;
    };
    auto add = [&](int part, const char* value, size_t length) {
        auto& span = next();
        span.text = _spanText(lua, space.scene, 2, part, value, length, orientation);
        span.lead = value[0] == ' ' || value[0] == '\t';
        span.trail = value[length - 1u] == ' ' || value[length - 1u] == '\t';
        lua_rawseti(lua, handles, spanCount + 1);
        style(part);
        span.extent = _measured(lua, scene, span.text);
        tally(span, part);
    };
    // A box a module makes with the maker on top of the stack, which keeps what place needs.
    auto addBox = [&](int part, const char* key) {
        auto maker = lua_gettop(lua);
        auto& span = next();
        span.box = true;
        lua_pushvalue(lua, maker);
        lua_pushvalue(lua, 1);
        lua_pushvalue(lua, part);
        _spanSetting(lua, 2, part, orientation);
        detail::luaCall(lua, 3, 6);
        auto& extent = span.extent;
        // Spans also set lines apart by the ink, so it must lie from top down to bottom.
        if (!_makerBox(lua, maker, extent) || !std::isfinite(extent.top) || !std::isfinite(extent.bottom)
            || extent.bottom > extent.top) {
            luaL_error(lua, "the %s part of spans measured no box", key);
        }
        lua_rawseti(lua, handles, spanCount + 1);
        lua_settop(lua, maker);
        style(part);
        tally(span, part);
    };
    for (auto i = 1u; i <= count; i++) {
        lua_rawgeti(lua, 2, i);
        auto item = lua_gettop(lua);
        if (lua_istable(lua, item)) {
            auto key = _spanMaker(lua, item);
            if (key) {
                addBox(item, key);
                lua_settop(lua, item - 1);
                continue;
            }
            _partString(lua, item);
            size_t length;
            auto value = lua_tolstring(lua, -1, &length);
            if (std::strlen(value) != length) luaL_error(lua, "span text cannot contain zero bytes");
            if (!length) luaL_error(lua, "a spans part needs a non-empty string");
            add(item, value, length);
            lua_settop(lua, item - 1);
            continue;
        }
        size_t length;
        auto value = lua_tolstring(lua, item, &length);
        if (std::strlen(value) != length) luaL_error(lua, "span text cannot contain zero bytes");
        if (length > LUA_TEXT_LIMIT) luaL_error(lua, "text exceeds the 1 MiB limit");
        for (size_t at = 0; at < length;) {
            auto c = value[at];
            if (c == '\n') {
                breaks++;
                gap = 0.0f;
                at++;
                continue;
            }
            if (split != LuaSplit::None && (c == ' ' || c == '\t')) {
                gap += blank.width;
                at++;
                continue;
            }
            auto begin = at;
            if (split == LuaSplit::None) {
                while (at < length && value[at] != '\n') at++;
            } else if (split == LuaSplit::Words) {
                while (at < length && value[at] != '\n' && value[at] != ' ' && value[at] != '\t') at++;
            } else {
                auto lead = static_cast<uint8_t>(c);
                auto bytes = lead >= 0xf0u && lead < 0xf8u ? 4u : lead >= 0xe0u ? 3u : lead >= 0xc0u ? 2u : 1u;
                at += bytes;
                if (at > length) at = length;
            }
            add(0, value + begin, at - begin);
        }
        lua_settop(lua, item - 1);
    }
    if (!spanCount) return luaL_error(lua, "spans need at least one character that is not blank");

    // Lines: a line feed starts one, and so does a part that would pass the wrap width. A box
    // and the texts or boxes it holds on to break as one, before the first of them.
    auto lineCapacity = static_cast<size_t>(spanCount) + feeds;
    auto lines = static_cast<LuaSpanLine*>(lua_newuserdatauv(lua, sizeof(LuaSpanLine) * lineCapacity, 0));
    uint32_t lineCount = 1;
    new (lines) LuaSpanLine();
    auto x = 0.0f;
    auto empty = true;
    for (auto i = 0u; i < spanCount; i++) {
        auto& span = spans[i];
        auto feed = span.breaks;
        if (!feed && wrap > 0.0f && !empty && !_spanGlued(spans, i)) {
            auto run = span.extent.width;
            for (auto j = i + 1u; j < spanCount && _spanGlued(spans, j); j++) run += spans[j].extent.width;
            if (x + span.gap + run > wrap) feed = 1;
        }
        for (auto j = 0u; j < feed; j++) {
            // A line that a line feed ends before any text is a blank line.
            if (empty) {
                lines[lineCount - 1u].ascent = blank.ascent;
                lines[lineCount - 1u].descent = blank.descent;
            }
            new (lines + lineCount++) LuaSpanLine();
            x = 0.0f;
            empty = true;
        }
        if (!empty) x += span.gap;
        span.line = lineCount - 1u;
        span.x = x;
        x += span.extent.width;
        empty = false;
        auto& line = lines[span.line];
        line.width = x;
        line.ascent = std::max(line.ascent, span.extent.ascent + span.rise);
        line.descent = std::max(line.descent, span.extent.descent - span.rise);
        auto inkTop = span.extent.top + span.rise;
        auto inkBottom = span.extent.bottom + span.rise;
        if (span.box) {
            line.boxTop = line.boxes ? std::max(line.boxTop, inkTop) : inkTop;
            line.boxBottom = line.boxes ? std::min(line.boxBottom, inkBottom) : inkBottom;
            line.boxes = true;
        } else {
            line.textTop = line.texts ? std::max(line.textTop, inkTop) : inkTop;
            line.textBottom = line.texts ? std::min(line.textBottom, inkBottom) : inkBottom;
            line.texts = true;
        }
    }
    auto width = 0.0f;
    auto height = 0.0f;
    for (auto i = 0u; i < lineCount; i++) width = std::max(width, lines[i].width);
    if (leading > 0.0f) {
        // Baselines stand leading line heights apart, unless ink would collide: a line then
        // moves down just as far as keeps it clear, and the lines after it keep the pitch.
        auto lineHeight = blank.ascent + blank.descent;
        height = lines[0].ascent + lines[lineCount - 1u].descent;
        for (auto i = 1u; i < lineCount; i++) {
            lines[i].pitch = _spanPitch(lines[i - 1u], lines[i], leading * lineHeight, 0.1f * lineHeight);
            height += lines[i].pitch;
        }
    } else {
        for (auto i = 0u; i < lineCount; i++) height += lines[i].ascent + lines[i].descent;
    }
    auto left = point.x - align.x * width;
    auto top = point.y + align.y * height;
    auto baseline = top;
    auto line = UINT32_MAX;
    for (auto i = 0u; i < spanCount; i++) {
        auto& span = spans[i];
        while (line != span.line) {
            if (line == UINT32_MAX) {
                line = 0u;
                baseline = pinned ? point.y : top - lines[0].ascent;
            } else if (leading > 0.0f) {
                line++;
                baseline -= lines[line].pitch;
            } else {
                baseline -= lines[line].descent;
                line++;
                baseline -= lines[line].ascent;
            }
        }
        auto lineLeft = left + align.x * (width - lines[line].width);
        if (span.box) {
            span.at = {lineLeft + span.x, baseline + span.rise, point.z};
            continue;
        }
        _setOnBaseline(span.text, span.extent, {lineLeft + span.x, baseline + span.rise, point.z});
    }

    // The group takes the row's id and layer options; its texts and boxes follow it in order.
    auto group = _typeset(lua, space, "spans");
    auto result = lua_gettop(lua);
    LuaSpace inside{space.scene, group};
    for (auto i = 0u; i < spanCount; i++) {
        lua_rawgeti(lua, styles, i + 1);
        auto style = lua_gettop(lua);
        lua_rawgeti(lua, handles, i + 1);
        auto& span = spans[i];
        if (!span.box) {
            _add(lua, inside, static_cast<LuaObject*>(lua_touserdata(lua, -1)), span.text, style, "span");
            lua_settop(lua, result);
            continue;
        }
        _place(lua, group, result, style, span.at, "a spans box");
        lua_settop(lua, result);
    }
    return 1;
}

// ---------- Code blocks ----------

// The token classes as the colors option names them, in CodeClass order, and the Theme roles
// they take unless the script sets them, so that a block reads on every theme.
static constexpr const char* CODE_CLASSES[] = {
    "plain", "keyword", "string", "number", "comment", "function", "type", "operator"};
static constexpr const char* CODE_COLORS[] = {
    "foreground", "secondary", "success", "danger", "muted", "accent", "warning", "foreground"};
// The languages the language option names.
static constexpr LuaName<detail::CodeLanguage> CODE_LANGUAGES[] = {
    {"plain", detail::CodeLanguage::Plain}, {"lua", detail::CodeLanguage::Lua},
    {"python", detail::CodeLanguage::Python}, {"javascript", detail::CodeLanguage::JavaScript},
    {"typescript", detail::CodeLanguage::TypeScript}, {"c", detail::CodeLanguage::C},
    {"cpp", detail::CodeLanguage::Cpp}};

// One token of a code block, measured in world units before it attaches.
struct LuaCodeToken
{
    Text* text = nullptr;
    TextExtent extent;
    uint32_t line = 0;
    float x = 0.0f;
};

// Code holds only text, which takes a color but no stroke or outline.
static void _codeOptions(lua_State* lua)
{
    lua_pushnil(lua);
    while (lua_next(lua, 2)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "option names must be strings");
        auto name = lua_tostring(lua, -2);
        for (auto shape : {"stroke", "fill", "gradient", "width", "radius", "dash", "dash_offset"}) {
            if (std::strcmp(name, shape) == 0) {
                luaL_error(lua, "code holds only text, so %s is unsupported; color and colors set its colors", name);
            }
        }
        if (!_option(name, {"text", "language", "point", "align", "size", "font", "orientation", "leading",
                            "pitch", "baseline", "colors"}, true)) {
            luaL_error(lua, "unknown option '%s'", name);
        }
        lua_pop(lua, 1);
    }
}

// Pushes the colors of the token classes as a list in CodeClass order: the Theme roles, or the
// color option for every class, and then the classes the colors option names.
static void _codePalette(lua_State* lua, const Theme& theme)
{
    lua_createtable(lua, 8, 0);
    auto palette = lua_gettop(lua);
    lua_getfield(lua, 2, "color");
    auto colored = !lua_isnil(lua, -1);
    if (colored) _color(lua, -1, "color", &theme);
    for (auto i = 0u; i < 8u; i++) {
        if (colored) lua_pushvalue(lua, -1);
        else lua_pushstring(lua, CODE_COLORS[i]);
        lua_rawseti(lua, palette, i + 1);
    }
    lua_pop(lua, 1);
    lua_getfield(lua, 2, "colors");
    if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1)) luaL_error(lua, "colors must be a table of token classes and colors");
        lua_pushnil(lua);
        while (lua_next(lua, -2)) {
            auto at = 8u;
            if (lua_type(lua, -2) == LUA_TSTRING) {
                auto name = lua_tostring(lua, -2);
                for (auto i = 0u; i < 8u; i++) {
                    if (std::strcmp(name, CODE_CLASSES[i]) == 0) at = i;
                }
            }
            if (at == 8u) {
                luaL_error(lua, "colors may set only plain, keyword, string, number, comment, function, type and operator");
            }
            _color(lua, -1, CODE_CLASSES[at], &theme);
            lua_rawseti(lua, palette, at + 1);
        }
    }
    lua_pop(lua, 1);
}

// The advance of the blanks of a line from begin to end, in spaces: four for a tab.
static uint32_t _codeBlanks(const char* line, uint32_t begin, uint32_t end)
{
    auto spaces = 0u;
    for (auto i = begin; i < end; i++) {
        if (line[i] == ' ') spaces++;
        else if (line[i] == '\t') spaces += 4u;
    }
    return spaces;
}

// The options a block that edit makes takes from the block it replaces: its look and place, but
// not its text, its id, its mask, which masks one object, or how much of it shows.
static constexpr const char* CODE_KEPT[] = {
    "language", "point", "align", "size", "font", "orientation", "leading", "pitch", "baseline", "color", "colors",
    "layer", "isolate", "blend", "blur", "shadow"};

// Records the text and the kept options, at index 2, of the block code made at index.
static void _codeRemember(lua_State* lua, int block)
{
    _sceneTable(lua, CODE_REGISTRY, 1, true);
    lua_pushlightuserdata(lua, static_cast<LuaObject*>(lua_touserdata(lua, block))->object);
    lua_createtable(lua, 0, 2);
    lua_getfield(lua, 2, "text");
    lua_setfield(lua, -2, "text");
    _pickOptions(lua, CODE_KEPT);
    lua_setfield(lua, -2, "options");
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}

// scene:code{...}: source code set line by line in tokens colored by what they are. The block is
// a group of lines, each a group of its tokens, so that every line and token is a handle. It
// takes model as its transform: none, or that of the block an edit replaces.
static int _codeBlock(lua_State* lua, const Mat4& model)
{
    using namespace detail;
    auto space = _space(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    _codeOptions(lua);
    size_t length;
    auto source = _fieldString(lua, 2, "text", &length);
    if (length > LUA_TEXT_LIMIT) return luaL_error(lua, "text exceeds the 1 MiB limit");
    auto language = _fieldEnum(lua, 2, "language", CodeLanguage::Plain, CODE_LANGUAGES);
    auto scene = space.scene->scene;
    auto point = _fieldWorld(lua, 2, "point", {});
    auto align = _fieldVec2(lua, 2, "align", {0.5f, 0.5f});
    auto pinned = _fieldBool(lua, 2, "baseline", false);
    auto pitch = 0.0f;
    if (lua_getfield(lua, 2, "pitch") != LUA_TNIL) {
        pitch = _number(lua, -1, "pitch");
        if (pitch <= 0.0f) {
            return luaL_error(lua, "pitch must be a positive distance in world units");
        }
        if (lua_getfield(lua, 2, "leading") != LUA_TNIL) {
            return luaL_error(lua, "code takes pitch or leading, not both");
        }
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    auto leading = _fieldNumber(lua, 2, "leading", 1.0f);
    if (leading <= 0.0f) {
        return luaL_error(lua, "leading must be a positive multiple of the line height");
    }
    auto orientation = _textOrientation(lua, 2);
    _codePalette(lua, scene->theme());
    auto palette = lua_gettop(lua);

    // The advance of a space and the height of a line, in the block's own font.
    auto blank = _blank(lua, space.scene, orientation, true).extent;

    // A line feed at the very end closes the last line rather than opening another.
    if (length && source[length - 1u] == '\n') length--;
    auto longest = 0u;
    auto lineCount = 1u;
    for (size_t at = 0, begin = 0; at <= length; at++) {
        if (at < length && source[at] != '\n') continue;
        longest = std::max(longest, static_cast<uint32_t>(at - begin));
        if (at < length) lineCount++;
        begin = at + 1u;
    }
    if (lineCount > LUA_OBJECT_LIMIT) {
        return luaL_error(lua, "code would make more than %d lines", static_cast<int>(LUA_OBJECT_LIMIT));
    }
    // A token takes at least one byte, and the block holds fewer texts than the object limit.
    auto room = std::min(longest, LUA_OBJECT_LIMIT) + 1u;
    auto found = static_cast<CodeToken*>(lua_newuserdatauv(lua, sizeof(CodeToken) * room, 0));

    uint32_t capacity = 16;
    auto tokens = static_cast<LuaCodeToken*>(lua_newuserdatauv(lua, sizeof(LuaCodeToken) * capacity, 0));
    auto tokenStore = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto handles = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto styles = lua_gettop(lua);
    auto blockId = _pushId(lua);
    uint32_t tokenCount = 0;
    CodeState state;
    auto line = source;
    for (auto row = 0u; row < lineCount; row++) {
        auto end = static_cast<const char*>(std::memchr(line, '\n', static_cast<size_t>(source + length - line)));
        auto size = static_cast<uint32_t>((end ? end : source + length) - line);
        auto count = codeTokens(language, line, size, state, found, room);
        if (count > room || tokenCount + count > LUA_OBJECT_LIMIT) {
            return luaL_error(lua, "code would make more than %d texts", static_cast<int>(LUA_OBJECT_LIMIT));
        }
        auto x = 0.0f;
        auto after = 0u;
        for (auto i = 0u; i < count; i++) {
            auto& piece = found[i];
            tokens = _grow(lua, tokenStore, tokens, tokenCount, capacity);
            // A tab inside a comment or string widens to four spaces, as between tokens.
            lua_pushlstring(lua, line + piece.offset, piece.length);
            if (std::memchr(line + piece.offset, '\t', piece.length)) {
                luaL_gsub(lua, lua_tostring(lua, -1), "\t", "    ");
                lua_remove(lua, -2);
            }
            size_t textLength;
            auto value = lua_tolstring(lua, -1, &textLength);
            auto& token = *new (tokens + tokenCount) LuaCodeToken();
            token.text = _spanText(lua, space.scene, 2, 0, value, textLength, orientation);
            token.text->role = TextRole::Code;
            lua_rawseti(lua, handles, tokenCount + 1);
            lua_pop(lua, 1);
            lua_createtable(lua, 0, 3);
            lua_rawgeti(lua, palette, static_cast<int>(piece.kind) + 1);
            lua_setfield(lua, -2, "color");
            // A group's layer orders no drawings below it, so every token takes the block's.
            lua_getfield(lua, 2, "layer");
            lua_setfield(lua, -2, "layer");
            _childId(lua, -1, blockId, row + 1u, i + 1u);
            lua_rawseti(lua, styles, tokenCount + 1);
            token.extent = _measured(lua, scene, token.text);
            token.line = row;
            x += static_cast<float>(_codeBlanks(line, after, piece.offset)) * blank.width;
            token.x = x;
            x += token.extent.width;
            after = piece.offset + piece.length;
            tokenCount++;
        }
        line += size + 1u;
    }
    if (!tokenCount) return luaL_error(lua, "code needs at least one character that is not blank");

    // Lines stack at a fixed pitch, so a line's place depends on its number alone.
    if (pitch == 0.0f) pitch = (blank.ascent + blank.descent) * leading;
    auto width = 0.0f;
    for (auto i = 0u; i < tokenCount; i++) width = std::max(width, tokens[i].x + tokens[i].extent.width);
    auto height = static_cast<float>(lineCount - 1u) * pitch + blank.ascent + blank.descent;
    auto left = point.x - align.x * width;
    auto top = point.y + align.y * height;
    for (auto i = 0u; i < tokenCount; i++) {
        auto& token = tokens[i];
        auto baseline = (pinned ? point.y : top - blank.ascent) - static_cast<float>(token.line) * pitch;
        _setOnBaseline(token.text, token.extent, {left + token.x, baseline, point.z});
    }

    // The block takes the id and layer options; its lines follow it in order, blank ones too.
    auto block = _typeset(lua, space, "code", &model);
    auto result = lua_gettop(lua);
    LuaSpace inside{space.scene, block};
    auto next = 0u;
    for (auto row = 0u; row < lineCount; row++) {
        lua_createtable(lua, 0, 1);
        _childId(lua, -1, blockId, row + 1u);
        auto lineStyle = lua_gettop(lua);
        auto lineOwner = _owner(lua, space.scene);
        auto lineGroup = Group::gen();
        _add(lua, inside, lineOwner, lineGroup, lineStyle, "code line");
        auto lineTop = lua_gettop(lua);
        LuaSpace members{space.scene, lineGroup};
        for (; next < tokenCount && tokens[next].line == row; next++) {
            lua_rawgeti(lua, styles, next + 1);
            auto style = lua_gettop(lua);
            lua_rawgeti(lua, handles, next + 1);
            _add(lua, members, static_cast<LuaObject*>(lua_touserdata(lua, -1)), tokens[next].text, style, "code token");
            lua_settop(lua, lineTop);
        }
        lua_settop(lua, result);
    }
    _codeRemember(lua, result);
    return 1;
}

static int _code(lua_State* lua)
{
    return _codeBlock(lua, Mat4::identity());
}

// code{...} as edit calls it: the block takes the transform its upvalue points to.
static int _codeMoved(lua_State* lua)
{
    return _codeBlock(lua, *static_cast<const Mat4*>(lua_touserdata(lua, lua_upvalueindex(1))));
}

// Pushes the source of the code block whose handle is at index, the {text, options} that code
// recorded, or stops the script.
static void _codeSource(lua_State* lua, int index, const char* method)
{
    auto handle = _handle(lua, index);
    _sceneTable(lua, CODE_REGISTRY, index, false);
    if (!lua_isnil(lua, -1)) {
        lua_pushlightuserdata(lua, handle->object);
        lua_rawget(lua, -2);
        lua_remove(lua, -2);
    }
    if (!lua_istable(lua, -1)) luaL_error(lua, "%s needs a block that code made", method);
}

// The language of the code block whose source is at index.
static detail::CodeLanguage _codeLanguageOf(lua_State* lua, int index)
{
    lua_getfield(lua, index, "options");
    auto language = _fieldEnum(lua, -1, "language", detail::CodeLanguage::Plain, CODE_LANGUAGES);
    lua_pop(lua, 1);
    return language;
}

// A token of a code block's text: its bytes from begin to end, and its line and its place in that
// line, counting from 0.
struct LuaCodeSpan
{
    uint32_t begin = 0;
    uint32_t end = 0;
    uint32_t row = 0;
    uint32_t index = 0;
};

// Pushes the tokens of text as code splits it, in order, and returns them and their count.
static LuaCodeSpan* _codeSpans(lua_State* lua, detail::CodeLanguage language, const char* text, size_t length,
                               uint32_t& count)
{
    using namespace detail;
    if (length && text[length - 1u] == '\n') length--;
    // The first pass counts the tokens, and the most one line holds; the second records them.
    count = 0;
    auto most = 0u;
    CodeState state;
    for (size_t begin = 0;;) {
        auto end = begin;
        while (end < length && text[end] != '\n') end++;
        auto found = codeTokens(language, text + begin, static_cast<uint32_t>(end - begin), state, nullptr, 0u);
        count += found;
        most = std::max(most, found);
        if (end >= length) break;
        begin = end + 1u;
    }
    auto spans = static_cast<LuaCodeSpan*>(lua_newuserdatauv(lua, sizeof(LuaCodeSpan) * (count ? count : 1u), 0));
    auto tokens = static_cast<CodeToken*>(lua_newuserdatauv(lua, sizeof(CodeToken) * (most ? most : 1u), 0));
    state = CodeState{};
    auto filled = 0u;
    for (size_t begin = 0, row = 0;; row++) {
        auto end = begin;
        while (end < length && text[end] != '\n') end++;
        auto found = codeTokens(language, text + begin, static_cast<uint32_t>(end - begin), state, tokens, most);
        for (auto i = 0u; i < found; i++) {
            auto& span = spans[filled++];
            span.begin = static_cast<uint32_t>(begin) + tokens[i].offset;
            span.end = span.begin + tokens[i].length;
            span.row = static_cast<uint32_t>(row);
            span.index = i;
        }
        if (end >= length) break;
        begin = end + 1u;
    }
    lua_pop(lua, 1);
    return spans;
}

// The first of the spans that ends after at, or count.
static uint32_t _codeSpanAfter(const LuaCodeSpan* spans, uint32_t count, size_t at)
{
    uint32_t low = 0;
    auto high = count;
    while (low < high) {
        auto middle = low + (high - low) / 2u;
        if (spans[middle].end <= at) low = middle + 1u;
        else high = middle;
    }
    return low;
}

// Whether at falls between the tokens rather than inside one.
static bool _codeEdge(const LuaCodeSpan* spans, uint32_t count, size_t at)
{
    auto i = _codeSpanAfter(spans, count, at);
    return i == count || spans[i].begin >= at;
}

// What a search or an edit looks for: a text, which of its matches to take, the lines to look
// in, and whether a match may cut a token.
struct LuaCodeQuery
{
    const char* text = nullptr;
    size_t length = 0;
    // The match to take, counting from 1; 0 takes every match and UINT32_MAX the last.
    uint32_t pick = 1;
    // The lines to look in, counting from 1, or 0 for every line.
    uint32_t first = 0;
    uint32_t last = 0;
    bool whole = false;
};

// Reads a line number or {first, last} at index, counting from 1, or stops the script with
// message.
static void _codeLines(lua_State* lua, int index, uint32_t& first, uint32_t& last, const char* message)
{
    index = lua_absindex(lua, index);
    auto number = [&](int at) {
        int whole;
        auto value = lua_tointegerx(lua, at, &whole);
        if (lua_type(lua, at) != LUA_TNUMBER || !whole || value < 1) luaL_error(lua, "%s", message);
        return value > UINT32_MAX - 1 ? UINT32_MAX - 1 : static_cast<uint32_t>(value);
    };
    if (lua_type(lua, index) == LUA_TNUMBER) {
        first = last = number(index);
        return;
    }
    if (!lua_istable(lua, index) || lua_rawlen(lua, index) != 2u) luaL_error(lua, "%s", message);
    lua_rawgeti(lua, index, 1);
    lua_rawgeti(lua, index, 2);
    first = number(-2);
    last = number(-1);
    lua_pop(lua, 2);
    if (first > last) luaL_error(lua, "%s", message);
}

// Reads the match, lines, and whole options of the table at index into query.
static void _codeQuery(lua_State* lua, int index, LuaCodeQuery& query)
{
    lua_getfield(lua, index, "match");
    auto type = lua_type(lua, -1);
    auto valid = type == LUA_TNIL;
    if (type == LUA_TSTRING) {
        auto name = lua_tostring(lua, -1);
        valid = true;
        if (std::strcmp(name, "first") == 0) query.pick = 1u;
        else if (std::strcmp(name, "last") == 0) query.pick = UINT32_MAX;
        else if (std::strcmp(name, "all") == 0) query.pick = 0u;
        else valid = false;
    } else if (type == LUA_TNUMBER) {
        int whole;
        auto value = lua_tointegerx(lua, -1, &whole);
        valid = whole && value >= 1;
        query.pick = value > UINT32_MAX - 1 ? UINT32_MAX - 1 : static_cast<uint32_t>(value);
    }
    if (!valid) luaL_error(lua, "match must be 'first', 'last', 'all' or a match number from 1");
    lua_pop(lua, 1);
    lua_getfield(lua, index, "lines");
    if (!lua_isnil(lua, -1)) {
        _codeLines(lua, -1, query.first, query.last, "lines takes a line number or {first, last}, counting from 1");
    }
    lua_pop(lua, 1);
    query.whole = _fieldBool(lua, index, "whole", false);
}

// The lines of text as code counts them: a line feed at the very end opens none.
static uint32_t _codeLineCount(const char* text, size_t length)
{
    if (length && text[length - 1u] == '\n') length--;
    auto count = 1u;
    for (size_t i = 0; i < length; i++) count += text[i] == '\n';
    return count;
}

// Where line number of text starts, counting from 1; past the last line, the end of the text.
static size_t _codeLineStart(const char* text, size_t length, uint32_t number)
{
    size_t at = 0;
    for (auto line = 1u; line < number; line++) {
        auto end = static_cast<const char*>(std::memchr(text + at, '\n', length - at));
        if (!end) return length;
        at = static_cast<size_t>(end - text) + 1u;
    }
    return at;
}

// Pushes the table of the longest part of the query's text that ends each of its prefixes and
// starts it too, so that a search reads every byte of the code once.
static uint32_t* _codeFailures(lua_State* lua, const LuaCodeQuery& query)
{
    auto failures = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * query.length, 0));
    failures[0] = 0;
    for (size_t i = 1, k = 0; i < query.length; i++) {
        while (k && query.text[i] != query.text[k]) k = failures[k - 1u];
        if (query.text[i] == query.text[k]) k++;
        failures[i] = static_cast<uint32_t>(k);
    }
    return failures;
}

// Calls visit(begin) for each match of query in text that it picks, in order; matches do not
// overlap, and when spans are given, each is one that cuts none of them. Stops the script,
// before any visit, when the lines hold no match or fewer than the one picked; prefix starts
// those messages.
template <typename Visit>
static void _codeFind(lua_State* lua, const char* text, size_t length, const LuaCodeQuery& query,
                      const uint32_t* failures, const LuaCodeSpan* spans, uint32_t spanCount, const char* prefix,
                      Visit visit)
{
    auto begin = size_t{0};
    auto end = length;
    if (query.first) {
        auto lines = _codeLineCount(text, length);
        if (query.last > lines) {
            luaL_error(lua, "%sline %d is past the last of the code's %d lines", prefix, static_cast<int>(query.last),
                       static_cast<int>(lines));
        }
        begin = _codeLineStart(text, length, query.first);
        end = _codeLineStart(text, length, query.last + 1u);
    }
    auto count = 0u;
    auto latest = begin;
    auto state = size_t{0};
    for (auto at = begin; at < end;) {
        auto c = text[at++];
        while (state && query.text[state] != c) state = failures[state - 1u];
        if (query.text[state] == c) state++;
        if (state < query.length) continue;
        auto found = at - query.length;
        if (spans && (!_codeEdge(spans, spanCount, found) || !_codeEdge(spans, spanCount, at))) {
            state = failures[state - 1u];
            continue;
        }
        state = 0;
        count++;
        latest = found;
        if (!query.pick || count == query.pick) visit(found);
        if (count == query.pick) break;
    }
    if (!count) {
        auto whole = spans ? " made of whole tokens" : "";
        if (!query.first) luaL_error(lua, "%sthe code has no '%s'%s", prefix, query.text, whole);
        if (query.first == query.last) {
            luaL_error(lua, "%sline %d of the code has no '%s'%s", prefix, static_cast<int>(query.first), query.text,
                       whole);
        }
        luaL_error(lua, "%slines %d to %d of the code have no '%s'%s", prefix, static_cast<int>(query.first),
                   static_cast<int>(query.last), query.text, whole);
    }
    if (query.pick == UINT32_MAX) visit(latest);
    else if (count < query.pick) {
        luaL_error(lua, "%smatch %d is past the last of the %d matches of '%s'", prefix, static_cast<int>(query.pick),
                   static_cast<int>(count), query.text);
    }
}

// block:search(text, {match = "first", lines = n or {first, last}, whole = false}): the tokens
// of a code block that the matches of text cover any part of, in order. A match that covers
// part of a token takes the whole token, the smallest piece a block draws.
static int _search(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    LuaCodeQuery query;
    query.text = luaL_checklstring(lua, 2, &query.length);
    if (!query.length) return luaL_error(lua, "search needs a text to find");
    if (_optionTable(lua, 3, {"match", "lines", "whole"})) _codeQuery(lua, 3, query);
    _codeSource(lua, 1, "search");
    auto source = lua_gettop(lua);
    auto language = _codeLanguageOf(lua, source);
    lua_getfield(lua, source, "text");
    size_t length;
    auto text = lua_tolstring(lua, -1, &length);
    uint32_t spanCount;
    auto spans = _codeSpans(lua, language, text, length, spanCount);
    auto failures = _codeFailures(lua, query);
    auto marks = static_cast<bool*>(lua_newuserdatauv(lua, sizeof(bool) * (spanCount ? spanCount : 1u), 0));
    for (auto i = 0u; i < spanCount; i++) marks[i] = false;
    _codeFind(lua, text, length, query, failures, query.whole ? spans : nullptr, spanCount, "", [&](size_t begin) {
        auto end = begin + query.length;
        for (auto i = _codeSpanAfter(spans, spanCount, begin); i < spanCount && spans[i].begin < end; i++) {
            marks[i] = true;
        }
    });

    // The lines of the block are its rows, and the tokens of a line come first in it.
    auto block = handle->object;
    lua_getiuservalue(lua, 1, 1);
    auto sceneIndex = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto list = lua_gettop(lua);
    lua_Integer count = 0;
    Object* line = nullptr;
    auto at = UINT32_MAX;
    auto rows = 0u;
    auto next = 0u;
    for (auto i = 0u; i < spanCount; i++) {
        if (!marks[i]) continue;
        auto& span = spans[i];
        while (at != span.row && next < block->childCount()) {
            auto child = block->childAt(next++);
            if (child->type() != Type::Group) continue;
            if (rows++ == span.row) {
                line = child;
                at = span.row;
            }
        }
        if (at != span.row || span.index >= line->childCount()) {
            return luaL_error(lua, "the code block no longer holds the tokens code made");
        }
        _knownHandle(lua, handle->scene, sceneIndex, line->childAt(span.index));
        lua_rawseti(lua, list, ++count);
    }
    if (!count) return luaL_error(lua, "'%s' matches only blanks, which no token holds", query.text);
    return 1;
}

// ---------- Matrices, tables, groups and spaces ----------

// How an entry of a matrix sits across its column.
enum struct LuaJustify : uint8_t
{
    Left,
    Center,
    Right
};

// One entry of a matrix, measured in world units before it attaches.
struct LuaGridEntry
{
    Text* text = nullptr;
    TextExtent extent;
    // A box a module made in place of the text, such as a tex formula: the width reserve keeps
    // for it, and where its baseline starts once placed.
    bool box = false;
    float reserve = 0.0f;
    Vec3 at{};
};

// The most entries one matrix holds.
static constexpr uint32_t LUA_GRID_LIMIT = 1024;

// Counts the rows of entries in the list at index: rows of at least one entry, all as long.
static void _gridShape(lua_State* lua, int index, const char* name, uint32_t& rows, uint32_t& columns)
{
    index = lua_absindex(lua, index);
    auto count = lua_istable(lua, index) ? lua_rawlen(lua, index) : 0u;
    if (!count) luaL_error(lua, "%s rows must be a list of rows, each a list of entries", name);
    if (count > LUA_GRID_LIMIT) {
        luaL_error(lua, "a %s holds at most %d entries", name, static_cast<int>(LUA_GRID_LIMIT));
    }
    rows = static_cast<uint32_t>(count);
    columns = 0;
    for (auto r = 1u; r <= rows; r++) {
        lua_rawgeti(lua, index, r);
        auto length = lua_istable(lua, -1) ? lua_rawlen(lua, -1) : 0u;
        if (!length) luaL_error(lua, "%s row %d must be a list of at least one entry", name, static_cast<int>(r));
        auto held = static_cast<uint32_t>(std::min<lua_Unsigned>(length, UINT32_MAX));
        if (r == 1u) columns = held;
        else if (held != columns) {
            luaL_error(lua, "%s rows must all hold as many entries as the first, %d; row %d holds %d", name,
                       static_cast<int>(std::min(columns, LUA_GRID_LIMIT + 1u)), static_cast<int>(r),
                       static_cast<int>(std::min(held, LUA_GRID_LIMIT + 1u)));
        }
        lua_pop(lua, 1);
    }
    if (static_cast<uint64_t>(rows) * columns > LUA_GRID_LIMIT) {
        luaL_error(lua, "a %s holds at most %d entries", name, static_cast<int>(LUA_GRID_LIMIT));
    }
}

// Reads justify: one of 'left', 'center' and 'right' for every column, or a list of one for
// each column.
static void _justify(lua_State* lua, int index, const char* name, LuaJustify* output, uint32_t columns)
{
    auto read = [&](int at) {
        auto value = lua_type(lua, at) == LUA_TSTRING ? lua_tostring(lua, at) : "";
        if (std::strcmp(value, "left") == 0) return LuaJustify::Left;
        if (std::strcmp(value, "center") == 0) return LuaJustify::Center;
        if (std::strcmp(value, "right") == 0) return LuaJustify::Right;
        luaL_error(lua, "%s justify must be 'left', 'center' or 'right', or a list of one for each of its %d columns",
                   name, static_cast<int>(columns));
        return LuaJustify::Center;
    };
    lua_getfield(lua, index, "justify");
    if (lua_istable(lua, -1)) {
        if (lua_rawlen(lua, -1) != columns) read(0);
        for (auto c = 0u; c < columns; c++) {
            lua_rawgeti(lua, -1, c + 1u);
            output[c] = read(-1);
            lua_pop(lua, 1);
        }
    } else {
        auto justify = lua_isnil(lua, -1) ? LuaJustify::Center : read(-1);
        for (auto c = 0u; c < columns; c++) output[c] = justify;
    }
    lua_pop(lua, 1);
}

// The fewest digits after the point, at most 6, that write value exactly.
static uint8_t _fewestDecimals(double value)
{
    for (uint8_t digits = 0; digits < 6u; digits++) {
        if (std::fabs(value - std::round(value)) <= 1.0e-9 * std::fmax(1.0, std::fabs(value))) return digits;
        value *= 10.0;
    }
    return 6u;
}

// Creates the detached text of one entry, the value at the top of the stack, in the style of
// the options at index: a number counts, written with decimals or else with the fewest
// digits after the point that show it, and a string is set as it is. Replaces the value with
// the handle that owns the text.
static Text* _gridText(lua_State* lua, LuaScene* box, int index, const char* name, int decimals,
                       TextOrientation orientation)
{
    auto type = lua_type(lua, -1);
    if (type == LUA_TNUMBER) {
        auto value = lua_tonumber(lua, -1);
        if (!std::isfinite(value) || std::fabs(value) > FLT_MAX) {
            luaL_error(lua, "%s entries must be finite numbers", name);
        }
        NumberFormat format;
        format.decimals = decimals < 0 ? _fewestDecimals(value) : static_cast<uint8_t>(decimals);
        lua_pop(lua, 1);
        auto text = _spanText(lua, box, index, 0, "", 0, orientation);
        if (text->count(static_cast<float>(value), format) != Result::Success) {
            luaL_error(lua, "could not create %s entry: out of memory", name);
        }
        return text;
    }
    if (type != LUA_TSTRING) {
        luaL_error(lua, "a %s entry must be a number or a string, got %s", name, luaL_typename(lua, -1));
    }
    size_t length;
    auto value = lua_tolstring(lua, -1, &length);
    if (std::strlen(value) != length) luaL_error(lua, "%s entries cannot contain zero bytes", name);
    if (length > LUA_TEXT_LIMIT) luaL_error(lua, "text exceeds the 1 MiB limit");
    auto text = _spanText(lua, box, index, 0, value, length, orientation);
    lua_remove(lua, -2);
    return text;
}

// Places an entry in its cell, the box from left across by width and from top down by
// height: the glyphs sit ascent below the top of the cell's inside, on the row's baseline,
// and across it by justify. The text's box is the whole cell, so bounds, layout and around
// see the cell; one align pins both the glyphs and the box to the point.
static void _gridPlace(LuaGridEntry& entry, float left, float top, float width, float height,
                       float ascent, const Vec2& padding, LuaJustify justify, float z)
{
    auto& extent = entry.extent;
    auto glyphHeight = extent.ascent + extent.descent;
    auto share = justify == LuaJustify::Left ? 0.0f : justify == LuaJustify::Right ? 1.0f : 0.5f;
    auto glyphLeft = left + padding.x + share * (width - 2.0f * padding.x - extent.width);
    auto glyphTop = top - padding.y - ascent + extent.ascent;
    if (entry.box) {
        entry.at = {glyphLeft, top - padding.y - ascent, z};
        return;
    }
    auto spareX = width - extent.width;
    auto spareY = height - glyphHeight;
    auto alignX = spareX > 1.0e-5f * width ? std::clamp((glyphLeft - left) / spareX, 0.0f, 1.0f) : 0.5f;
    auto alignY = glyphHeight > 0.0f ? extent.ascent / glyphHeight : 0.0f;
    if (spareY > 1.0e-5f * height) alignY = std::clamp((top - glyphTop) / spareY, 0.0f, 1.0f);
    entry.text->align = {alignX, alignY};
    entry.text->point = {left + alignX * width, top - alignY * height, z};
    entry.text->box = {width, height};
}

// The decimals option of the numbers of a grid, or -1 when each takes the fewest it needs.
static int _gridDecimals(lua_State* lua, int index)
{
    if (!_has(lua, index, "decimals")) return -1;
    auto decimals = _fieldNumber(lua, index, "decimals", 0.0f);
    if (decimals < 0.0f || decimals > 6.0f || decimals != std::floor(decimals)) {
        luaL_error(lua, "decimals must be a whole number from 0 to 6");
    }
    return static_cast<int>(decimals);
}

// The spacing the field name of the options at 2 sets on both axes, one number for both or
// {x, y} of zero or more, or fallback without it.
static Vec2 _fieldSpacing(lua_State* lua, const char* name, Vec2 fallback)
{
    lua_getfield(lua, 2, name);
    if (lua_type(lua, -1) == LUA_TNUMBER) fallback.x = fallback.y = static_cast<float>(lua_tonumber(lua, -1));
    else if (!lua_isnil(lua, -1)) fallback = _vec2(lua, -1, name);
    lua_pop(lua, 1);
    if (!_finite(fallback) || fallback.x < 0.0f || fallback.y < 0.0f) {
        luaL_error(lua, "%s must be a number or {x, y} of zero or more", name);
    }
    return fallback;
}

// The theme's style of a text role.
static const TextTheme& _roleTheme(const Theme& theme, TextRole role)
{
    switch (role) {
        case TextRole::H1: return theme.h1;
        case TextRole::H2: return theme.h2;
        case TextRole::H3: return theme.h3;
        case TextRole::Code: return theme.code;
        default: return theme.text;
    }
}

// The brackets around a matrix.
enum struct LuaBrackets : uint8_t
{
    Square,
    Round,
    Bar,
    None
};

// scene:matrix{rows = {{...}, ...}}: entries in rows and columns between brackets, as a group
// whose children are its rows, each a group of the texts of its entries, and then its
// brackets. A number entry counts as scene:number does. With the tex module a string entry is
// set as inline math, as a cell of a TeX matrix is; without it, as text.
static int _matrixGen(lua_State* lua)
{
    auto space = _factory(lua, {"rows", "point", "align", "brackets", "gap", "justify", "decimals", "size", "font",
                                "role", "reserve", "tabular", "orientation", "id", "color", "fill", "stroke", "width",
                                "opacity", "progress", "layer", "isolate", "mask", "blend", "blur", "shadow"}, false);
    auto scene = space.scene->scene;
    lua_getfield(lua, 2, "rows");
    auto list = lua_gettop(lua);
    uint32_t rows;
    uint32_t columns;
    _gridShape(lua, list, "matrix", rows, columns);
    auto point = _fieldWorld(lua, 2, "point", {});
    auto align = _fieldVec2(lua, 2, "align", {0.5f, 0.5f});
    static constexpr LuaName<LuaBrackets> BRACKETS[] = {{"square", LuaBrackets::Square}, {"round", LuaBrackets::Round},
                                                        {"bar", LuaBrackets::Bar}, {"none", LuaBrackets::None}};
    auto brackets = _fieldEnum(lua, 2, "brackets", LuaBrackets::Square, BRACKETS);
    auto decimals = _gridDecimals(lua, 2);
    auto orientation = _textOrientation(lua, 2);
    auto justify = static_cast<LuaJustify*>(lua_newuserdatauv(lua, sizeof(LuaJustify) * columns, 0));
    _justify(lua, 2, "matrix", justify, columns);

    // The line a blank takes in the matrix's own style sets the spacing and the brackets.
    auto measured = _blank(lua, space.scene, orientation);
    auto& blank = measured.extent;
    auto role = measured.role;
    auto pixels = measured.size > 0.0f ? measured.size : _roleTheme(scene->theme(), role).size;
    auto line = blank.ascent + blank.descent;
    auto gap = _fieldSpacing(lua, "gap", {line, 0.5f * line});

    auto count = rows * columns;
    auto entries = static_cast<LuaGridEntry*>(lua_newuserdatauv(lua, sizeof(LuaGridEntry) * count, 0));
    lua_createtable(lua, static_cast<int>(count), 0);
    auto handles = lua_gettop(lua);
    lua_createtable(lua, static_cast<int>(count), 0);
    auto styles = lua_gettop(lua);
    auto matrixId = _pushId(lua);
    // The maker the tex module gave spans, which sets a string entry as a formula, or nil.
    lua_getfield(lua, LUA_REGISTRYINDEX, SPAN_PART_REGISTRY);
    lua_getfield(lua, -1, "tex");
    lua_remove(lua, -2);
    auto texMaker = lua_isfunction(lua, -1) ? lua_gettop(lua) : 0;
    for (auto r = 0u; r < rows; r++) {
        for (auto c = 0u; c < columns; c++) {
            auto i = r * columns + c;
            lua_rawgeti(lua, list, r + 1u);
            lua_rawgeti(lua, -1, c + 1u);
            auto& entry = *new (entries + i) LuaGridEntry();
            size_t length = 0;
            auto formula = texMaker && lua_type(lua, -1) == LUA_TSTRING && lua_tolstring(lua, -1, &length)
                        && std::strspn(lua_tostring(lua, -1), " \t") < length;
            if (formula) {
                // maker(handle, {tex = entry}, setting) measures the formula and returns its box
                // and place(row, style, x, y, z), which handles keeps until the rows attach.
                auto top = lua_gettop(lua);
                lua_pushvalue(lua, texMaker);
                lua_pushvalue(lua, 1);
                lua_createtable(lua, 0, 1);
                lua_pushvalue(lua, top);
                lua_setfield(lua, -2, "tex");
                _spanSetting(lua, 2, 0, orientation);
                detail::luaCall(lua, 3, 6);
                if (!_makerBox(lua, top, entry.extent)) luaL_error(lua, "a tex entry of matrix measured no box");
                entry.box = true;
                lua_replace(lua, top);
                lua_settop(lua, top);
            } else {
                entry.text = _gridText(lua, space.scene, 2, "matrix", decimals, orientation);
            }
            lua_rawseti(lua, handles, i + 1u);
            lua_pop(lua, 1);
            _spanStyle(lua, 2, 0);
            _childId(lua, -1, matrixId, r + 1u, c + 1u);
            lua_rawseti(lua, styles, i + 1u);
            if (entry.text) {
                entry.text->tabular = _fieldBool(lua, 2, "tabular", false);
                entry.extent = _measured(lua, scene, entry.text);
            }
            lua_getfield(lua, 2, "reserve");
            if (lua_istable(lua, -1)) {
                if (lua_rawlen(lua, -1) != columns) luaL_error(lua, "matrix reserve needs one string per column");
                lua_rawgeti(lua, -1, c + 1u);
                lua_remove(lua, -2);
                if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "matrix reserve needs one string per column");
            }
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "matrix reserve must be a string or a list of strings");
                size_t length;
                auto sample = lua_tolstring(lua, -1, &length);
                if (std::strlen(sample) != length) luaL_error(lua, "reserve cannot contain zero bytes");
                if (length > LUA_TEXT_LIMIT) luaL_error(lua, "reserve exceeds the 1 MiB limit");
                // A formula reserves the width of the sample as text in the matrix's style.
                auto sampler = entry.text ? entry.text : _spanText(lua, space.scene, 2, 0, "", 0, orientation);
                TextExtent reserved;
                auto result = scene->measure(sampler, sample, reserved);
                _check(lua, scene, result, "could not reserve text");
                if (entry.text) entry.text->box.x = reserved.width;
                else {
                    entry.reserve = reserved.width;
                    lua_pop(lua, 1);
                }
            }
            lua_pop(lua, 1);
        }
    }

    // A column is as wide as its widest entry, and a row holds the tallest ascent and descent
    // of its entries, so that they share one baseline.
    auto widths = static_cast<float*>(lua_newuserdatauv(lua, sizeof(float) * (columns + 2u * rows), 0));
    auto ascents = widths + columns;
    auto descents = ascents + rows;
    auto contentWidth = gap.x * static_cast<float>(columns - 1u);
    auto contentHeight = gap.y * static_cast<float>(rows - 1u);
    for (auto c = 0u; c < columns; c++) {
        widths[c] = 0.0f;
        for (auto r = 0u; r < rows; r++) {
            auto& entry = entries[r * columns + c];
            widths[c] = std::max(widths[c], std::max(entry.extent.width, entry.text ? entry.text->box.x : entry.reserve));
        }
        contentWidth += widths[c];
    }
    for (auto r = 0u; r < rows; r++) {
        ascents[r] = blank.ascent;
        descents[r] = blank.descent;
        for (auto c = 0u; c < columns; c++) {
            ascents[r] = std::max(ascents[r], entries[r * columns + c].extent.ascent);
            descents[r] = std::max(descents[r], entries[r * columns + c].extent.descent);
        }
        contentHeight += ascents[r] + descents[r];
    }
    // A bracket stands clear of the entries, reaches a little past them above and below, and
    // turns toward them: a square bracket by its serifs, a round one by its bulge.
    auto clearance = brackets == LuaBrackets::None ? 0.0f : 0.3f * line;
    auto overhang = brackets == LuaBrackets::None ? 0.0f : 0.15f * line;
    auto reach = 0.0f;
    if (brackets == LuaBrackets::Square) reach = 0.3f * line;
    else if (brackets == LuaBrackets::Round) reach = std::max(0.25f * line, 0.08f * (contentHeight + 2.0f * overhang));
    auto side = clearance + reach;
    auto width = contentWidth + 2.0f * side;
    auto height = contentHeight + 2.0f * overhang;
    auto left = point.x - align.x * width + side;
    auto top = point.y + align.y * height - overhang;
    auto y = top;
    for (auto r = 0u; r < rows; r++) {
        auto x = left;
        for (auto c = 0u; c < columns; c++) {
            _gridPlace(entries[r * columns + c], x, y, widths[c], ascents[r] + descents[r], ascents[r], {}, justify[c],
                       point.z);
            x += widths[c] + gap.x;
        }
        y -= ascents[r] + descents[r] + gap.y;
    }

    // The group takes the id and layer options; its rows follow it in order, then the brackets.
    auto group = _typeset(lua, space, "matrix");
    auto result = lua_gettop(lua);
    LuaSpace inside{space.scene, group};
    for (auto r = 0u; r < rows; r++) {
        lua_createtable(lua, 0, 1);
        _childId(lua, -1, matrixId, r + 1u);
        auto rowStyle = lua_gettop(lua);
        auto rowGroup = Group::gen();
        _add(lua, inside, _owner(lua, space.scene), rowGroup, rowStyle, "matrix row");
        auto rowTop = lua_gettop(lua);
        LuaSpace members{space.scene, rowGroup};
        for (auto c = 0u; c < columns; c++) {
            auto i = r * columns + c;
            lua_rawgeti(lua, styles, i + 1u);
            auto style = lua_gettop(lua);
            lua_rawgeti(lua, handles, i + 1u);
            auto& entry = entries[i];
            if (entry.box) {
                _place(lua, rowGroup, rowTop, style, entry.at, "a tex entry of matrix");
            } else {
                _add(lua, members, static_cast<LuaObject*>(lua_touserdata(lua, -1)), entry.text, style, "matrix entry");
            }
            lua_settop(lua, rowTop);
        }
        lua_settop(lua, result);
    }
    if (brackets == LuaBrackets::None) return 1;
    // The brackets take the color of the entries' role unless the matrix sets color or stroke,
    // and a stroke about a twelfth of the text's size.
    auto bracketColor = _roleTheme(scene->theme(), role).color;
    auto bracketWidth = orientation == TextOrientation::Billboard ? pixels / 12.0f : scene->theme().objectWidth;
    auto bottom = top - contentHeight;
    for (auto end = 0u; end < 2u; end++) {
        // From the end nearer the entries, around the outside, for the left bracket; the right
        // one mirrors it.
        auto direction = end ? -1.0f : 1.0f;
        auto inner = end ? left + contentWidth + clearance : left - clearance;
        auto outer = inner - direction * reach;
        auto high = top + overhang;
        auto low = bottom - overhang;
        PathCommand commands[4];
        uint32_t commandCount = 0;
        commands[commandCount].verb = PathVerb::Move;
        commands[commandCount++].to = {inner, high, point.z};
        if (brackets == LuaBrackets::Square) {
            commands[commandCount].verb = PathVerb::Line;
            commands[commandCount++].to = {outer, high, point.z};
            commands[commandCount].verb = PathVerb::Line;
            commands[commandCount++].to = {outer, low, point.z};
            commands[commandCount].verb = PathVerb::Line;
            commands[commandCount++].to = {inner, low, point.z};
        } else if (brackets == LuaBrackets::Round) {
            // The bulge reaches the outer edge halfway down, where a cubic with both controls
            // a third farther out than the edge peaks.
            auto control = inner - direction * reach * (4.0f / 3.0f);
            auto lean = 0.2f * (high - low);
            commands[commandCount].verb = PathVerb::Cubic;
            commands[commandCount].control1 = {control, high - lean, point.z};
            commands[commandCount].control2 = {control, low + lean, point.z};
            commands[commandCount++].to = {inner, low, point.z};
        } else {
            commands[commandCount].verb = PathVerb::Line;
            commands[commandCount++].to = {inner, low, point.z};
        }
        auto path = Path::gen(commands, commandCount);
        if (path) {
            path->stroke(bracketColor);
            path->strokeWidth(bracketWidth);
        }
        lua_createtable(lua, 0, 5);
        auto style = lua_gettop(lua);
        for (auto name : {"color", "stroke", "width", "layer"}) {
            lua_getfield(lua, 2, name);
            lua_setfield(lua, style, name);
        }
        if (matrixId) {
            lua_pushfstring(lua, "%s/%s", matrixId, end ? "right" : "left");
            lua_setfield(lua, style, "id");
        }
        _add(lua, inside, _owner(lua, space.scene), path, style, "matrix bracket");
        lua_settop(lua, result);
    }
    return 1;
}

// The rules a table draws: none, one under its header, or that one and the lines between its
// rows, its columns, or both with a frame around.
enum struct LuaRules : uint8_t
{
    None,
    Header,
    Rows,
    Columns,
    Grid
};

// scene:table{rows = {{...}, ...}, header = {...}}: entries in cells that tile a grid, the
// header's first, as a group whose children are its rows, each a group of the texts of its
// entries, then the stripes behind every second row below the header and then the rules.
static int _tableGen(lua_State* lua)
{
    auto space = _factory(lua, {"rows", "header", "point", "align", "padding", "justify", "decimals", "stripes",
                                "rules", "size", "font", "role", "orientation", "id", "color", "fill", "stroke",
                                "width", "opacity", "progress", "layer", "isolate", "mask", "blend", "blur",
                                "shadow"}, false);
    auto scene = space.scene->scene;
    auto& theme = scene->theme();
    lua_getfield(lua, 2, "rows");
    auto list = lua_gettop(lua);
    uint32_t bodyRows;
    uint32_t columns;
    _gridShape(lua, list, "table", bodyRows, columns);
    lua_getfield(lua, 2, "header");
    auto header = lua_gettop(lua);
    auto headed = !lua_isnil(lua, header);
    if (headed && (!lua_istable(lua, header) || lua_rawlen(lua, header) != columns)) {
        return luaL_error(lua, "table header must be a list of one entry for each of its %d columns",
                          static_cast<int>(columns));
    }
    auto rows = bodyRows + (headed ? 1u : 0u);
    if (static_cast<uint64_t>(rows) * columns > LUA_GRID_LIMIT) {
        return luaL_error(lua, "a table holds at most %d entries", static_cast<int>(LUA_GRID_LIMIT));
    }
    auto point = _fieldWorld(lua, 2, "point", {});
    auto align = _fieldVec2(lua, 2, "align", {0.5f, 0.5f});
    static constexpr LuaName<LuaRules> RULES[] = {{"header", LuaRules::Header}, {"rows", LuaRules::Rows},
                                                  {"columns", LuaRules::Columns}, {"grid", LuaRules::Grid},
                                                  {"none", LuaRules::None}};
    auto rules = _fieldEnum(lua, 2, "rules", headed ? LuaRules::Header : LuaRules::None, RULES);
    // Stripes fill every second row below the header, in the theme's surface color or another.
    auto striped = false;
    Color stripe = theme.color(ThemeColorRole::Surface);
    lua_getfield(lua, 2, "stripes");
    if (lua_isboolean(lua, -1)) striped = lua_toboolean(lua, -1);
    else if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) != LUA_TSTRING) return luaL_error(lua, "stripes must be true or a color");
        stripe = _color(lua, -1, "stripes", &theme);
        striped = true;
    }
    lua_pop(lua, 1);
    auto decimals = _gridDecimals(lua, 2);
    auto orientation = _textOrientation(lua, 2);
    auto justify = static_cast<LuaJustify*>(lua_newuserdatauv(lua, sizeof(LuaJustify) * columns, 0));
    _justify(lua, 2, "table", justify, columns);
    auto layer = _fieldInteger(lua, 2, "layer", 0);
    if (layer < INT32_MIN || layer > INT32_MAX) return luaL_error(lua, "layer is out of range");

    // The line a blank takes in the table's own style sets the padding inside the cells.
    auto blank = _blank(lua, space.scene, orientation).extent;
    auto line = blank.ascent + blank.descent;
    auto padding = _fieldSpacing(lua, "padding", {0.8f * line, 0.3f * line});

    auto count = rows * columns;
    auto entries = static_cast<LuaGridEntry*>(lua_newuserdatauv(lua, sizeof(LuaGridEntry) * count, 0));
    lua_createtable(lua, static_cast<int>(count), 0);
    auto handles = lua_gettop(lua);
    lua_createtable(lua, static_cast<int>(count), 0);
    auto styles = lua_gettop(lua);
    // The header takes the accent color unless the table colors every text.
    auto colored = _has(lua, 2, "color") || _has(lua, 2, "fill");
    auto tableId = _pushId(lua);
    for (auto r = 0u; r < rows; r++) {
        auto heading = headed && r == 0u;
        for (auto c = 0u; c < columns; c++) {
            auto i = r * columns + c;
            if (heading) lua_pushvalue(lua, header);
            else lua_rawgeti(lua, list, r + (headed ? 0u : 1u));
            lua_rawgeti(lua, -1, c + 1u);
            auto& entry = *new (entries + i) LuaGridEntry();
            entry.text = _gridText(lua, space.scene, 2, "table", decimals, orientation);
            lua_rawseti(lua, handles, i + 1u);
            lua_pop(lua, 1);
            _spanStyle(lua, 2, 0);
            if (heading && !colored) {
                lua_pushliteral(lua, "accent");
                lua_setfield(lua, -2, "fill");
            }
            _childId(lua, -1, tableId, r + 1u, c + 1u);
            lua_rawseti(lua, styles, i + 1u);
            entry.extent = _measured(lua, scene, entry.text);
        }
    }

    // Cells tile the table: a column is its widest entry and the padding on both sides wide,
    // and a row holds the tallest ascent and descent of its entries inside the padding.
    auto widths = static_cast<float*>(lua_newuserdatauv(lua, sizeof(float) * (columns + 2u * rows), 0));
    auto ascents = widths + columns;
    auto heights = ascents + rows;
    auto width = 0.0f;
    auto height = 0.0f;
    for (auto c = 0u; c < columns; c++) {
        widths[c] = 0.0f;
        for (auto r = 0u; r < rows; r++) widths[c] = std::max(widths[c], entries[r * columns + c].extent.width);
        widths[c] += 2.0f * padding.x;
        width += widths[c];
    }
    for (auto r = 0u; r < rows; r++) {
        ascents[r] = blank.ascent;
        auto descent = blank.descent;
        for (auto c = 0u; c < columns; c++) {
            ascents[r] = std::max(ascents[r], entries[r * columns + c].extent.ascent);
            descent = std::max(descent, entries[r * columns + c].extent.descent);
        }
        heights[r] = ascents[r] + descent + 2.0f * padding.y;
        height += heights[r];
    }
    auto left = point.x - align.x * width;
    auto top = point.y + align.y * height;
    auto y = top;
    for (auto r = 0u; r < rows; r++) {
        auto x = left;
        for (auto c = 0u; c < columns; c++) {
            _gridPlace(entries[r * columns + c], x, y, widths[c], heights[r], ascents[r], padding, justify[c], point.z);
            x += widths[c];
        }
        y -= heights[r];
    }

    // The group takes the id and layer options; its rows follow it in order.
    auto group = _typeset(lua, space, "table");
    auto result = lua_gettop(lua);
    LuaSpace inside{space.scene, group};
    for (auto r = 0u; r < rows; r++) {
        lua_createtable(lua, 0, 1);
        _childId(lua, -1, tableId, r + 1u);
        auto rowStyle = lua_gettop(lua);
        auto rowGroup = Group::gen();
        _add(lua, inside, _owner(lua, space.scene), rowGroup, rowStyle, "table row");
        auto rowTop = lua_gettop(lua);
        LuaSpace members{space.scene, rowGroup};
        for (auto c = 0u; c < columns; c++) {
            auto i = r * columns + c;
            lua_rawgeti(lua, styles, i + 1u);
            auto style = lua_gettop(lua);
            lua_rawgeti(lua, handles, i + 1u);
            auto entryOwner = static_cast<LuaObject*>(lua_touserdata(lua, -1));
            _add(lua, members, entryOwner, entries[i].text, style, "table entry");
            lua_settop(lua, rowTop);
        }
        lua_settop(lua, result);
    }

    // A stripe lies one layer below the texts, so a band at that layer added later lies
    // between the stripe and the texts.
    auto rowTop = [&](uint32_t row) {
        auto at = top;
        for (auto r = 0u; r < row; r++) at -= heights[r];
        return at;
    };
    if (striped) {
        for (auto body = 1u; body < bodyRows; body += 2u) {
            auto r = body + (headed ? 1u : 0u);
            auto center = Vec3{left + 0.5f * width, rowTop(r) - 0.5f * heights[r], point.z};
            auto band = Rectangle::gen(center, {width, heights[r]});
            if (band) {
                band->fill(stripe);
                band->stroke({0, 0, 0, 0});
            }
            lua_createtable(lua, 0, 1);
            lua_pushinteger(lua, std::max<lua_Integer>(layer - 1, INT32_MIN));
            lua_setfield(lua, -2, "layer");
            auto style = lua_gettop(lua);
            _add(lua, inside, _owner(lua, space.scene), band, style, "table stripe");
            lua_settop(lua, result);
        }
    }

    // The rule under the header and the frame take the foreground color and the lines
    // between rows and columns the theme's border color, unless stroke sets both.
    auto rule = [&](const Vec3& from, const Vec3& to, bool strong) {
        auto ruled = Line::gen(from, to);
        if (ruled) {
            ruled->stroke(theme.color(strong ? ThemeColorRole::Foreground : ThemeColorRole::Border));
            ruled->strokeWidth(strong ? 1.5f : 1.0f);
        }
        lua_createtable(lua, 0, 3);
        auto style = lua_gettop(lua);
        for (auto name : {"stroke", "width", "layer"}) {
            lua_getfield(lua, 2, name);
            lua_setfield(lua, style, name);
        }
        _add(lua, inside, _owner(lua, space.scene), ruled, style, "table rule");
        lua_settop(lua, result);
    };
    auto right = left + width;
    auto bottom = top - height;
    if (rules == LuaRules::None) return 1;
    if (headed) rule({left, rowTop(1u), point.z}, {right, rowTop(1u), point.z}, true);
    if (rules == LuaRules::Rows || rules == LuaRules::Grid) {
        for (auto r = (headed ? 2u : 1u); r < rows; r++) {
            rule({left, rowTop(r), point.z}, {right, rowTop(r), point.z}, false);
        }
    }
    if (rules == LuaRules::Columns || rules == LuaRules::Grid) {
        auto x = left;
        for (auto c = 0u; c + 1u < columns; c++) {
            x += widths[c];
            rule({x, top, point.z}, {x, bottom, point.z}, false);
        }
    }
    if (rules == LuaRules::Grid) {
        rule({left, top, point.z}, {right, top, point.z}, true);
        rule({left, bottom, point.z}, {right, bottom, point.z}, true);
        rule({left, top, point.z}, {left, bottom, point.z}, true);
        rule({right, top, point.z}, {right, bottom, point.z}, true);
    }
    return 1;
}

static int _vector(lua_State* lua)
{
    auto space = _factory(lua, {"value", "origin", "tail", "tip", "trim_distance"});
    auto value = _fieldWorld(lua, 2, "value");
    auto origin = _fieldWorld(lua, 2, "origin", {});
    auto tail = _fieldNumber(lua, 2, "tail", 0.0f);
    auto tip = _fieldNumber(lua, 2, "tip", 16.0f);
    if (tail < 0.0f || tip < 0.0f) return luaL_error(lua, "tail and tip must be non-negative");
    auto trimDistance = _trimDistance(lua, 2);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Vector::gen(value, origin));
    if (object) {
        object->trimDistance = trimDistance;
        object->tail = tail;
        object->tip = tip;
    }
    return _add(lua, space, owner, object, 2, "vector");
}

static int _groupGen(lua_State* lua)
{
    auto space = _factory(lua, {"matrix", "typeset"});
    for (auto shape : {"color", "gradient", "radius", "dash", "dash_offset"}) {
        _refuse(lua, shape, "a group draws nothing itself, so %s is unsupported; the stroke, fill and width it "
                            "sets pass to the shapes below it");
    }
    auto typeset = _fieldBool(lua, 2, "typeset", false);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Group::gen());
    if (object) object->typeset = typeset;
    if (object && _has(lua, 2, "matrix")) {
        lua_getfield(lua, 2, "matrix");
        _matrix(lua, -1, object->model.e, 16, "matrix");
        lua_pop(lua, 1);
    }
    return _add(lua, space, owner, object, 2, "group");
}

// The ticks option of a space: true for a tick at every step of each range, or {x = step,
// y = step, z = step}, each zero for none or true for the step of its range.
static Vec3 _spaceTicks(lua_State* lua, int index, const Space* space)
{
    static constexpr auto TICKS = "ticks must be true or {x = step, y = step, z = step}";
    Vec3 ticks;
    lua_getfield(lua, index, "ticks");
    if (lua_isboolean(lua, -1)) {
        if (lua_toboolean(lua, -1)) ticks = {space->x.step, space->y.step, space->z.step};
    } else if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1)) luaL_error(lua, TICKS);
        _options(lua, -1, {"x", "y", "z"}, false);
        const char* names[] = {"x", "y", "z"};
        const float steps[] = {space->x.step, space->y.step, space->z.step};
        float* outputs[] = {&ticks.x, &ticks.y, &ticks.z};
        for (auto axis = 0u; axis < 3u; axis++) {
            lua_getfield(lua, -1, names[axis]);
            if (lua_isboolean(lua, -1)) {
                if (lua_toboolean(lua, -1)) *outputs[axis] = steps[axis];
            } else if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TNUMBER) luaL_error(lua, TICKS);
                *outputs[axis] = _number(lua, -1, "a tick step");
                if (*outputs[axis] < 0.0f) luaL_error(lua, "ticks %s must be zero or more", names[axis]);
            }
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);
    return ticks;
}

// The labels of a space, from space:labels{...}, the table at index: x, y and z list
// {value, label} pairs, each label a string, which becomes a text below the space in the size
// and color of its numbers unless size and color say otherwise, or the handle of an object of
// the scene. Each goes beside the tick at its value, gap pixels past it, on the side side names
// (see Space::place). Pushes the list of their handles, in the order given.
static void _spaceLabels(lua_State* lua, LuaScene* scene, Space* space, int index)
{
    static constexpr auto SIDE = "labels side must be {x = 'below' or 'above', y = 'left' or 'right', z = 'left' or 'right'}";
    index = lua_absindex(lua, index);
    _options(lua, index, {"x", "y", "z", "gap", "size", "color", "side", "placement"}, false);
    static constexpr LuaName<bool> PLACEMENTS[] = {{"world", false}, {"screen", true}};
    auto screen = _fieldEnum(lua, index, "placement", false, PLACEMENTS, "labels placement");
    if (screen && _has(lua, index, "side")) luaL_error(lua, "screen labels do not take side");
    auto gap = _fieldNumber(lua, index, "gap", 4.0f);
    if (gap < 0.0f) luaL_error(lua, "labels gap must be zero or more pixels");
    auto size = _fieldNumber(lua, index, "size", space->numberSize);
    if (size <= 0.0f) luaL_error(lua, "labels size must be positive");
    auto color = space->numberColor;
    _fieldColor(lua, index, "color", color, &scene->scene->theme());
    // The way from each axis to its labels in the space: below x and left of y and z.
    Vec3 sides[] = {{0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    const char* axes[] = {"x", "y", "z"};
    lua_getfield(lua, index, "side");
    if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1)) luaL_error(lua, SIDE);
        _options(lua, -1, {"x", "y", "z"}, false);
        for (auto axis = 0u; axis < 3u; axis++) {
            auto near = axis == 0u ? "below" : "left";
            auto far = axis == 0u ? "above" : "right";
            auto side = _fieldOptionalString(lua, -1, axes[axis], near);
            if (std::strcmp(side, far) == 0) sides[axis] = sides[axis] * -1.0f;
            else if (std::strcmp(side, near) != 0) luaL_error(lua, SIDE);
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);
    lua_newtable(lua);
    auto list = lua_gettop(lua);
    auto count = 0;
    for (auto axis = 0u; axis < 3u; axis++) {
        lua_getfield(lua, index, axes[axis]);
        auto entries = lua_gettop(lua);
        if (!lua_isnil(lua, entries) && !lua_istable(lua, entries)) {
            luaL_error(lua, "labels %s must be a list of {value, label}", axes[axis]);
        }
        auto total = lua_isnil(lua, entries) ? 0 : static_cast<int>(lua_rawlen(lua, entries));
        auto number = 0;
        for (auto i = 1; i <= total; i++) {
            lua_rawgeti(lua, entries, i);
            if (lua_istable(lua, -1)) {
                lua_rawgeti(lua, -1, 1);
                lua_rawgeti(lua, -2, 2);
            }
            auto kind = lua_type(lua, -1);
            if (lua_gettop(lua) != entries + 3 || lua_type(lua, -2) != LUA_TNUMBER
                || (kind != LUA_TSTRING && kind != LUA_TUSERDATA)) {
                luaL_error(lua, "labels %s entry %d must be {value, label}, a number and a string or handle",
                           axes[axis], i);
            }
            auto value = _number(lua, -2, "a label's value");
            if (kind == LUA_TSTRING) {
                // The string stays on the stack until the text has copied it.
                size_t length;
                auto string = lua_tolstring(lua, -1, &length);
                if (std::strlen(string) != length) luaL_error(lua, "a label cannot contain zero bytes");
                if (length > LUA_TEXT_LIMIT) luaL_error(lua, "a label exceeds the 1 MiB limit");
                lua_newtable(lua);
                auto style = lua_gettop(lua);
                if (auto id = space->tag()) {
                    do {
                        lua_pushfstring(lua, "%s/label/%s/%d", id, axes[axis], ++number);
                        if (!scene->scene->object(lua_tostring(lua, -1))) break;
                        lua_pop(lua, 1);
                    } while (true);
                    lua_setfield(lua, style, "id");
                }
                auto owner = _owner(lua, scene);
                auto label = _adopt(owner, Text::gen(string));
                if (label) {
                    label->size = size;
                    label->fill(color);
                    auto result = screen ? space->placeScreen(label, static_cast<uint8_t>(axis), value, gap)
                                         : space->place(label, static_cast<uint8_t>(axis), value, sides[axis], gap);
                    _check(lua, scene->scene, result, "could not place label");
                }
                _add(lua, {scene, space}, owner, label, style, "label");
            } else {
                auto label = _object(lua, scene->scene, -1);
                if (screen && label->type() != Type::Text) luaL_error(lua, "a screen label must be a billboard Text");
                auto result = screen ? space->placeScreen(static_cast<Text*>(label), static_cast<uint8_t>(axis), value, gap)
                                     : space->place(label, static_cast<uint8_t>(axis), value, sides[axis], gap);
                _check(lua, scene->scene, result, "could not place label");
                lua_pushvalue(lua, -1);
            }
            lua_rawseti(lua, list, ++count);
            lua_settop(lua, entries);
        }
        lua_pop(lua, 1);
    }
}

// space:labels{x = {{value, label}, ...}, ...}: places labels beside the ticks of the space
// and returns their handles. It makes texts and lays out handles by the transforms the space
// and the labels were authored with, not where clips move them, so like a factory or a layout
// call it leaves the tracks that wait waiting.
static int _labelsMethod(lua_State* lua)
{
    auto space = _space(lua);
    if (!space.parent || space.parent->type() != Type::Space) return luaL_error(lua, "labels requires a Space");
    luaL_checktype(lua, 2, LUA_TTABLE);
    _spaceLabels(lua, space.scene, static_cast<Space*>(space.parent), 2);
    return 1;
}

static int _spaceGen(lua_State* lua)
{
    auto space = _factory(lua, {"x", "y", "z", "matrix", "axis_x", "axis_y", "axis_z", "numbers", "number_mode",
                                "number_size", "number_color", "ticks", "tick_length", "axis_width"});
    for (auto shape : {"gradient"}) _refuse(lua, shape, "%s is supported only by vector Shapes");
    auto x = _fieldRange(lua, 2, "x", {});
    auto y = _fieldRange(lua, 2, "y", {});
    auto z = _fieldRange(lua, 2, "z", {});
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Space::gen(x, y, z));
    if (object) {
        if (_has(lua, 2, "matrix")) {
            lua_getfield(lua, 2, "matrix");
            _matrix(lua, -1, object->model.e, 16, "matrix");
            lua_pop(lua, 1);
        }
        object->axisWidth = _fieldNumber(lua, 2, "axis_width", object->axisWidth);
        if (_has(lua, 2, "axis_width") && object->axisWidth < 0.0f) return luaL_error(lua, "axis_width must be zero or more pixels");
        Color axis;
        auto& theme = space.scene->scene->theme();
        if (_fieldColor(lua, 2, "axis_x", axis, &theme)) object->axisX = axis;
        if (_fieldColor(lua, 2, "axis_y", axis, &theme)) object->axisY = axis;
        if (_fieldColor(lua, 2, "axis_z", axis, &theme)) object->axisZ = axis;
        if (_fieldColor(lua, 2, "number_color", axis, &theme)) object->numberColor = axis;
        object->numbers = _fieldBool(lua, 2, "numbers", false);
        object->numberSize = _fieldNumber(lua, 2, "number_size", object->numberSize);
        static constexpr LuaName<NumberMode> MODES[] = {
            {"fixed", NumberMode::Fixed}, {"relative", NumberMode::Relative}};
        object->numberMode = _fieldEnum(lua, 2, "number_mode", NumberMode::Fixed, MODES);
        if (object->numberSize <= 0.0f) return luaL_error(lua, "number_size must be positive");
        object->ticks = _spaceTicks(lua, 2, object);
        object->tickLength = _fieldNumber(lua, 2, "tick_length", object->tickLength);
        if (object->tickLength < 0.0f) return luaL_error(lua, "tick_length must be zero or more pixels");
    }
    return _add(lua, space, owner, object, 2, "space");
}

// ---------- Circles, arcs, angles and polygons ----------

static int _circle(lua_State* lua)
{
    auto space = _factory(lua, {"center"});
    auto center = _fieldWorld(lua, 2, "center", {});
    auto radius = _fieldNumber(lua, 2, "radius", 1.0f);
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Circle::gen(center, radius), 2, "circle");
}

static int _arc(lua_State* lua)
{
    auto space = _factory(lua, {"center", "start", "angle", "sector"});
    auto center = _fieldWorld(lua, 2, "center", {});
    auto radius = _fieldNumber(lua, 2, "radius", 1.0f);
    auto start = _fieldNumber(lua, 2, "start", 0.0f);
    auto angle = _fieldNumber(lua, 2, "angle", HALF_PI);
    if (radius <= 0.0f) return luaL_error(lua, "radius must be positive");
    if (std::fabs(angle) > TWO_PI) return luaL_error(lua, "angle must be between -2 pi and 2 pi");
    auto sector = _fieldBool(lua, 2, "sector", false);
    auto owner = _owner(lua, space.scene);
    auto object = Arc::gen(center, radius, start, angle);
    if (object) object->sector = sector;
    return _add(lua, space, owner, object, 2, "arc");
}

static int _angle(lua_State* lua)
{
    auto space = _factory(lua, {"vertex", "from", "to", "other", "right", "label", "label_distance"});
    auto vertex = _fieldWorld(lua, 2, "vertex", {});
    auto from = _fieldWorld(lua, 2, "from", {1.0f, 0.0f, 0.0f});
    auto to = _fieldWorld(lua, 2, "to", {0.0f, 1.0f, 0.0f});
    auto radius = _fieldNumber(lua, 2, "radius", 1.0f);
    if (radius <= 0.0f) return luaL_error(lua, "radius must be positive");
    auto other = _fieldBool(lua, 2, "other", false);
    static constexpr LuaName<RightAngle> RIGHTS[] = {
        {"auto", RightAngle::Auto}, {"never", RightAngle::Never}, {"always", RightAngle::Always}};
    auto mode = _fieldEnum(lua, 2, "right", RightAngle::Auto, RIGHTS);
    auto distance = _fieldNumber(lua, 2, "label_distance", radius * 1.7f);
    if (distance < 0.0f) return luaL_error(lua, "label_distance must be zero or more");
    auto owner = _owner(lua, space.scene);
    auto object = Angle::gen(vertex, from, to, radius);
    if (object) { object->other = other; object->right = mode; object->labelDistance = distance; }
    _add(lua, space, owner, object, 2, "angle");
    auto handle = lua_gettop(lua);
    lua_getfield(lua, 2, "label");
    if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) == LUA_TSTRING) {
            lua_newtable(lua);
            lua_pushvalue(lua, -2); lua_setfield(lua, -2, "text");
            lua_remove(lua, -2);
        } else if (!lua_istable(lua, -1)) return luaL_error(lua, "label must be text or a text or tex table");
        auto options = lua_gettop(lua);
        auto tex = _has(lua, options, "tex");
        lua_getfield(lua, handle, tex ? "tex" : "text");
        lua_pushvalue(lua, handle); lua_pushvalue(lua, options);
        detail::luaCall(lua, 2, 1);
    }
    lua_settop(lua, handle);
    return 1;
}

// Grows output by the bounds of object in the space of ancestor, the root when it is null,
// through the models of the groups between them. False when object is not below ancestor.
static bool _surround(const Object* object, const Object* ancestor, Bounds& output, bool& defined)
{
    auto model = Mat4::identity();
    auto parent = object->parent();
    for (; parent && parent != ancestor; parent = parent->parent()) model = parent->model * model;
    if (parent != ancestor) return false;
    Bounds own;
    if (!object->bounds(own)) return true;
    for (auto corner = 0u; corner < 8u; corner++) {
        include(output, defined, model * Vec3{corner & 1u ? own.max.x : own.min.x,
                                              corner & 2u ? own.max.y : own.min.y,
                                              corner & 4u ? own.max.z : own.min.z});
    }
    return true;
}

// Calls add with the stack index of the handle at index, or of each handle in the list there,
// which must not be empty; anything else stops the script with shape.
template<typename Add>
static void _eachHandle(lua_State* lua, int index, const char* shape, Add add)
{
    if (lua_type(lua, index) == LUA_TUSERDATA) {
        add(index);
        return;
    }
    auto count = lua_istable(lua, index) ? lua_rawlen(lua, index) : 0u;
    if (!count) luaL_error(lua, "%s", shape);
    for (auto i = 1u; i <= count; i++) {
        lua_rawgeti(lua, index, static_cast<lua_Integer>(i));
        if (lua_type(lua, -1) != LUA_TUSERDATA) luaL_error(lua, "%s", shape);
        add(-1);
        lua_pop(lua, 1);
    }
}

// The around option of a rectangle, or of what else names itself as joiner: the bounds of a
// handle, or of a list of handles, in the space it joins. Without a joiner, the value that
// scene:bounds measures in the root's space.
static Bounds _around(lua_State* lua, const LuaSpace& space, int index, const char* joiner = "rectangle")
{
    auto scene = space.scene->scene;
    auto shape = joiner ? "around must be a handle or a list of handles" : "bounds takes a handle or a list of handles";
    Bounds bounds;
    auto defined = false;
    _eachHandle(lua, index, shape, [&](int item) {
        // Everything is below the root, so only a joiner in a group can miss.
        if (!_surround(_object(lua, scene, item), space.parent, bounds, defined)) {
            luaL_error(lua, "around takes objects below the group the %s joins", joiner ? joiner : "scene");
        }
    });
    if (!defined) {
        luaL_error(lua, "%s needs objects with bounds, such as texts set with measure = true",
                   joiner ? "around" : "bounds");
    }
    return bounds;
}

static int _rectangle(lua_State* lua)
{
    auto space = _factory(lua, {"center", "size", "corner", "around", "padding"});
    auto center = _fieldWorld(lua, 2, "center", {});
    auto size = _fieldVec2(lua, 2, "size", {1.0f, 1.0f});
    auto corner = _fieldNumber(lua, 2, "corner", 0.0f);
    lua_getfield(lua, 2, "around");
    if (!lua_isnil(lua, -1)) {
        if (_has(lua, 2, "center") || _has(lua, 2, "size")) {
            return luaL_error(lua, "around sets the center and size, so leave out center and size");
        }
        // The padding grows every side.
        auto padding = _fieldSpacing(lua, "padding", {0.1f, 0.1f});
        auto bounds = _around(lua, space, lua_gettop(lua));
        center = bounds.center();
        size = {bounds.max.x - bounds.min.x + 2.0f * padding.x, bounds.max.y - bounds.min.y + 2.0f * padding.y};
    } else if (_has(lua, 2, "padding")) {
        return luaL_error(lua, "padding needs around");
    }
    lua_pop(lua, 1);
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Rectangle::gen(center, size, corner), 2, "rectangle");
}

// scene:bounds(handle_or_list) reports where a handle, or a list of handles, lies in world
// units, as scene:rectangle{around} frames it: through the groups above each, as everything
// authored so far leaves them. Sides are numbers, points {x, y, z}.
static int _boundsOf(lua_State* lua)
{
    auto box = _scene(lua);
    lua_settop(lua, 2);
    auto bounds = _around(lua, {box, nullptr}, 2, nullptr);
    auto number = [&](const char* name, float value) {
        lua_pushnumber(lua, value);
        lua_setfield(lua, -2, name);
    };
    auto point = [&](const char* name, const Vec3& value) {
        lua_createtable(lua, 3, 0);
        lua_pushnumber(lua, value.x);
        lua_rawseti(lua, -2, 1);
        lua_pushnumber(lua, value.y);
        lua_rawseti(lua, -2, 2);
        lua_pushnumber(lua, value.z);
        lua_rawseti(lua, -2, 3);
        lua_setfield(lua, -2, name);
    };
    lua_createtable(lua, 0, 9);
    number("left", bounds.min.x);
    number("right", bounds.max.x);
    number("bottom", bounds.min.y);
    number("top", bounds.max.y);
    number("width", bounds.max.x - bounds.min.x);
    number("height", bounds.max.y - bounds.min.y);
    point("center", bounds.center());
    point("min", bounds.min);
    point("max", bounds.max);
    return 1;
}

static int _polygon(lua_State* lua)
{
    auto space = _factory(lua, {"points"});
    lua_getfield(lua, 2, "points");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "missing field 'points'");
    uint32_t count;
    auto points = _points(lua, -1, 3, count);
    _fit(lua, space.scene, {count * sizeof(Vec3) + 512u, 0, count});
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Polygon::gen(points, count), 2, "polygon");
}

// ---------- Plots, areas and regions ----------

// Where a value of a plot lies: undefined, as nil, nan or an infinity, or below, inside or above
// its y_range, which holds every defined value when there is none.
enum class LuaPlotSide : uint8_t
{
    Undefined,
    Below,
    Inside,
    Above
};

struct LuaPlotSample
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    LuaPlotSide side = LuaPlotSide::Undefined;
};

// A plot cut into pieces as it is read: the points of each piece in turn and where each ends,
// in userdata buffers at fixed stack slots, so growing one never frees memory still in use and
// an error leaves them to the collector.
struct LuaPlotCut
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    Vec3* points = nullptr;
    uint32_t* ends = nullptr;
    uint32_t capacity = 0;
    uint32_t endCapacity = 0;
    uint32_t sampleCapacity = 0;
    uint32_t count = 0;
    uint32_t pieces = 0;
    uint32_t start = 0;
    int pointSlot = 0;
    int endSlot = 0;
    int sampleSlot = 0;
    int fn = 0;
    bool open = false;
    bool band = false;
    double lo = 0.0;
    double hi = 0.0;
    // The discontinuities, sorted, in a userdata buffer on the stack, and whether any were given.
    const double* breaks = nullptr;
    uint32_t breakCount = 0;
    bool broken = false;
    PlotSamples samples;
};

// What a plot keeps of its cut, which a state that gives it fn or points cuts it with again: the
// x_range that fn was sampled over, when it was, the band of y_range, and the discontinuities,
// sorted, which follow the struct in its userdata. room and endRoom are the most points and
// pieces of more than one it has held, which the scene keeps room for.
struct LuaPlotRecord
{
    Range range;
    Range contourY;
    double level = 0.0;
    bool contour = false;
    bool sampled = false;
    bool band = false;
    double lo = 0.0;
    double hi = 0.0;
    uint32_t breakCount = 0;
    bool broken = false;
    uint32_t room = 0;
    uint32_t endRoom = 0;
    uint32_t sampleRoom = 0;
    bool copied = false;
};

// The times a plot halves the gap between two samples to find where its graph leaves y_range,
// stops being defined, or crosses the band: a step of 1 narrows to about 1e-12.
static constexpr int LUA_PLOT_HALVINGS = 40;

static LuaPlotSide _plotSide(const LuaPlotCut& cut, double y)
{
    if (cut.band && y < cut.lo) return LuaPlotSide::Below;
    if (cut.band && y > cut.hi) return LuaPlotSide::Above;
    return LuaPlotSide::Inside;
}

static void _plotAdd(LuaPlotCut& cut, double x, double y, double z)
{
    if (cut.count == LUA_POINT_LIMIT) luaL_error(cut.lua, "scene point limit exceeded");
    cut.points = _grow(cut.lua, cut.pointSlot, cut.points, cut.count, cut.capacity, LUA_POINT_LIMIT);
    new (cut.points + cut.count++) Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}

// Adds a point a cut makes, unless the open piece already ends there.
static void _plotPut(LuaPlotCut& cut, double x, double y, double z)
{
    if (cut.count > cut.start) {
        auto& last = cut.points[cut.count - 1u];
        if (last.x == static_cast<float>(x) && last.y == static_cast<float>(y) && last.z == static_cast<float>(z)) {
            return;
        }
    }
    _plotAdd(cut, x, y, z);
}

// Ends the open piece, which is dropped if it has fewer than two points to draw a stroke between.
static void _plotEnd(LuaPlotCut& cut)
{
    if (!cut.open) return;
    cut.open = false;
    if (cut.count - cut.start < 2u) {
        cut.count = cut.start;
        return;
    }
    cut.ends = _grow(cut.lua, cut.endSlot, cut.ends, cut.pieces, cut.endCapacity);
    cut.ends[cut.pieces++] = cut.count;
}

static void _plotStart(LuaPlotCut& cut)
{
    _plotEnd(cut);
    cut.open = true;
    cut.start = cut.count;
}

// Begins a run of samples with its first, a piece if it is inside.
static void _plotBegin(LuaPlotCut& cut, const LuaPlotSample& sample)
{
    _plotEnd(cut);
    if (sample.side != LuaPlotSide::Inside) return;
    _plotStart(cut);
    _plotAdd(cut, sample.x, sample.y, sample.z);
}

static double _plotEdge(const LuaPlotCut& cut, LuaPlotSide side)
{
    return side == LuaPlotSide::Above ? cut.hi : cut.lo;
}

// The point at height level on the segment from p to q, whose heights lie on either side of it.
static LuaPlotSample _plotLevel(const LuaPlotSample& p, const LuaPlotSample& q, double level)
{
    auto t = (level - p.y) / (q.y - p.y);
    return {p.x + (q.x - p.x) * t, level, p.z + (q.z - p.z) * t, LuaPlotSide::Inside};
}

// The segment from point p to the next point q, both defined, where p is drawn if it is inside:
// the part of it inside the band, which ends exactly on its edge.
static void _plotSegment(LuaPlotCut& cut, const LuaPlotSample& p, const LuaPlotSample& q)
{
    if (p.side == LuaPlotSide::Inside && q.side == LuaPlotSide::Inside) {
        _plotAdd(cut, q.x, q.y, q.z);
        return;
    }
    if (p.side == LuaPlotSide::Inside) {
        auto exit = _plotLevel(p, q, _plotEdge(cut, q.side));
        _plotPut(cut, exit.x, exit.y, exit.z);
        _plotEnd(cut);
        return;
    }
    if (q.side == p.side) return;
    auto entry = _plotLevel(p, q, _plotEdge(cut, p.side));
    _plotStart(cut);
    _plotPut(cut, entry.x, entry.y, entry.z);
    if (q.side == LuaPlotSide::Inside) {
        _plotPut(cut, q.x, q.y, q.z);
        return;
    }
    auto exit = _plotLevel(p, q, _plotEdge(cut, q.side));
    _plotPut(cut, exit.x, exit.y, exit.z);
    _plotEnd(cut);
}

// A point of a plot, read as _world reads one, and whether it is defined: a coordinate that is
// nan, infinite or too large for the scene leaves it undefined.
static bool _plotPoint(lua_State* lua, int index, LuaPlotSample& sample)
{
    _checkTable(lua, index, "point");
    index = lua_absindex(lua, index);
    auto defined = true;
    auto read = [&](const char* key, int item, double& value) {
        if (key) lua_getfield(lua, index, key);
        else lua_rawgeti(lua, index, item);
        value = _checkNumber(lua, -1, key ? key : "point component");
        if (!(std::fabs(value) <= FLT_MAX)) defined = false;
        lua_pop(lua, 1);
    };
    sample.z = 0.0;
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "z");
    if (keyed) {
        if (!_has(lua, index, "x") || !_has(lua, index, "y")) {
            luaL_error(lua, "point must contain x and y, with optional z");
        }
        if (_has(lua, index, "z")) read("z", 0, sample.z);
        read("x", 0, sample.x);
        read("y", 0, sample.y);
        return defined;
    }
    auto size = lua_rawlen(lua, index);
    if (size != 2 && size != 3) luaL_error(lua, "point must contain 2 or 3 numbers");
    read(nullptr, 1, sample.x);
    read(nullptr, 2, sample.y);
    if (size == 3) read(nullptr, 3, sample.z);
    return defined;
}

// Whether x is one of the discontinuities.
static bool _plotOnBreak(const LuaPlotCut& cut, double x)
{
    return std::binary_search(cut.breaks, cut.breaks + cut.breakCount, x);
}

// Whether a discontinuity lies strictly between a and b.
static bool _plotAcrossBreak(const LuaPlotCut& cut, double a, double b)
{
    auto low = std::min(a, b), high = std::max(a, b);
    auto next = std::upper_bound(cut.breaks, cut.breaks + cut.breakCount, low);
    return next != cut.breaks + cut.breakCount && *next < high;
}

// points = {...}: a polyline through the points, broken at each undefined one and at each
// discontinuity, where a point on it is dropped and a segment across it is not drawn, and cut
// to the band.
static void _plotTable(LuaPlotCut& cut, int table)
{
    auto lua = cut.lua;
    auto size = lua_rawlen(lua, table);
    LuaPlotSample previous;
    for (lua_Unsigned i = 1; i <= size; i++) {
        lua_rawgeti(lua, table, static_cast<lua_Integer>(i));
        LuaPlotSample sample;
        auto defined = _plotPoint(lua, -1, sample) && !_plotOnBreak(cut, sample.x);
        lua_pop(lua, 1);
        sample.side = defined ? _plotSide(cut, sample.y) : LuaPlotSide::Undefined;
        if (sample.side == LuaPlotSide::Undefined) _plotEnd(cut);
        else if (previous.side == LuaPlotSide::Undefined || _plotAcrossBreak(cut, previous.x, sample.x)) {
            _plotBegin(cut, sample);
        } else _plotSegment(cut, previous, sample);
        previous = sample;
    }
    _plotEnd(cut);
}

// fn at x, which is undefined where it returns nil, nan, an infinity, or a number too large for
// the scene.
static LuaPlotSample _plotSample(LuaPlotCut& cut, double x)
{
    auto lua = cut.lua;
    lua_pushvalue(lua, cut.fn);
    lua_pushnumber(lua, x);
    if (_sampleCall(lua, cut.scene, 1, 1) != LUA_OK) lua_error(lua);
    auto number = 0;
    auto y = lua_tonumberx(lua, -1, &number);
    if (!number && !lua_isnil(lua, -1)) {
        luaL_error(lua, "plot fn must return a number, or nil where it is undefined, not %s",
                   luaL_typename(lua, -1));
    }
    lua_pop(lua, 1);
    LuaPlotSample sample;
    sample.x = x;
    if (number && std::fabs(y) <= FLT_MAX) {
        sample.y = y;
        sample.side = _plotSide(cut, y);
    }
    return sample;
}

// Where the graph between a sample inside the band and one that is not leaves it, found by
// halving the gap between them: on the edge of the band where the graph runs up to it, or else
// at the last value inside, where the graph jumps out of the band or stops being defined.
static LuaPlotSample _plotBorder(LuaPlotCut& cut, LuaPlotSample inside, LuaPlotSample outside)
{
    for (auto i = 0; i < LUA_PLOT_HALVINGS; i++) {
        auto x = 0.5 * (inside.x + outside.x);
        if (x == inside.x || x == outside.x) break;
        auto middle = _plotSample(cut, x);
        if (middle.side == LuaPlotSide::Inside) inside = middle;
        else outside = middle;
    }
    // A graph that runs up to the edge ends within a hair of it after the halvings, while one
    // that jumps stays far from it; a thousandth of the band tells them apart.
    auto tolerance = (cut.hi - cut.lo) * 1.0e-3;
    if (outside.side == LuaPlotSide::Above && cut.hi - inside.y <= tolerance) inside.y = cut.hi;
    if (outside.side == LuaPlotSide::Below && inside.y - cut.lo <= tolerance) inside.y = cut.lo;
    return inside;
}

static void _plotSampleAdd(LuaPlotCut& cut, Vec3 point)
{
    if (cut.samples.count == LUA_POINT_LIMIT) luaL_error(cut.lua, "scene point limit exceeded");
    auto points = _grow(cut.lua, cut.sampleSlot, const_cast<Vec3*>(cut.samples.points), cut.samples.count,
                        cut.sampleCapacity, LUA_POINT_LIMIT);
    points[cut.samples.count++] = point;
    cut.samples.points = points;
}

// Between samples on opposite sides of the band, from left to right, the graph crosses it or
// jumps over it, as tan does at a pole: halves the gap until a sample falls inside, and draws
// the piece through it, or until nothing is left between them, and draws nothing.
static bool _plotAcross(LuaPlotCut& cut, LuaPlotSample left, LuaPlotSample right)
{
    for (auto i = 0; i < LUA_PLOT_HALVINGS; i++) {
        auto x = 0.5 * (left.x + right.x);
        if (x == left.x || x == right.x) return false;
        auto middle = _plotSample(cut, x);
        if (middle.side == LuaPlotSide::Inside) {
            auto entry = _plotBorder(cut, middle, left);
            auto exit = _plotBorder(cut, middle, right);
            _plotStart(cut);
            _plotPut(cut, entry.x, entry.y, 0.0);
            _plotPut(cut, middle.x, middle.y, 0.0);
            _plotPut(cut, exit.x, exit.y, 0.0);
            _plotEnd(cut);
            return true;
        }
        if (middle.side == left.side) left = middle;
        else if (middle.side == right.side) right = middle;
        else return false;
    }
    return false;
}

// The graph from sample p to the next sample q, where p is drawn if it is inside.
static void _plotStep(LuaPlotCut& cut, const LuaPlotSample& p, const LuaPlotSample& q)
{
    if (p.side == LuaPlotSide::Inside) {
        if (q.side == LuaPlotSide::Inside) {
            _plotAdd(cut, q.x, q.y, 0.0);
            return;
        }
        auto border = _plotBorder(cut, p, q);
        _plotPut(cut, border.x, border.y, 0.0);
        _plotEnd(cut);
        return;
    }
    if (q.side == LuaPlotSide::Inside) {
        auto border = _plotBorder(cut, q, p);
        _plotStart(cut);
        _plotPut(cut, border.x, border.y, 0.0);
        _plotPut(cut, q.x, q.y, 0.0);
        return;
    }
    if (p.side != LuaPlotSide::Undefined && q.side != LuaPlotSide::Undefined && p.side != q.side) {
        if (!_plotAcross(cut, p, q)) {
            // Retain the rejected crossing as a gap before the last sample, as explicit
            // discontinuities do, so the vertical blend cannot reconnect a pole.
            auto point = cut.samples.points[--cut.samples.count];
            auto undefined = std::numeric_limits<float>::quiet_NaN();
            _plotSampleAdd(cut, {undefined, undefined, 0.0f});
            _plotSampleAdd(cut, point);
        }
    }
}

// Samples fn across x_range. Splits discontinuities at +/-0.001 step and skips intervening
// samples, including discontinuities at the first or last sample.
static void _plotSampled(LuaPlotCut& cut, const Range& range, uint32_t count)
{
    auto capacity = static_cast<size_t>(count) + 3u * cut.breakCount;
    if (capacity > LUA_POINT_LIMIT) luaL_error(cut.lua, "scene point limit exceeded");
    auto points = static_cast<Vec3*>(lua_newuserdatauv(cut.lua, sizeof(Vec3) * capacity, 0));
    cut.sampleSlot = lua_gettop(cut.lua);
    cut.sampleCapacity = static_cast<uint32_t>(capacity);
    cut.samples.points = points;
    cut.samples.band = cut.band;
    cut.samples.yRange = {static_cast<float>(cut.lo), static_cast<float>(cut.hi)};
    auto undefined = std::numeric_limits<float>::quiet_NaN();
    auto sampleAt = [&](double x) {
        auto sample = _plotSample(cut, x);
        _plotSampleAdd(cut, {static_cast<float>(x), sample.side == LuaPlotSide::Undefined
            ? undefined : static_cast<float>(sample.y), 0.0f});
        return sample;
    };
    auto upper = std::nextafter(range.max, std::numeric_limits<float>::infinity());
    auto grid = [&](uint32_t i) {
        auto x = range.min + range.step * static_cast<float>(i);
        if (x > range.max && x <= upper) x = range.max;
        return static_cast<double>(x);
    };
    auto first = grid(0u), last = grid(count - 1u);
    auto margin = static_cast<double>(range.step) * 1.0e-3;
    auto next = 0u;
    while (next < cut.breakCount && cut.breaks[next] < first) next++;
    auto inside = [&](uint32_t index) { return index < cut.breakCount && cut.breaks[index] <= last; };
    auto resume = -std::numeric_limits<double>::infinity();
    auto running = false;
    LuaPlotSample previous;
    for (auto i = 0u; i < count; i++) {
        auto x = grid(i);
        while (inside(next) && cut.breaks[next] - margin < x) {
            auto at = cut.breaks[next++];
            if (running && previous.x < at - margin) _plotStep(cut, previous, sampleAt(at - margin));
            _plotEnd(cut);
            _plotSampleAdd(cut, {undefined, undefined, 0.0f});
            running = false;
            resume = at + margin;
            // A discontinuity within two margins of the next one shares its gap.
            if (inside(next) && cut.breaks[next] - margin <= resume) continue;
            if (resume < last) {
                previous = sampleAt(resume);
                _plotBegin(cut, previous);
                running = true;
            }
        }
        if (x <= resume) continue;
        auto sample = sampleAt(x);
        if (running) _plotStep(cut, previous, sample);
        else _plotBegin(cut, sample);
        previous = sample;
        running = true;
    }
    _plotEnd(cut);
}

// The builder materializes the exact endpoint and the uncut column before emitting Lua.
static void _plotStored(LuaPlotCut& cut)
{
    auto lua = cut.lua;
    _options(lua, cut.fn, {"points", "samples"}, false);
    lua_getfield(lua, cut.fn, "points");
    _checkTable(lua, -1, "points");
    _plotTable(cut, lua_gettop(lua));
    lua_pop(lua, 1);
    lua_getfield(lua, cut.fn, "samples");
    _checkTable(lua, -1, "samples");
    auto table = lua_gettop(lua);
    auto count = lua_rawlen(lua, table);
    if (count < 2u || count > LUA_POINT_LIMIT) {
        luaL_error(lua, "plot samples must hold from 2 to %d points", static_cast<int>(LUA_POINT_LIMIT));
    }
    auto points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * count, 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, table, i + 1u);
        LuaPlotSample sample;
        _plotPoint(lua, -1, sample);
        points[i] = {static_cast<float>(sample.x), static_cast<float>(sample.y), static_cast<float>(sample.z)};
        lua_pop(lua, 1);
    }
    cut.samples = {points, static_cast<uint32_t>(count), cut.band,
                   {static_cast<float>(cut.lo), static_cast<float>(cut.hi)}};
    if (!cut.samples.valid()) luaL_error(lua, "plot samples need an increasing x grid and a finite y_range");
}

// Cuts a plot into buffers on the stack, made with room for samples points: the points at index,
// unless it is 0, or else the samples a stored fn holds or fn sampled across range.
static void _plotCut(LuaPlotCut& cut, int points, const Range& range, uint32_t samples)
{
    auto lua = cut.lua;
    cut.capacity = std::min(LUA_POINT_LIMIT, samples + 16u);
    cut.points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * cut.capacity, 0));
    cut.pointSlot = lua_gettop(lua);
    cut.endCapacity = 16u;
    cut.ends = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * cut.endCapacity, 0));
    cut.endSlot = lua_gettop(lua);
    if (points) _plotTable(cut, points);
    else if (lua_istable(lua, cut.fn)) _plotStored(cut);
    else _plotSampled(cut, range, samples);
}

// The range field name of the table at index, which what requires.
static Range _requiredRange(lua_State* lua, int index, const char* name, const char* what)
{
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) luaL_error(lua, "%s requires %s", what, name);
    auto range = _range(lua, -1, name);
    lua_pop(lua, 1);
    return range;
}

// The samples fn = f takes over x_range: from min every step up to max, which counts when a step
// lands on it.
static uint32_t _plotSamples(lua_State* lua, const Range& range)
{
    auto intervals = std::floor((static_cast<double>(range.max) - range.min) / range.step);
    if (!std::isfinite(intervals) || intervals < 1.0 || intervals >= LUA_POINT_LIMIT) {
        luaL_error(lua, "x_range produces an invalid sample count");
    }
    auto count = static_cast<uint32_t>(intervals) + 1u;
    auto upper = std::nextafter(range.max, std::numeric_limits<float>::infinity());
    auto sample = [&](uint32_t index) {
        return range.min + range.step * static_cast<float>(index);
    };
    while (count > 1u && (!std::isfinite(sample(count - 1u)) || sample(count - 1u) > upper))
        count--;
    while (count < LUA_POINT_LIMIT && std::isfinite(sample(count)) && sample(count) <= upper)
        count++;
    return count;
}

// y_range = {lo, hi} in the table at index: the band a plot is cut to.
static void _plotBand(lua_State* lua, int index, LuaPlotCut& cut)
{
    lua_getfield(lua, index, "y_range");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2) luaL_error(lua, "y_range must be {lo, hi}");
    double bounds[2];
    for (auto i = 0; i < 2; i++) {
        lua_rawgeti(lua, -1, i + 1);
        auto number = 0;
        bounds[i] = lua_tonumberx(lua, -1, &number);
        if (!number || !(std::fabs(bounds[i]) <= FLT_MAX)) luaL_error(lua, "y_range must hold two finite numbers");
        lua_pop(lua, 1);
    }
    if (!(bounds[0] < bounds[1])) luaL_error(lua, "y_range must have lo below hi");
    lua_pop(lua, 1);
    cut.band = true;
    cut.lo = bounds[0];
    cut.hi = bounds[1];
}

// discontinuities = {...} in the table at index: the x where the graph must not join, sorted into
// a userdata buffer left on the stack.
static void _plotDiscontinuities(lua_State* lua, int index, LuaPlotCut& cut)
{
    lua_getfield(lua, index, "discontinuities");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    if (!lua_istable(lua, -1)) luaL_error(lua, "discontinuities must be a list of numbers");
    auto size = lua_rawlen(lua, -1);
    if (size > LUA_POINT_LIMIT) {
        luaL_error(lua, "discontinuities must hold at most %d numbers", static_cast<int>(LUA_POINT_LIMIT));
    }
    auto count = static_cast<uint32_t>(size);
    auto breaks = static_cast<double*>(lua_newuserdatauv(lua, 2u * sizeof(double) * (count ? count : 1u), 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, -2, static_cast<lua_Integer>(i) + 1);
        auto number = 0;
        breaks[i] = lua_tonumberx(lua, -1, &number);
        if (!number || !std::isfinite(breaks[i])) luaL_error(lua, "discontinuities must be finite numbers");
        lua_pop(lua, 1);
    }
    stableSort(breaks, breaks + count, count, sizeof(double), [](const void* a, const void* b, const void*) {
        return *static_cast<const double*>(a) < *static_cast<const double*>(b);
    });
    // The buffer takes the place of the table, so it stays on the stack while the plot is read.
    lua_replace(lua, -2);
    cut.breaks = breaks;
    cut.breakCount = count;
    cut.broken = true;
}

// Pushes what cut cuts with as a LuaPlotRecord, with range when fn is sampled over one, for a plot
// that has held room points and endRoom pieces at most.
static void _plotKeep(lua_State* lua, const LuaPlotCut& cut, const Range* range, uint32_t room, uint32_t endRoom,
                      uint32_t sampleRoom = 0u, bool copied = false)
{
    auto size = sizeof(LuaPlotRecord) + sizeof(double) * cut.breakCount;
    auto record = new (lua_newuserdatauv(lua, size, 0)) LuaPlotRecord;
    if (range) {
        record->range = *range;
        record->sampled = true;
    }
    record->band = cut.band;
    record->lo = cut.lo;
    record->hi = cut.hi;
    record->broken = cut.broken;
    record->breakCount = cut.breakCount;
    record->room = room;
    record->endRoom = endRoom;
    record->sampleRoom = std::max(sampleRoom, cut.samples.count);
    record->copied = copied;
    if (cut.breakCount) std::memcpy(static_cast<void*>(record + 1), cut.breaks, sizeof(double) * cut.breakCount);
}

// Records what cut cuts with, and range when fn is sampled over one, for the plot it made, of the
// handle at index, so that the states that give it fn or points cut them the same way.
static void _plotRemember(lua_State* lua, int index, const LuaPlotCut& cut, const Range* range)
{
    index = lua_absindex(lua, index);
    _sceneTable(lua, PLOT_REGISTRY, index, true);
    lua_pushlightuserdata(lua, static_cast<LuaObject*>(lua_touserdata(lua, index))->object);
    _plotKeep(lua, cut, range, cut.count, cut.pieces > 1u ? cut.pieces : 0u);
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}

static int _plot(lua_State* lua)
{
    auto space = _factory(lua, {"points", "fn", "x_range", "y_range", "discontinuities", "endpoint_radius"});
    auto endpointRadius = _fieldNumber(lua, 2, "endpoint_radius", 0.0f);
    if (endpointRadius < 0.0f) return luaL_error(lua, "endpoint_radius must be zero or more");
    lua_settop(lua, 2);
    lua_getfield(lua, 2, "points");
    auto points = !lua_isnil(lua, 3);
    if (points && !lua_istable(lua, 3)) return luaL_error(lua, "points must be a table");
    lua_getfield(lua, 2, "fn");
    auto function = !lua_isnil(lua, 4);
    if (function && !lua_isfunction(lua, 4) && !lua_istable(lua, 4)) return luaL_error(lua, "fn must be a function");
    if (points && function) return luaL_error(lua, "plot accepts either points or fn, not both");
    if (!points && !function) return luaL_error(lua, "plot requires points or fn with x_range");
    LuaPlotCut cut;
    cut.lua = lua;
    cut.scene = space.scene;
    cut.fn = 4;
    _plotBand(lua, 2, cut);
    _plotDiscontinuities(lua, 2, cut);
    Range range;
    uint32_t samples;
    if (points) {
        auto size = lua_rawlen(lua, 3);
        if (size < 2u) return luaL_error(lua, "points must contain at least %d vectors", 2);
        if (size > UINT32_MAX || size > LUA_MEMORY_LIMIT / sizeof(Vec3)) return luaL_error(lua, "points array is too large");
        samples = static_cast<uint32_t>(size);
    } else {
        range = _requiredRange(lua, 2, "x_range", "plot fn");
        samples = _plotSamples(lua, range);
    }
    _plotCut(cut, points ? 3 : 0, range, samples);
    if (!cut.pieces) {
        if (cut.band) return luaL_error(lua, "plot has nothing to draw inside y_range");
        return luaL_error(lua, "plot has nothing to draw: it needs two defined values in a row");
    }
    auto bytes = static_cast<size_t>(cut.count) * sizeof(Vec3) + 512u;
    bytes += static_cast<size_t>(cut.samples.count) * sizeof(Vec3);
    if (cut.pieces > 1u) bytes += static_cast<size_t>(cut.pieces) * sizeof(uint32_t);
    _fit(lua, space.scene, {bytes, 0, cut.count});
    // The buffers stay on the stack below the handle until the plot has copied them.
    auto owner = _owner(lua, space.scene);
    auto plot = Plot::gen(cut.points, cut.count, cut.ends, cut.pieces, function ? &cut.samples : nullptr);
    if (plot) {
        plot->flat = cut.band || cut.broken;
        plot->endpointRadius = endpointRadius;
    }
    auto added = _add(lua, space, owner, plot, 2, "plot");
    if (function || cut.band || cut.broken) _plotRemember(lua, -1, cut, function ? &range : nullptr);
    return added;
}

// The steps that cover range (see Range::steps()), which must number from 1 to limit.
static uint32_t _steps(lua_State* lua, const Range& range, uint32_t limit, const char* name)
{
    auto steps = range.steps();
    if (steps < 1u || steps > limit) {
        luaL_error(lua, "%s must cover from 1 to %d steps", name, static_cast<int>(limit));
    }
    return steps;
}

// Calls the function at index with x, as a sampling callback of box, and returns its result,
// which must be a finite number.
static float _call(lua_State* lua, LuaScene* box, int index, float x, const char* name)
{
    lua_pushvalue(lua, index);
    lua_pushnumber(lua, x);
    if (_sampleCall(lua, box, 1, 1) != LUA_OK) lua_error(lua);
    auto y = _number(lua, -1, name);
    lua_pop(lua, 1);
    return y;
}

// A Lua function that a core call samples through _sampleFunction. Nothing may raise through
// the core, so the sampler catches what the function raises and leaves it, or a result that is
// not a finite number, on the stack, for _sampled to raise once the core call returns.
struct LuaFunction
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    int index = 0;
    const char* name = nullptr;
    bool raised = false;
    bool refused = false;
};

static bool _sampleFunction(float x, float& value, void* data) noexcept
{
    auto function = static_cast<LuaFunction*>(data);
    auto lua = function->lua;
    lua_pushvalue(lua, function->index);
    lua_pushnumber(lua, x);
    if (_sampleCall(lua, function->scene, 1, 1) != LUA_OK) {
        function->raised = true;
        return false;
    }
    int number = 0;
    auto result = lua_tonumberx(lua, -1, &number);
    if (!number || !std::isfinite(result) || result < -FLT_MAX || result > FLT_MAX) {
        function->refused = true;
        return false;
    }
    value = static_cast<float>(result);
    lua_pop(lua, 1);
    return true;
}

// Raises what stopped the function of a core call, as calling it directly would have raised it.
static void _sampled(lua_State* lua, const LuaFunction& function)
{
    if (function.raised) lua_error(lua);
    if (function.refused) _number(lua, -1, function.name);
}

// scene:area{fn = f, x_range = {a, b, step}, base = 0}: the region between the graph of fn and
// base, a number or a second function, sampled every step from a to b, as a polygon.
static int _area(lua_State* lua)
{
    auto space = _factory(lua, {"fn", "x_range", "base"});
    lua_getfield(lua, 2, "fn");
    if (!lua_isfunction(lua, -1)) return luaL_error(lua, "area requires fn, a function");
    auto fn = lua_gettop(lua);
    auto range = _requiredRange(lua, 2, "x_range", "area");
    lua_getfield(lua, 2, "base");
    auto base = lua_gettop(lua);
    auto curved = lua_isfunction(lua, base);
    if (!curved && !lua_isnil(lua, base) && lua_type(lua, base) != LUA_TNUMBER) {
        return luaL_error(lua, "base must be a number or a function");
    }
    auto level = curved || lua_isnil(lua, base) ? 0.0f : _number(lua, base, "base");
    auto steps = _steps(lua, range, LUA_POINT_LIMIT / 2u - 1u, "x_range");
    // The graph from a to b, then the base back from b to a: two corners, or a second graph.
    auto count = curved ? 2u * (steps + 1u) : steps + 3u;
    _fit(lua, space.scene, {count * sizeof(Vec3) + 512u, 0, count});
    auto points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * count, 0));
    for (auto i = 0u; i <= steps; i++) {
        auto x = range.stepAt(i);
        new (points + i) Vec3{x, _call(lua, space.scene, fn, x, "area fn result"), 0.0f};
    }
    for (auto i = steps + 1u; i < count; i++) {
        auto back = i - steps - 1u;
        auto x = range.stepAt(curved ? steps - back : (back ? 0u : steps));
        new (points + i) Vec3{x, curved ? _call(lua, space.scene, base, x, "area base result") : level, 0.0f};
    }
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Polygon::gen(points, count), 2, "area");
}

// scene:union{of = {a, b, ...}}, and likewise intersection, difference and exclusion: the region
// the closed shapes in of make together, folded from the left, as a path in the space it joins.
// The shapes stay as they are; the path is measured from where they stand as authored.
static int _region(lua_State* lua, PathOperation operation, const char* name)
{
    auto space = _factory(lua, {"of"});
    lua_getfield(lua, 2, "of");
    if (!lua_istable(lua, -1)) return luaL_error(lua, "%s requires of, a list of shapes", name);
    auto count = lua_rawlen(lua, -1);
    if (count < 2u || count > LUA_REGION_SHAPES) {
        return luaL_error(lua, "%s of takes from 2 to %d shapes", name, static_cast<int>(LUA_REGION_SHAPES));
    }
    const Object* shapes[LUA_REGION_SHAPES];
    for (auto i = 1u; i <= count; i++) {
        lua_rawgeti(lua, -1, static_cast<lua_Integer>(i));
        if (lua_type(lua, -1) != LUA_TUSERDATA) return luaL_error(lua, "%s of must list shape handles", name);
        shapes[i - 1u] = _object(lua, space.scene->scene, -1);
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    auto owner = _owner(lua, space.scene);
    Path* path = nullptr;
    switch (Path::combine(shapes, static_cast<uint32_t>(count), operation, space.parent, path)) {
        case Result::Success: break;
        case Result::InvalidArguments:
            return luaL_error(lua, "%s takes circles, rectangles, polygons and closed paths", name);
        case Result::InsufficientCondition: return luaL_error(lua, "the %s of these shapes is empty", name);
        case Result::NonSupport:
            return luaL_error(lua, "%s: the shapes have more than 8192 edges or crossings; simplify them", name);
        default: return luaL_error(lua, "could not create %s: out of memory", name);
    }
    return _add(lua, space, owner, path, 2, name);
}

static int _union(lua_State* lua)
{
    return _region(lua, PathOperation::Union, "union");
}

static int _intersection(lua_State* lua)
{
    return _region(lua, PathOperation::Intersection, "intersection");
}

static int _difference(lua_State* lua)
{
    return _region(lua, PathOperation::Difference, "difference");
}

static int _exclusion(lua_State* lua)
{
    return _region(lua, PathOperation::Exclusion, "exclusion");
}

// ---------- Riemann sums, contours and fields ----------

// The colors and the layer of a Riemann sum reach each of its rectangles.
static constexpr const char* RIEMANN_SHAPE_STYLES[] = {
    "color", "stroke", "fill", "gradient", "width", "dash", "dash_offset", "layer"};

// scene:riemann{fn = f, x_range = {a, b, dx}, sample = "left"}: the rectangles of a Riemann sum
// of fn, one for each step of x_range and as tall as fn at the left end, the right end, or the
// midpoint of its step, as a group whose children are the rectangles from left to right.
// values gives the heights instead, one per step.
static int _riemann(lua_State* lua)
{
    auto space = _factory(lua, {"fn", "values", "x_range", "sample", "negative", "id", "color", "stroke", "fill",
                                "gradient", "width", "dash", "dash_offset", "opacity", "progress", "layer", "isolate",
                                "mask", "blend", "blur", "shadow"}, false);
    auto range = _requiredRange(lua, 2, "x_range", "riemann");
    auto steps = _steps(lua, range, LUA_GROUP_LIMIT, "x_range");
    lua_getfield(lua, 2, "fn");
    auto fn = lua_gettop(lua);
    lua_getfield(lua, 2, "values");
    auto values = lua_gettop(lua);
    if (lua_isnil(lua, fn) == lua_isnil(lua, values)) return luaL_error(lua, "riemann requires either fn or values");
    if (!lua_isnil(lua, fn) && !lua_isfunction(lua, fn)) return luaL_error(lua, "fn must be a function");
    if (!lua_isnil(lua, values) && (!lua_istable(lua, values) || lua_rawlen(lua, values) != steps)) {
        return luaL_error(lua, "values must hold one height for each of the %d steps of x_range",
                          static_cast<int>(steps));
    }
    if (lua_isnil(lua, fn) && _has(lua, 2, "sample")) {
        return luaL_error(lua, "sample needs fn, since values are the heights themselves");
    }
    static constexpr LuaName<RiemannSample> SAMPLES[] = {
        {"left", RiemannSample::Left}, {"right", RiemannSample::Right}, {"midpoint", RiemannSample::Midpoint}};
    auto sample = _fieldEnum(lua, 2, "sample", RiemannSample::Left, SAMPLES);
    float* heights = nullptr;
    if (lua_isnil(lua, fn)) {
        heights = static_cast<float*>(lua_newuserdatauv(lua, sizeof(float) * steps, 0));
        for (auto i = 0u; i < steps; i++) {
            lua_rawgeti(lua, values, static_cast<lua_Integer>(i) + 1);
            if (lua_type(lua, -1) != LUA_TNUMBER) return luaL_error(lua, "values must be numbers");
            heights[i] = _number(lua, -1, "a value");
            lua_pop(lua, 1);
        }
    }

    auto owner = _owner(lua, space.scene);
    auto handle = lua_gettop(lua);
    LuaFunction function{lua, space.scene, fn, "riemann fn result"};
    Group* group = nullptr;
    Result status;
    if (heights) {
        status = Group::riemann(range, heights, steps, group);
    } else {
        luaL_checkstack(lua, 3, nullptr);
        status = Group::riemann(range, _sampleFunction, &function, sample, group);
    }
    owner->object = group;
    _sampled(lua, function);
    if (status != Result::Success) {
        return luaL_error(lua, "could not create riemann: %s",
                          status == Result::InvalidArguments ? "a step of x_range is too narrow to draw"
                                                             : _explain(status));
    }

    // The group takes the id, opacity, and layer options; its rectangles follow it in order.
    auto groupStyle = _pickOptions(lua, ROW_STYLES);
    _style(lua, space.scene->scene, group, groupStyle);
    auto groupId = _pushId(lua);
    auto negative = _has(lua, 2, "negative");
    for (auto i = 0u; i < steps; i++) {
        auto top = lua_gettop(lua);
        auto style = _pickOptions(lua, RIEMANN_SHAPE_STYLES);
        auto bar = static_cast<Rectangle*>(group->childAt(i));
        if (negative && bar->center.y < 0.0f) {
            lua_getfield(lua, 2, "negative");
            lua_setfield(lua, style, "fill");
        }
        _childId(lua, style, groupId, i + 1u);
        _style(lua, space.scene->scene, bar, style);
        lua_settop(lua, top);
    }
    lua_pushvalue(lua, handle);
    return _attach(lua, space, owner, group, groupStyle, "riemann");
}

// Default magnitude ramp: theme accent, success, warning and danger.
static constexpr ThemeColorRole FIELD_COLORS[] = {ThemeColorRole::Accent, ThemeColorRole::Success,
                                                  ThemeColorRole::Warning, ThemeColorRole::Danger};
static constexpr uint32_t LUA_FIELD_COLOR_LIMIT = 8;
// The colors and the layer of a field reach each of its arrows or lines.
static constexpr const char* FIELD_SHAPE_STYLES[] = {"color", "stroke", "width", "dash", "dash_offset", "layer"};

// Unless the options at 2 give a field a color or stroke, the colors its magnitudes ramp through,
// kept in stops: the colors option, a list of 2 to 8 colors, or the theme's field colors; and the
// magnitude option, {low, high}, which they spread over instead of the field's own range from
// zero to its greatest magnitude.
template<typename Options>
static void _fieldColors(lua_State* lua, const Theme& theme, Color (&stops)[LUA_FIELD_COLOR_LIMIT], Options& options)
{
    if (_has(lua, 2, "color") || _has(lua, 2, "stroke")) return;
    options.colors = stops;
    lua_getfield(lua, 2, "colors");
    if (lua_isnil(lua, -1)) {
        for (auto role : FIELD_COLORS) stops[options.colorCount++] = theme.color(role);
    } else {
        auto count = lua_istable(lua, -1) ? lua_rawlen(lua, -1) : 0u;
        if (count < 2u || count > LUA_FIELD_COLOR_LIMIT) {
            luaL_error(lua, "colors must be a list of 2 to %d colors", static_cast<int>(LUA_FIELD_COLOR_LIMIT));
        }
        for (auto i = 1u; i <= count; i++) {
            lua_rawgeti(lua, -1, static_cast<lua_Integer>(i));
            if (lua_type(lua, -1) != LUA_TSTRING) {
                luaL_error(lua, "colors must be a list of 2 to %d colors", static_cast<int>(LUA_FIELD_COLOR_LIMIT));
            }
            stops[options.colorCount++] = _color(lua, -1, "colors", &theme);
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);
    lua_getfield(lua, 2, "magnitude");
    if (!lua_isnil(lua, -1)) {
        auto range = _vec2(lua, -1, "magnitude");
        if (!(range.x < range.y)) luaL_error(lua, "magnitude must be {low, high} with low below high");
        options.low = range.x;
        options.high = range.y;
    }
    lua_pop(lua, 1);
}

// The samples of range (see Range::samples()), which must number from 1 to limit.
static uint32_t _samples(lua_State* lua, const Range& range, uint32_t limit, const char* name)
{
    auto samples = range.samples();
    if (samples < 1u || samples > limit) {
        luaL_error(lua, "%s must hold from 1 to %d samples", name, static_cast<int>(limit));
    }
    return samples;
}

static Colormap _colormapName(const char* name);

struct LuaContour
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    int fn = 0;
    bool raised = false;
    double x = 0.0;
    double y = 0.0;
};

// Errors wait until the core releases its tracing buffers, as for warp callbacks.
static bool _contourSample(double x, double y, double& value, void* data) noexcept
{
    auto context = static_cast<LuaContour*>(data);
    auto lua = context->lua;
    context->x = x;
    context->y = y;
    lua_pushvalue(lua, context->fn);
    lua_pushnumber(lua, x);
    lua_pushnumber(lua, y);
    if (_sampleCall(lua, context->scene, 2, 1) != LUA_OK) {
        context->raised = true;
        return false;
    }
    auto number = lua_type(lua, -1) == LUA_TNUMBER;
    value = lua_tonumber(lua, -1);
    lua_pop(lua, 1);
    return number && std::fabs(value) <= FLT_MAX;
}

// Raises what stopped the contour function, as calling it directly would have: what it raised,
// which names its own line, or a refused value at the line of the call.
static int _contourError(const LuaContour& context)
{
    if (context.raised) return lua_error(context.lua);
    return luaL_error(context.lua, "contour fn must return a finite number; at (%f, %f) it did not",
                      static_cast<lua_Number>(context.x), static_cast<lua_Number>(context.y));
}

static void _contourRemember(lua_State* lua, int index, const Range& xRange, const Range& yRange,
                             double level, uint32_t room, uint32_t endRoom)
{
    index = lua_absindex(lua, index);
    _sceneTable(lua, PLOT_REGISTRY, index, true);
    lua_pushlightuserdata(lua, static_cast<LuaObject*>(lua_touserdata(lua, index))->object);
    auto record = new (lua_newuserdatauv(lua, sizeof(LuaPlotRecord), 0)) LuaPlotRecord;
    record->range = xRange;
    record->contourY = yRange;
    record->level = level;
    record->contour = true;
    record->room = room;
    record->endRoom = endRoom;
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}

// One plot per level, with all its connected pieces, in the order levels gives.
static int _contour(lua_State* lua)
{
    auto space = _factory(lua, {"fn", "values", "levels", "x_range", "y_range", "colors", "id", "color", "stroke",
                                "width", "dash", "dash_offset", "opacity", "progress", "layer", "isolate", "mask",
                                "blend", "blur", "shadow", "labels"}, false);
    ContourLabelOptions labelOptions;
    lua_getfield(lua, 2, "labels");
    auto labels = !lua_isnil(lua, -1) && !(lua_isboolean(lua, -1) && !lua_toboolean(lua, -1));
    if (labels && !lua_isboolean(lua, -1)) {
        if (!lua_istable(lua, -1)) return luaL_error(lua, "labels must be a boolean or a table");
        auto config = lua_gettop(lua);
        _options(lua, config, {"format", "size", "spacing", "padding", "upright"}, false);
        labelOptions.format = _fieldOptionalString(lua, config, "format", "%.1f");
        if (std::strlen(labelOptions.format) != 4u || labelOptions.format[0] != '%'
            || labelOptions.format[1] != '.' || labelOptions.format[2] < '0' || labelOptions.format[2] > '6'
            || labelOptions.format[3] != 'f') return luaL_error(lua, "label format must be %%.0f through %%.6f");
        lua_pop(lua, 1);
        labelOptions.size = _fieldNumber(lua, config, "size", labelOptions.size);
        labelOptions.spacing = _fieldNumber(lua, config, "spacing", labelOptions.spacing);
        labelOptions.padding = _fieldNumber(lua, config, "padding", labelOptions.padding);
        labelOptions.upright = _fieldBool(lua, config, "upright", true);
        if (labelOptions.size <= 0.0f) return luaL_error(lua, "label size must be positive");
        if (labelOptions.spacing < 0.0f || labelOptions.padding < 0.0f) return luaL_error(lua, "label spacing and padding must be zero or more");
    }
    lua_pop(lua, 1);
    lua_getfield(lua, 2, "levels");
    auto levelTable = lua_gettop(lua);
    auto levelCount = lua_istable(lua, levelTable) ? lua_rawlen(lua, levelTable) : 0u;
    if (!levelCount || levelCount > LUA_GROUP_LIMIT) {
        return luaL_error(lua, "levels must hold from 1 to %d finite numbers", static_cast<int>(LUA_GROUP_LIMIT));
    }
    auto levels = static_cast<double*>(lua_newuserdatauv(lua, sizeof(double) * levelCount, 0));
    for (auto i = 0u; i < levelCount; i++) {
        lua_rawgeti(lua, levelTable, i + 1u);
        if (lua_type(lua, -1) != LUA_TNUMBER || !(std::fabs(lua_tonumber(lua, -1)) <= FLT_MAX)) {
            return luaL_error(lua, "levels must be finite numbers");
        }
        levels[i] = lua_tonumber(lua, -1);
        lua_pop(lua, 1);
    }
    auto xRange = _requiredRange(lua, 2, "x_range", "contour");
    auto yRange = _requiredRange(lua, 2, "y_range", "contour");
    auto columns = _samples(lua, xRange, 65536u, "x_range");
    auto rows = _samples(lua, yRange, 65536u, "y_range");
    if (columns < 2u || rows < 2u) return luaL_error(lua, "contour needs at least two samples along each range");
    if (static_cast<uint64_t>(columns) * rows > 65536u) {
        return luaL_error(lua, "a contour grid holds at most 65536 samples; widen the steps of x_range and y_range");
    }
    auto count = columns * rows;
    for (auto range : {xRange, yRange}) {
        auto n = _samples(lua, range, 65536u, "range");
        for (auto i = 1u; i < n; i++) {
            if (range.sampleAt(i) <= range.sampleAt(i - 1u)) {
                return luaL_error(lua, "contour grid samples must have distinct coordinates");
            }
        }
    }
    if (static_cast<uint64_t>(count) * levelCount > LUA_POINT_LIMIT) {
        return luaL_error(lua, "contour samples times levels must be at most %d", static_cast<int>(LUA_POINT_LIMIT));
    }
    _fit(lua, space.scene, {0, 0, count});
    lua_getfield(lua, 2, "fn");
    auto fn = lua_gettop(lua);
    lua_getfield(lua, 2, "values");
    auto table = lua_gettop(lua);
    auto sampled = lua_isnil(lua, fn);
    if (sampled == static_cast<bool>(lua_isnil(lua, table))) return luaL_error(lua, "contour requires either fn or values");
    if (!sampled && !lua_isfunction(lua, fn)) return luaL_error(lua, "fn must be a function");
    if (sampled && (!lua_istable(lua, table) || lua_rawlen(lua, table) != count)) {
        return luaL_error(lua, "values must hold one finite number for each of the %d samples of x_range and y_range",
                          static_cast<int>(count));
    }
    auto& theme = space.scene->scene->theme();
    lua_getfield(lua, 2, "colors");
    auto colorTable = lua_gettop(lua);
    auto map = Colormap::None;
    auto colored = !lua_isnil(lua, colorTable);
    if (lua_type(lua, colorTable) == LUA_TSTRING) {
        map = _colormapName(lua_tostring(lua, colorTable));
        if (map == Colormap::None) return luaL_error(lua, "contour colors must name 'viridis', 'plasma', 'inferno', 'magma' or 'cividis'");
    } else if (colored && (!lua_istable(lua, colorTable) || lua_rawlen(lua, colorTable) != levelCount)) {
        return luaL_error(lua, "contour colors must hold one color per level or name a colormap");
    }
    auto colors = static_cast<Color*>(lua_newuserdatauv(lua, sizeof(Color) * levelCount, 0));
    auto low = *std::min_element(levels, levels + levelCount), high = *std::max_element(levels, levels + levelCount);
    for (auto i = 0u; colored && i < levelCount; i++) {
        if (map != Colormap::None) colors[i] = colormapColor(map, high > low ? static_cast<float>((levels[i] - low) / (high - low)) : 0.5f);
        else {
            lua_rawgeti(lua, colorTable, i + 1u);
            colors[i] = _color(lua, -1, "contour colors", &theme);
            lua_pop(lua, 1);
        }
    }
    auto values = static_cast<double*>(lua_newuserdatauv(lua, sizeof(double) * count, 0));
    LuaContour context{lua, space.scene, fn};
    for (auto i = 0u; i < count; i++) {
        auto x = xRange.sampleAt(i % columns), y = yRange.sampleAt(i / columns);
        if (sampled) {
            lua_rawgeti(lua, table, i + 1u);
            if (lua_type(lua, -1) != LUA_TNUMBER || !(std::fabs(lua_tonumber(lua, -1)) <= FLT_MAX)) {
                return luaL_error(lua, "contour values must be finite numbers; at (%f, %f) one was not",
                                  static_cast<lua_Number>(x), static_cast<lua_Number>(y));
            }
            values[i] = lua_tonumber(lua, -1);
            lua_pop(lua, 1);
        } else if (!_contourSample(x, y, values[i], &context)) return _contourError(context);
    }
    // Grid samples and retained vertices share the point budget; callbacks use the Lua hook.
    _spend(space.scene, {0, 0, count});
    auto groupStyle = _pickOptions(lua, ROW_STYLES);
    auto groupId = _pushId(lua);
    auto groupOwner = _owner(lua, space.scene);
    auto group = Group::gen();
    _add(lua, space, groupOwner, group, groupStyle, "contour");
    auto handle = lua_gettop(lua);
    LuaSpace inside{space.scene, group};
    for (auto i = 0u; i < levelCount; i++) {
        lua_createtable(lua, 0, 7);
        auto style = lua_gettop(lua);
        for (auto name : FIELD_SHAPE_STYLES) {
            if (colored && (std::strcmp(name, "color") == 0 || std::strcmp(name, "stroke") == 0)) continue;
            lua_getfield(lua, 2, name);
            lua_setfield(lua, style, name);
        }
        _childId(lua, style, groupId, i + 1u);
        auto owner = _owner(lua, space.scene);
        Plot* plot = nullptr;
        auto result = Plot::contour(xRange, yRange, values, count, levels[i], plot,
                                    sampled ? nullptr : _contourSample, &context, _room(space.scene).points);
        if (context.raised) return _contourError(context);
        if (result == Result::NonSupport) return luaL_error(lua, "scene point limit exceeded");
        if (result == Result::InvalidArguments && !sampled) return _contourError(context);
        owner->object = plot;
        if (result != Result::Success) return luaL_error(lua, "could not trace contour: %s", _explain(result));
        if (colored) plot->stroke(colors[i]);
        _style(lua, space.scene->scene, plot, style);
        if (labels) {
            result = plot->labelContours(labelOptions);
            if (result != Result::Success) return luaL_error(lua, "could not label contour: %s", _explain(result));
        }
        _attach(lua, inside, owner, plot, style, "contour level");
        _contourRemember(lua, -1, xRange, yRange, levels[i], plot->count(),
                          plot->contours() > 1u ? plot->contours() : 0u);
        lua_settop(lua, handle);
    }
    return 1;
}

// A vector a field function gives, kept in double precision while a point is carried.
struct LuaFieldVector
{
    double x = 0.0;
    double y = 0.0;
};

// Calls the field function at index with x and y, as a sampling callback of box, and reads the
// vector it gives, two numbers or a list {x, y}; returns whether both are finite.
static bool _fieldCall(lua_State* lua, LuaScene* box, int index, double x, double y, const char* name,
                       LuaFieldVector& output)
{
    lua_pushvalue(lua, index);
    lua_pushnumber(lua, x);
    lua_pushnumber(lua, y);
    if (_sampleCall(lua, box, 2, 2) != LUA_OK) lua_error(lua);
    auto listed = lua_istable(lua, -2);
    if (listed) {
        lua_rawgeti(lua, -2, 1);
        lua_rawgeti(lua, -3, 2);
    }
    auto numbers = lua_type(lua, -2) == LUA_TNUMBER && lua_type(lua, -1) == LUA_TNUMBER;
    output = {lua_tonumber(lua, -2), lua_tonumber(lua, -1)};
    lua_pop(lua, listed ? 4 : 2);
    if (!numbers) {
        luaL_error(lua, "%s must return two finite numbers or {x, y}; at (%f, %f) it did not", name,
                   static_cast<lua_Number>(x), static_cast<lua_Number>(y));
    }
    return std::fabs(output.x) <= FLT_MAX && std::fabs(output.y) <= FLT_MAX;
}

// One step of the classic Runge-Kutta method along the field function at fn from x, y by h,
// giving the field where the step starts at start; false where the field is not finite.
static bool _rungeKutta(lua_State* lua, LuaScene* box, int fn, double& x, double& y, double h, const char* name,
                        LuaFieldVector& start)
{
    LuaFieldVector k2, k3, k4;
    if (!_fieldCall(lua, box, fn, x, y, name, start)) return false;
    if (!_fieldCall(lua, box, fn, x + 0.5 * h * start.x, y + 0.5 * h * start.y, name, k2)) return false;
    if (!_fieldCall(lua, box, fn, x + 0.5 * h * k2.x, y + 0.5 * h * k2.y, name, k3)) return false;
    if (!_fieldCall(lua, box, fn, x + h * k3.x, y + h * k3.y, name, k4)) return false;
    auto nextX = x + h / 6.0 * (start.x + 2.0 * k2.x + 2.0 * k3.x + k4.x);
    auto nextY = y + h / 6.0 * (start.y + 2.0 * k2.y + 2.0 * k3.y + k4.y);
    if (!(std::fabs(nextX) <= FLT_MAX) || !(std::fabs(nextY) <= FLT_MAX)) return false;
    x = nextX;
    y = nextY;
    return true;
}

// A Lua field function that a core call samples through _sampleField, as LuaFunction is one of
// x. A vector field needs every vector finite, while a stream line ends where one is not.
struct LuaField
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    int index = 0;
    const char* name = nullptr;
    bool finite = true;
    bool raised = false;
    bool refused = false;
    double x = 0.0;
    double y = 0.0;
};

static bool _sampleField(double x, double y, double& u, double& v, void* data) noexcept
{
    auto field = static_cast<LuaField*>(data);
    auto lua = field->lua;
    lua_pushvalue(lua, field->index);
    lua_pushnumber(lua, x);
    lua_pushnumber(lua, y);
    if (_sampleCall(lua, field->scene, 2, 2) != LUA_OK) {
        field->raised = true;
        return false;
    }
    auto listed = lua_istable(lua, -2);
    if (listed) {
        lua_rawgeti(lua, -2, 1);
        lua_rawgeti(lua, -3, 2);
    }
    auto numbers = lua_type(lua, -2) == LUA_TNUMBER && lua_type(lua, -1) == LUA_TNUMBER;
    u = lua_tonumber(lua, -2);
    v = lua_tonumber(lua, -1);
    lua_pop(lua, listed ? 4 : 2);
    if (!numbers || (field->finite && !(std::fabs(u) <= FLT_MAX && std::fabs(v) <= FLT_MAX))) {
        field->refused = true;
        field->x = x;
        field->y = y;
        return false;
    }
    return true;
}

// Raises what stopped the field function of a core call, as calling it directly would have.
static void _sampledField(lua_State* lua, const LuaField& field)
{
    if (field.raised) lua_error(lua);
    if (field.refused) {
        luaL_error(lua, "%s must return two finite numbers or {x, y}; at (%f, %f) it did not", field.name,
                   static_cast<lua_Number>(field.x), static_cast<lua_Number>(field.y));
    }
}

// Attaches a field's group, whose owner is the handle at handle. The group takes the id, the
// opacity, the layer and the other group styles of the options at 2; its children, in order,
// take the colors, the width, the dashes and the layer, and below an id of the group, the id
// <id>/<column>/<row> of their sample in a grid of columns, or <id>/<seed> without one.
static int _attachField(lua_State* lua, const LuaSpace& space, LuaObject* owner, int handle, Group* group,
                        const uint32_t* indices, uint32_t columns, const char* name)
{
    auto scene = space.scene->scene;
    auto groupStyle = _pickOptions(lua, ROW_STYLES);
    _style(lua, scene, group, groupStyle);
    auto groupId = _pushId(lua);
    for (auto i = 0u; i < group->childCount(); i++) {
        auto top = lua_gettop(lua);
        auto style = _pickOptions(lua, FIELD_SHAPE_STYLES);
        if (columns) _childId(lua, style, groupId, indices[i] % columns + 1u, indices[i] / columns + 1u);
        else _childId(lua, style, groupId, indices[i] + 1u);
        _style(lua, scene, group->childAt(i), style);
        lua_settop(lua, top);
    }
    lua_pushvalue(lua, handle);
    return _attach(lua, space, owner, group, groupStyle, name);
}

// Builds row-major arrows from a field callback or explicit vectors, bottom-left first.
// Lengths saturate or scale as configured; colors follow magnitude.
static int _vectorField(lua_State* lua)
{
    auto space = _factory(lua, {"fn", "values", "x_range", "y_range", "length", "scale", "anchor", "colors",
                                "magnitude", "tip", "id", "color", "stroke", "width", "dash", "dash_offset", "opacity",
                                "progress", "layer", "isolate", "mask", "blend", "blur", "shadow"}, false);
    auto xRange = _requiredRange(lua, 2, "x_range", "vector_field");
    auto yRange = _requiredRange(lua, 2, "y_range", "vector_field");
    auto columns = _samples(lua, xRange, LUA_GROUP_LIMIT, "x_range");
    auto rows = _samples(lua, yRange, LUA_GROUP_LIMIT, "y_range");
    if (static_cast<uint64_t>(columns) * rows > LUA_GROUP_LIMIT) {
        return luaL_error(lua, "a vector field holds at most %d arrows; widen the steps of x_range and y_range",
                          static_cast<int>(LUA_GROUP_LIMIT));
    }
    auto count = columns * rows;
    lua_getfield(lua, 2, "fn");
    auto fn = lua_gettop(lua);
    lua_getfield(lua, 2, "values");
    auto values = lua_gettop(lua);
    if (lua_isnil(lua, fn) == lua_isnil(lua, values)) {
        return luaL_error(lua, "vector_field requires either fn or values");
    }
    if (!lua_isnil(lua, fn) && !lua_isfunction(lua, fn)) return luaL_error(lua, "fn must be a function");
    if (!lua_isnil(lua, values) && (!lua_istable(lua, values) || lua_rawlen(lua, values) != count)) {
        return luaL_error(lua, "values must hold one {x, y} for each of the %d samples of x_range and y_range",
                          static_cast<int>(count));
    }
    // Arrows approach length as their vectors grow, a little under the smaller step by default;
    // scale draws every vector at that multiple of its own length instead.
    if (_has(lua, 2, "length") && _has(lua, 2, "scale")) {
        return luaL_error(lua, "length and scale cannot be used together");
    }
    VectorFieldOptions options;
    auto length = _fieldNumber(lua, 2, "length", 0.8f * std::fmin(xRange.step, yRange.step));
    if (!(length > 0.0f)) return luaL_error(lua, "length must be a positive number of world units");
    auto scale = _fieldNumber(lua, 2, "scale", 1.0f);
    if (!(scale > 0.0f)) return luaL_error(lua, "scale must be a positive number");
    if (_has(lua, 2, "scale")) options.scale = scale;
    else options.length = length;
    static constexpr LuaName<bool> ANCHORS[] = {{"tail", false}, {"middle", true}};
    options.centered = _fieldEnum(lua, 2, "anchor", false, ANCHORS);
    options.tip = _fieldNumber(lua, 2, "tip", 10.0f);
    if (options.tip < 0.0f) return luaL_error(lua, "tip must be zero or more pixels");
    Color stops[LUA_FIELD_COLOR_LIMIT];
    _fieldColors(lua, space.scene->scene->theme(), stops, options);
    Vec2* vectors = nullptr;
    if (lua_isnil(lua, fn)) {
        vectors = static_cast<Vec2*>(lua_newuserdatauv(lua, sizeof(Vec2) * count, 0));
        for (auto k = 0u; k < count; k++) {
            lua_rawgeti(lua, values, static_cast<lua_Integer>(k) + 1);
            if (!lua_istable(lua, -1)) return luaL_error(lua, "values must be {x, y} vectors");
            vectors[k] = _vec2(lua, -1, "a value");
            lua_pop(lua, 1);
        }
    }
    auto indices = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * count, 0));

    auto owner = _owner(lua, space.scene);
    auto handle = lua_gettop(lua);
    LuaField field{lua, space.scene, fn, "vector_field fn"};
    Group* group = nullptr;
    Result status;
    if (vectors) {
        status = Group::vectorField(xRange, yRange, vectors, count, group, options, indices);
    } else {
        luaL_checkstack(lua, 4, nullptr);
        status = Group::vectorField(xRange, yRange, _sampleField, &field, group, options, indices);
    }
    owner->object = group;
    _sampledField(lua, field);
    if (status != Result::Success) return luaL_error(lua, "could not create vector field: %s", _explain(status));
    return _attachField(lua, space, owner, handle, group, indices, columns, "vector field");
}

static constexpr uint32_t LUA_STREAM_STEP_LIMIT = 4096;

// Builds bounded Runge-Kutta streamlines from grid or explicit seeds, in seed order.
// Each line uses its mean field speed for coloring.
static int _streamLines(lua_State* lua)
{
    auto space = _factory(lua, {"fn", "x_range", "y_range", "seeds", "time", "dt", "direction", "colors", "magnitude",
                                "id", "color", "stroke", "width", "dash", "dash_offset", "opacity", "progress", "layer",
                                "isolate", "mask", "blend", "blur", "shadow"}, false);
    lua_getfield(lua, 2, "fn");
    if (!lua_isfunction(lua, -1)) return luaL_error(lua, "stream_lines requires fn, a function of x and y");
    auto fn = lua_gettop(lua);
    auto ranged = _has(lua, 2, "x_range") || _has(lua, 2, "y_range");
    if (ranged && (!_has(lua, 2, "x_range") || !_has(lua, 2, "y_range"))) {
        return luaL_error(lua, "stream_lines takes both x_range and y_range, or neither");
    }
    Range xRange;
    Range yRange;
    uint32_t columns = 0;
    uint32_t rows = 0;
    if (ranged) {
        xRange = _requiredRange(lua, 2, "x_range", "stream_lines");
        yRange = _requiredRange(lua, 2, "y_range", "stream_lines");
        columns = _samples(lua, xRange, LUA_GROUP_LIMIT, "x_range");
        rows = _samples(lua, yRange, LUA_GROUP_LIMIT, "y_range");
    }
    lua_getfield(lua, 2, "seeds");
    auto seeds = lua_gettop(lua);
    auto seeded = !lua_isnil(lua, seeds);
    if (!seeded && !ranged) return luaL_error(lua, "stream_lines requires seeds, or x_range and y_range to seed a grid");
    if (seeded && (!lua_istable(lua, seeds) || !lua_rawlen(lua, seeds))) {
        return luaL_error(lua, "seeds must be a list of {x, y} points");
    }
    auto count = seeded ? static_cast<uint64_t>(lua_rawlen(lua, seeds)) : static_cast<uint64_t>(columns) * rows;
    if (count > LUA_GROUP_LIMIT) {
        return luaL_error(lua, seeded ? "stream_lines draws at most %d lines"
                                      : "stream_lines draws at most %d lines; widen the steps of x_range and y_range",
                          static_cast<int>(LUA_GROUP_LIMIT));
    }
    StreamLineOptions options;
    options.time = _fieldNumber(lua, 2, "time", options.time);
    if (!(options.time > 0.0f)) return luaL_error(lua, "time must be a positive number");
    options.dt = _fieldNumber(lua, 2, "dt", options.dt);
    if (!(options.dt > 0.0f)) return luaL_error(lua, "dt must be a positive number");
    auto steps = options.steps();
    if (!steps || steps > LUA_STREAM_STEP_LIMIT) {
        return luaL_error(lua, "time / dt must be at most %d steps", static_cast<int>(LUA_STREAM_STEP_LIMIT));
    }
    static constexpr LuaName<StreamDirection> DIRECTIONS[] = {{"forward", StreamDirection::Forward},
                                                              {"backward", StreamDirection::Backward},
                                                              {"both", StreamDirection::Both}};
    options.direction = _fieldEnum(lua, 2, "direction", StreamDirection::Forward, DIRECTIONS);
    Color stops[LUA_FIELD_COLOR_LIMIT];
    _fieldColors(lua, space.scene->scene->theme(), stops, options);
    Vec2* points = nullptr;
    if (seeded) {
        points = static_cast<Vec2*>(lua_newuserdatauv(lua, sizeof(Vec2) * count, 0));
        for (auto k = 0u; k < count; k++) {
            lua_rawgeti(lua, seeds, static_cast<lua_Integer>(k) + 1);
            if (!lua_istable(lua, -1)) return luaL_error(lua, "seeds must be a list of {x, y} points");
            points[k] = _vec2(lua, -1, "a seed");
            lua_pop(lua, 1);
        }
    }
    auto indices = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * count, 0));

    auto owner = _owner(lua, space.scene);
    auto handle = lua_gettop(lua);
    // A vector that is not finite ends a line rather than the call.
    LuaField field{lua, space.scene, fn, "stream_lines fn", false};
    Group* group = nullptr;
    Result status;
    luaL_checkstack(lua, 4, nullptr);
    if (seeded) {
        Bounds region;
        region.min = {xRange.min, yRange.min, 0.0f};
        region.max = {xRange.max, yRange.max, 0.0f};
        status = Group::streamLines(points, static_cast<uint32_t>(count), ranged ? &region : nullptr, _sampleField,
                                    &field, group, options, indices);
    } else {
        status = Group::streamLines(xRange, yRange, _sampleField, &field, group, options, indices);
    }
    owner->object = group;
    _sampledField(lua, field);
    if (status == Result::InsufficientCondition) return luaL_error(lua, "scene point limit exceeded");
    if (status != Result::Success) return luaL_error(lua, "could not create stream lines: %s", _explain(status));
    return _attachField(lua, space, owner, handle, group, indices, 0u, "stream lines");
}

// ---------- Routes, paths, curves and surfaces ----------

static int _route(lua_State* lua)
{
    auto space = _factory(lua, {"points", "tail", "tip"});
    lua_getfield(lua, 2, "points");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "missing field 'points'");
    uint32_t count;
    auto points = _points(lua, -1, 2, count);
    _fit(lua, space.scene, {count * sizeof(Vec3) + 512u, 0, count});
    auto tail = _fieldNumber(lua, 2, "tail", 0.0f);
    auto tip = _fieldNumber(lua, 2, "tip", 16.0f);
    if (tail < 0.0f || tip < 0.0f) {
        return luaL_error(lua, "tail and tip must be non-negative");
    }
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, DirectedRoute::gen(points, count));
    if (object) {
        object->tail = tail;
        object->tip = tip;
    }
    return _add(lua, space, owner, object, 2, "route");
}

static uint32_t _pathSamples(lua_State* lua, int index, uint32_t fallback)
{
    auto samples = _fieldInteger(lua, index, "samples", fallback);
    if (samples <= 0 || samples > LUA_PATH_SAMPLE_LIMIT) {
        luaL_error(lua, "samples must be an integer from 1 to %d",
                   static_cast<int>(LUA_PATH_SAMPLE_LIMIT));
    }
    return static_cast<uint32_t>(samples);
}

static int _path(lua_State* lua)
{
    auto space = _factory(lua, {"commands", "samples"});
    auto samples = _pathSamples(lua, 2, 24u);
    lua_getfield(lua, 2, "commands");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "missing field 'commands'");
    _checkTable(lua, -1, "commands");
    auto table = lua_absindex(lua, -1);
    auto size = lua_rawlen(lua, table);
    if (size < 2u || size > UINT32_MAX || size > LUA_POINT_LIMIT) {
        return luaL_error(lua, "commands must contain 2 to %d entries",
                          static_cast<int>(LUA_POINT_LIMIT));
    }
    auto count = static_cast<uint32_t>(size);
    auto commands = static_cast<PathCommand*>(lua_newuserdatauv(lua, sizeof(PathCommand) * size, 0));
    static constexpr LuaName<PathVerb> VERBS[] = {{"move", PathVerb::Move}, {"line", PathVerb::Line},
                                                  {"quadratic", PathVerb::Quadratic}, {"cubic", PathVerb::Cubic},
                                                  {"close", PathVerb::Close}};
    auto points = 0u;
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, table, i + 1u);
        _checkTable(lua, -1, "a path command");
        auto item = lua_absindex(lua, -1);
        size_t size;
        auto name = _fieldString(lua, item, "type", &size);
        auto found = _findName(name, size, VERBS);
        if (!found) return luaL_error(lua, "unknown path command '%s'", name);
        auto verb = found->value;
        lua_pop(lua, 1);

        new (commands + i) PathCommand;
        commands[i].verb = verb;
        switch (verb) {
            case PathVerb::Move:
            case PathVerb::Line: {
                _options(lua, item, {"type", "to"}, false);
                commands[i].to = _fieldWorld(lua, item, "to");
                points++;
                break;
            }
            case PathVerb::Quadratic: {
                _options(lua, item, {"type", "control", "to"}, false);
                commands[i].control1 = _fieldWorld(lua, item, "control");
                commands[i].to = _fieldWorld(lua, item, "to");
                points += samples;
                break;
            }
            case PathVerb::Cubic: {
                _options(lua, item, {"type", "control1", "control2", "to"}, false);
                commands[i].control1 = _fieldWorld(lua, item, "control1");
                commands[i].control2 = _fieldWorld(lua, item, "control2");
                commands[i].to = _fieldWorld(lua, item, "to");
                points += samples;
                break;
            }
            case PathVerb::Close: {
                _options(lua, item, {"type"}, false);
                break;
            }
        }
        lua_pop(lua, 1);
        _fit(lua, space.scene, {0, 0, points});
    }
    if (commands[0].verb != PathVerb::Move) return luaL_error(lua, "path must begin with move");
    // Each move starts a contour; a contour needs two points, three if it closes, and the
    // contours close all or none.
    auto contourPoints = uint64_t{0};
    auto contours = 0u;
    auto closedContours = 0u;
    auto drawing = false;
    for (auto i = 0u; i < count; i++) {
        auto verb = commands[i].verb;
        if (verb != PathVerb::Move && !drawing) {
            return luaL_error(lua, "a path command after close must be move, starting the next contour");
        }
        if (verb == PathVerb::Move && drawing && contourPoints < 2u) {
            return luaL_error(lua, "every path contour needs at least two points, three if it closes");
        }
        switch (verb) {
            case PathVerb::Move:
                contours++;
                contourPoints = 1u;
                drawing = true;
                break;
            case PathVerb::Line: contourPoints++; break;
            case PathVerb::Quadratic:
            case PathVerb::Cubic: contourPoints += samples; break;
            case PathVerb::Close:
                if (contourPoints < 3u) {
                    return luaL_error(lua, "every path contour needs at least two points, three if it closes");
                }
                closedContours++;
                drawing = false;
                break;
        }
    }
    if (drawing && contourPoints < 2u) {
        return luaL_error(lua, "every path contour needs at least two points, three if it closes");
    }
    if (closedContours && closedContours != contours) {
        return luaL_error(lua, "the contours of a path must all close or all stay open");
    }
    _fit(lua, space.scene, {points * sizeof(Vec3) + 512u});
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Path::gen(commands, count, samples), 2, "path");
}

static int _curveGen(lua_State* lua)
{
    auto space = _factory(lua, {"from", "control1", "control2", "to", "samples"});
    auto samples = _pathSamples(lua, 2, 32u);
    auto points = samples + 1u;
    _fit(lua, space.scene, {points * sizeof(Vec3) + 512u, 0, points});
    auto from = _fieldWorld(lua, 2, "from");
    auto control1 = _fieldWorld(lua, 2, "control1");
    auto control2 = _fieldWorld(lua, 2, "control2");
    auto to = _fieldWorld(lua, 2, "to");
    auto owner = _owner(lua, space.scene);
    return _add(lua, space, owner, Curve::gen(from, control1, control2, to, samples), 2, "curve");
}

// The colormap a name gives, or Colormap::None.
static Colormap _colormapName(const char* name)
{
    static constexpr LuaName<Colormap> MAPS[] = {{"viridis", Colormap::Viridis}, {"plasma", Colormap::Plasma},
                                                 {"inferno", Colormap::Inferno}, {"magma", Colormap::Magma},
                                                 {"cividis", Colormap::Cividis}};
    auto found = _findName(name, std::strlen(name), MAPS);
    return found ? found->value : Colormap::None;
}

static Colormap _colormap(lua_State* lua, int index)
{
    auto map = lua_type(lua, index) == LUA_TSTRING ? _colormapName(lua_tostring(lua, index))
                                                   : Colormap::None;
    if (map == Colormap::None) {
        luaL_error(lua, "surface colors must name a colormap: 'viridis', 'plasma', 'inferno', "
                        "'magma' or 'cividis'");
    }
    return map;
}

// The range of a colormap that runs from the lowest of points along its axis to the highest, a
// flat surface taking the middle of the map.
static void _colormapExtent(SurfaceColormap& colormap, const Vec3* points, uint32_t count)
{
    auto low = static_cast<double>(points[0].dot(colormap.axis));
    auto high = low;
    for (auto i = 1u; i < count; i++) {
        auto value = static_cast<double>(points[i].dot(colormap.axis));
        low = std::min(low, value);
        high = std::max(high, value);
    }
    if (high - low <= 1.0e-6 * std::max({1.0, std::fabs(low), std::fabs(high)})) {
        low -= 1.0;
        high += 1.0;
    }
    colormap.min = static_cast<float>(low);
    colormap.max = static_cast<float>(high);
}

// colors = {map = "viridis", range = {low, high}, axis = "z"} at index: the map, the heights its
// ends sit at (the extent of points by default), and the direction heights run along.
static SurfaceColormap _colormapTable(lua_State* lua, int index, const Vec3* points,
                                      uint32_t count)
{
    index = lua_absindex(lua, index);
    _options(lua, index, {"map", "range", "axis"}, false);
    SurfaceColormap colormap;
    lua_getfield(lua, index, "map");
    colormap.map = _colormap(lua, -1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "axis");
    if (lua_type(lua, -1) == LUA_TSTRING) {
        auto name = lua_tostring(lua, -1);
        if (std::strcmp(name, "x") == 0) colormap.axis = {1.0f, 0.0f, 0.0f};
        else if (std::strcmp(name, "y") == 0) colormap.axis = {0.0f, 1.0f, 0.0f};
        else if (std::strcmp(name, "z") != 0) {
            luaL_error(lua, "surface colors axis must be 'x', 'y', 'z' or a vector");
        }
    } else if (lua_istable(lua, -1)) {
        auto axis = _world(lua, -1, "surface colors axis");
        auto length = std::sqrt(static_cast<double>(axis.x) * axis.x
                                + static_cast<double>(axis.y) * axis.y
                                + static_cast<double>(axis.z) * axis.z);
        if (!(length > 1.0e-12) || !std::isfinite(length)) {
            luaL_error(lua, "surface colors axis must not be zero");
        }
        colormap.axis = {static_cast<float>(axis.x / length), static_cast<float>(axis.y / length),
                         static_cast<float>(axis.z / length)};
    } else if (!lua_isnil(lua, -1)) {
        luaL_error(lua, "surface colors axis must be 'x', 'y', 'z' or a vector");
    }
    lua_pop(lua, 1);
    _colormapExtent(colormap, points, count);
    lua_getfield(lua, index, "range");
    if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2u) {
            luaL_error(lua, "surface colors range must be {low, high}");
        }
        lua_rawgeti(lua, -1, 1);
        colormap.min = _number(lua, -1, "surface colors range");
        lua_rawgeti(lua, -2, 2);
        colormap.max = _number(lua, -1, "surface colors range");
        lua_pop(lua, 2);
        auto span = static_cast<double>(colormap.max) - colormap.min;
        if (span == 0.0 || !std::isfinite(static_cast<float>(span))) {
            luaL_error(lua, "surface colors range must have two different ends");
        }
    }
    lua_pop(lua, 1);
    return colormap;
}

// The color the color function of a surface at index gives the point, called as a sampling
// callback of box.
static Color _callColors(lua_State* lua, LuaScene* box, int index, const Vec3& point)
{
    lua_pushvalue(lua, index);
    lua_pushnumber(lua, point.x);
    lua_pushnumber(lua, point.y);
    lua_pushnumber(lua, point.z);
    if (_sampleCall(lua, box, 3, 1) != LUA_OK) lua_error(lua);
    size_t size = 0;
    auto value = lua_type(lua, -1) == LUA_TSTRING ? lua_tolstring(lua, -1, &size) : nullptr;
    Color color;
    if (!value || !_colorName(value, size, &box->scene->theme(), color)) {
        luaL_error(lua, "surface colors must return a hex color or Theme color role; at (%f, %f, %f) it did not",
                   static_cast<lua_Number>(point.x), static_cast<lua_Number>(point.y),
                   static_cast<lua_Number>(point.z));
    }
    lua_pop(lua, 1);
    return color;
}

// The registry table that keeps the color function of each surface, keyed by the surface, for
// the morphs that call it again at the points between two surfaces.
static constexpr auto SURFACE_COLORS = "vis.surface.colors";

// Keeps the value at index as the color function of surface, or forgets the one kept for an
// earlier surface at the same address when it is not a function.
static void _keepColors(lua_State* lua, int index, const SurfaceMesh* surface)
{
    index = lua_absindex(lua, index);
    auto function = lua_isfunction(lua, index);
    if (lua_getfield(lua, LUA_REGISTRYINDEX, SURFACE_COLORS) != LUA_TTABLE) {
        lua_pop(lua, 1);
        if (!function) return;
        lua_newtable(lua);
        lua_pushvalue(lua, -1);
        lua_setfield(lua, LUA_REGISTRYINDEX, SURFACE_COLORS);
    }
    if (function) lua_pushvalue(lua, index);
    else lua_pushnil(lua);
    lua_rawsetp(lua, -2, surface);
    lua_pop(lua, 1);
}

// The colors option of a surface at index, for the count points: a colormap's name, which runs
// over the heights of the points; a table naming the map with its range and axis; a function of
// a point that returns its color; or a list of one color per point. The last two leave a
// userdata holding a color for each point on the stack and return it; a colormap returns nullptr
// and goes to colormap.
static Color* _surfaceColors(lua_State* lua, LuaScene* box, int index, const Vec3* points, uint32_t count,
                             SurfaceColormap& colormap)
{
    index = lua_absindex(lua, index);
    if (lua_type(lua, index) == LUA_TSTRING) {
        colormap.map = _colormap(lua, index);
        _colormapExtent(colormap, points, count);
        return nullptr;
    }
    auto function = lua_isfunction(lua, index);
    if (!function && !lua_istable(lua, index)) {
        luaL_error(lua, "surface colors must be a colormap's name, {map = ...}, a function or a "
                        "list of colors");
    }
    if (!function && _has(lua, index, "map")) {
        colormap = _colormapTable(lua, index, points, count);
        return nullptr;
    }
    if (!function && lua_rawlen(lua, index) != count) {
        luaL_error(lua, "surface colors must list one color per point, %d in all",
                   static_cast<int>(count));
    }
    auto colors = static_cast<Color*>(lua_newuserdatauv(lua, sizeof(Color) * count, 0));
    for (auto i = 0u; i < count; i++) {
        auto& point = points[i];
        if (function) {
            colors[i] = _callColors(lua, box, index, point);
            continue;
        }
        lua_rawgeti(lua, index, i + 1u);
        colors[i] = _color(lua, -1, "surface colors", &box->scene->theme());
        lua_pop(lua, 1);
    }
    return colors;
}

static int _surface(lua_State* lua)
{
    auto space = _factory(lua, {"points", "size", "mode", "shading", "light", "colors", "mesh"});
    lua_getfield(lua, 2, "size");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "surface requires size");
    uint32_t columns;
    uint32_t rows;
    _size(lua, -1, columns, rows);
    lua_pop(lua, 1);
    if (columns < 2u || rows < 2u) {
        return luaL_error(lua, "surface size must be at least 2 by 2");
    }

    lua_getfield(lua, 2, "points");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "surface requires points");
    _checkTable(lua, -1, "points");
    auto table = lua_absindex(lua, -1);
    auto count = columns * rows;
    if (lua_rawlen(lua, table) != count) {
        return luaL_error(lua, "surface points must match columns times rows");
    }
    // The points stay on the stack, out of the collector's reach, until the surface copies them.
    auto points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * count, 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, table, i + 1u);
        new (points + i) Vec3(_world(lua, -1, "surface point"));
        lua_pop(lua, 1);
    }
    lua_remove(lua, table);

    static constexpr LuaName<SurfaceMode> MODES[] = {
        {"solid", SurfaceMode::Solid}, {"mesh", SurfaceMode::Mesh}, {"solid_mesh", SurfaceMode::SolidMesh}};
    auto mode = _fieldEnum(lua, 2, "mode", SurfaceMode::Solid, MODES, "surface mode");
    auto shading = _fieldBool(lua, 2, "shading", true);

    // light = {direction = {x, y, z}, ambient = a}: a light fixed in the world in place of the
    // headlight, direction pointing toward it.
    SurfaceLight light;
    lua_getfield(lua, 2, "light");
    if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 0u) {
            return luaL_error(lua, "surface light must be {direction = {x, y, z}, ambient = a}");
        }
        if (!shading) return luaL_error(lua, "surface light needs shading");
        _options(lua, -1, {"direction", "ambient"}, false);
        lua_getfield(lua, -1, "direction");
        if (lua_isnil(lua, -1)) return luaL_error(lua, "surface light requires a direction");
        light.direction = _world(lua, -1, "surface light direction");
        lua_pop(lua, 1);
        if (light.direction.x == 0.0f && light.direction.y == 0.0f && light.direction.z == 0.0f) {
            return luaL_error(lua, "surface light direction must not be zero");
        }
        light.ambient = _fieldNumber(lua, -1, "ambient", light.ambient);
        if (light.ambient < 0.0f || light.ambient > 1.0f) {
            return luaL_error(lua, "surface light ambient must be between 0 and 1");
        }
    }
    lua_pop(lua, 1);

    // mesh = {u, v}: mesh lines at a resolution of their own, u across the columns and v across
    // the rows.
    uint32_t mesh[2] = {};
    lua_getfield(lua, 2, "mesh");
    if (!lua_isnil(lua, -1)) {
        if (mode == SurfaceMode::Solid) {
            return luaL_error(lua, "surface mesh needs mode 'mesh' or 'solid_mesh'");
        }
        if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2u) {
            return luaL_error(lua, "surface mesh must be {columns, rows}, the lines across each");
        }
        uint32_t limits[2] = {columns, rows};
        for (auto i = 0u; i < 2u; i++) {
            lua_rawgeti(lua, -1, static_cast<lua_Integer>(i + 1u));
            auto lines = _integer(lua, -1, "surface mesh");
            lua_pop(lua, 1);
            if (lines != 0 && (lines < 2 || lines > static_cast<lua_Integer>(limits[i]))) {
                return luaL_error(lua, "surface mesh takes 0 or 2 to %d lines across the columns "
                                       "and 0 or 2 to %d across the rows",
                                  static_cast<int>(columns), static_cast<int>(rows));
            }
            mesh[i] = static_cast<uint32_t>(lines);
        }
    }
    lua_pop(lua, 1);

    SurfaceColormap colormap;
    Color* colors = nullptr;
    lua_getfield(lua, 2, "colors");
    auto rule = lua_gettop(lua);
    if (!lua_isnil(lua, -1)) {
        colors = _surfaceColors(lua, space.scene, -1, points, count, colormap);
    }

    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, SurfaceMesh::gen(points, columns, rows));
    if (object) {
        object->mode = mode;
        object->shading = shading;
        object->colormap = colormap;
        object->meshColumns = mesh[0];
        object->meshRows = mesh[1];
        object->light = light;
        if (colors && object->colors(colors, count) != Result::Success) {
            return luaL_error(lua, "could not create surface: out of memory");
        }
        _keepColors(lua, rule, object);
    }
    return _add(lua, space, owner, object, 2, "surface");
}

// ---------- Pictures, cells, connectors and samples ----------

static Result _asset(LuaScene* scene, const char* name, const Asset*& asset, Asset*& owned)
{
    asset = scene->resolver ? scene->resolver(name, scene->resolverData) : nullptr;
    if (asset) return Result::Success;
    // Native fallback: relative names resolve against the script directory.
    auto size = std::strlen(name);
    auto memory = scene->memory;
    auto absolute = name[0] == '/' || name[0] == '\\' || (size > 2u && name[1] == ':');
    char* joined = nullptr;
    if (!absolute && memory->sourcePath && memory->sourceRoot) {
        if (memory->sourceRoot > SIZE_MAX - size - 1u) return Result::InvalidArguments;
        joined = new (std::nothrow) char[memory->sourceRoot + size + 1u];
        if (!joined) return Result::OutOfMemory;
        std::memcpy(joined, memory->sourcePath, memory->sourceRoot);
        std::memcpy(joined + memory->sourceRoot, name, size + 1u);
        name = joined;
    }
    auto result = AssetLoader::load(name, owned);
    delete[] joined;
    asset = owned;
    return result;
}

// Why an image asset could not be used.
static const char* _assetReason(Result result)
{
    switch (result) {
        case Result::NonSupport: return "only .svg, .png, .jpg, .jpeg and .webp images are supported";
        case Result::InvalidArguments:
            return "the file is missing, unreadable, empty, larger than 256 MiB or not a valid image";
        default: return _explain(result);
    }
}

static int _picture(lua_State* lua)
{
    auto space = _factory(lua, {"asset", "pixels", "size", "center", "filter"});
    for (auto shape : {"color", "stroke", "fill", "gradient", "radius", "dash", "dash_offset"}) {
        _refuse(lua, shape, "picture preserves source colors, so %s is unsupported");
    }
    auto hasAsset = _has(lua, 2, "asset");
    auto hasPixels = _has(lua, 2, "pixels");
    if (hasAsset == hasPixels) return luaL_error(lua, "picture requires exactly one of asset or pixels");
    if (hasAsset && _has(lua, 2, "size")) return luaL_error(lua, "picture size is only valid with pixels");
    auto center = _fieldWorld(lua, 2, "center", {});
    auto width = _fieldNumber(lua, 2, "width", 1.0f);
    static constexpr LuaName<ImageFilter> FILTERS[] = {
        {"bilinear", ImageFilter::Bilinear}, {"nearest", ImageFilter::Nearest}};
    auto filter = _fieldEnum(lua, 2, "filter", ImageFilter::Bilinear, FILTERS, "picture filter");
    const Asset* asset = nullptr;
    Asset* owned = nullptr;
    auto status = Result::Success;
    char assetName[LUA_PATH_LIMIT + 1] = {};
    if (hasAsset) {
        size_t nameSize;
        auto name = _fieldString(lua, 2, "asset", &nameSize);
        if (!nameSize || nameSize > LUA_PATH_LIMIT) {
            return luaL_error(lua, "asset name must be 1 to 4096 bytes");
        }
        std::memcpy(assetName, name, nameSize + 1u);
        lua_pop(lua, 1);
        status = _asset(space.scene, assetName, asset, owned);
    } else {
        lua_getfield(lua, 2, "size");
        if (lua_isnil(lua, -1)) return luaL_error(lua, "picture pixels require size");
        uint32_t columns;
        uint32_t rows;
        _size(lua, -1, columns, rows);
        lua_pop(lua, 1);
        lua_getfield(lua, 2, "pixels");
        _checkTable(lua, -1, "pixels");
        auto count = static_cast<size_t>(columns) * rows;
        if (lua_rawlen(lua, -1) != count) {
            return luaL_error(lua, "picture pixels must match columns times rows");
        }
        auto pixels = static_cast<uint32_t*>(lua_newuserdatauv(lua, count * sizeof(uint32_t), 0));
        for (auto i = 0u; i < count; i++) {
            lua_rawgeti(lua, -2, i + 1u);
            auto color = _color(lua, -1, "picture pixel");
            lua_pop(lua, 1);
            pixels[i] = static_cast<uint32_t>(color.a) << 24
                      | static_cast<uint32_t>(color.b) << 16
                      | static_cast<uint32_t>(color.g) << 8 | color.r;
        }
        status = AssetLoader::load(pixels, columns, rows, owned);
        asset = owned;
        lua_pop(lua, 2);
    }
    if (status != Result::Success) {
        delete owned;
        if (hasAsset) {
            return luaL_error(lua, "could not load asset '%s': %s", assetName, _assetReason(status));
        }
        return luaL_error(lua, "could not load picture pixels: %s",
                          status == Result::InvalidArguments ? "the image may hold at most 16777216 pixels"
                                                             : _explain(status));
    }
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Picture::gen(asset, center, width));
    delete owned;
    if (object) object->filter = filter;
    return _add(lua, space, owner, object, 2, "picture");
}

// How a cell, and each cell of a sampled field, fills its square.
static constexpr LuaName<CellMode> CELL_MODES[] = {{"full", CellMode::Full}, {"padd", CellMode::Padd}};

static int _cell(lua_State* lua)
{
    auto space = _factory(lua, {"origin", "size", "mode", "padding", "depth", "texture", "source", "destination",
                                "patches"});
    for (auto shape : {"stroke", "fill", "gradient", "width", "radius", "dash", "dash_offset"}) {
        _refuse(lua, shape, "cell uses color, texture and patches, so %s is unsupported");
    }
    if (!_has(lua, 2, "texture") && (_has(lua, 2, "source") || _has(lua, 2, "destination"))) {
        return luaL_error(lua, "cell source and destination require texture");
    }
    lua_getfield(lua, 2, "size");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "cell requires size");
    uint32_t columns;
    uint32_t rows;
    _size(lua, -1, columns, rows);
    lua_pop(lua, 1);
    auto origin = _fieldWorld(lua, 2, "origin", {});
    auto mode = _fieldEnum(lua, 2, "mode", CellMode::Full, CELL_MODES, "cell mode");
    auto padding = _fieldNumber(lua, 2, "padding", 0.05f);
    auto depth = _fieldNumber(lua, 2, "depth", 1.0f);
    Color color;
    _fieldColor(lua, 2, "color", color, &space.scene->scene->theme());
    // Texture regions and patches can raise through Lua; give the native Cell a GC owner first.
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Cell::gen(origin, columns, rows, color, mode, padding));
    if (object) object->depth = depth;
    if (object && _has(lua, 2, "texture")) {
        size_t nameSize;
        auto name = _fieldString(lua, 2, "texture", &nameSize);
        if (!nameSize || nameSize > LUA_PATH_LIMIT) {
            return luaL_error(lua, "texture name must be 1 to 4096 bytes");
        }
        char assetName[LUA_PATH_LIMIT + 1];
        std::memcpy(assetName, name, nameSize + 1u);
        lua_pop(lua, 1);
        auto source = _fieldRegion(lua, 2, "source");
        auto destination = _fieldRegion(lua, 2, "destination");
        const Asset* asset = nullptr;
        Asset* owned = nullptr;
        auto status = _asset(space.scene, assetName, asset, owned);
        if (status == Result::Success) status = object->texture(asset, source, destination);
        delete owned;
        if (status != Result::Success) {
            return luaL_error(lua, "could not apply texture '%s': %s", assetName,
                              status == Result::InvalidArguments && asset
                                  ? "the source and destination regions must lie inside the image and the cell"
                                  : _assetReason(status));
        }
    }
    if (object && _has(lua, 2, "patches")) {
        lua_getfield(lua, 2, "patches");
        _checkTable(lua, -1, "patches");
        auto patches = lua_rawlen(lua, -1);
        if (patches > 16384u) {
            return luaL_error(lua, "cell patch limit exceeded");
        }
        for (auto i = 0u; i < patches; i++) {
            lua_rawgeti(lua, -1, i + 1u);
            _checkTable(lua, -1, "a cell patch");
            _options(lua, -1, {"region", "color"}, false);
            auto region = _fieldRegion(lua, -1, "region");
            Color patchColor;
            if (!_fieldColor(lua, -1, "color", patchColor, &space.scene->scene->theme())) {
                return luaL_error(lua, "cell patch requires color");
            }
            auto status = object->fill(region, patchColor);
            lua_pop(lua, 1);
            if (status != Result::Success) {
                return luaL_error(lua, "invalid cell patch: %s",
                                  status == Result::InvalidArguments ? "the region must lie inside the cell"
                                                                     : _explain(status));
            }
        }
        lua_pop(lua, 1);
    }
    return _add(lua, space, owner, object, 2, "cell");
}

static Object* _object(lua_State* lua, Scene* scene, int index)
{
    auto handle = _handle(lua, index);
    if (handle->scene->scene != scene || scene->object(handle->object->id()) != handle->object) {
        luaL_error(lua, "handle belongs to another scene");
    }
    return handle->object;
}

Scene* detail::luaScene(lua_State* lua, int index)
{
    return _scene(lua, index)->scene;
}

Scene* detail::luaHandleScene(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    if (!luaL_testudata(lua, index, SCENE_METATABLE)) _handle(lua, index);
    _pushScene(lua, index);
    return _scene(lua, -1)->scene;
}

Object* detail::luaObject(lua_State* lua, Scene* scene, int index)
{
    return _object(lua, scene, index);
}

// Connector attachment uses sampled closed outlines, with bounds as the fallback.
static int _connector(lua_State* lua)
{
    auto space = _factory(lua, {"from", "to", "padding", "tail", "tip"});
    lua_getfield(lua, 2, "from");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "missing field 'from'");
    auto from = _object(lua, space.scene->scene, -1);
    lua_pop(lua, 1);
    lua_getfield(lua, 2, "to");
    if (lua_isnil(lua, -1)) return luaL_error(lua, "missing field 'to'");
    auto to = _object(lua, space.scene->scene, -1);
    lua_pop(lua, 1);
    auto owner = _owner(lua, space.scene);
    auto object = _adopt(owner, Connector::gen(from, to));
    if (object) {
        object->padding = _fieldNumber(lua, 2, "padding", object->padding);
        object->tail = _fieldNumber(lua, 2, "tail", object->tail);
        object->tip = _fieldNumber(lua, 2, "tip", object->tip);
        if (object->padding < 0.0f || object->tail < 0.0f || object->tip < 0.0f) {
            return luaL_error(lua, "connector padding, tail and tip must be non-negative");
        }
    }
    return _add(lua, space, owner, object, 2, "connector");
}

// The color function of a cell or voxel field. Nothing may raise through the core, so the
// sampler leaves what the function raises on the stack, or names the color it did not return,
// for _sampleGen to raise once the core call returns.
struct LuaSample
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    int callback = 0;
    bool raised = false;
    const char* refused = nullptr;
};

static bool _sample(float x, float y, float z, float time, Color& color,
                    void* data, bool spatial) noexcept
{
    auto context = static_cast<LuaSample*>(data);
    auto lua = context->lua;
    lua_pushvalue(lua, context->callback);
    lua_pushnumber(lua, x);
    lua_pushnumber(lua, y);
    if (spatial) lua_pushnumber(lua, z);
    lua_pushnumber(lua, time);
    if (_sampleCall(lua, context->scene, spatial ? 4 : 3, 1) != LUA_OK) {
        context->raised = true;
        return false;
    }
    size_t size = 0;
    auto value = lua_type(lua, -1) == LUA_TSTRING ? lua_tolstring(lua, -1, &size) : nullptr;
    if (!value) context->refused = "color string";
    else if (!_hex(value, size)) context->refused = "#rgb, #rgba, #rrggbb or #rrggbbaa color";
    else color = Color::hex(value);
    lua_pop(lua, 1);
    return !context->refused;
}

static bool _sampleCell(float x, float y, float time, Color& color, void* data) noexcept
{
    return _sample(x, y, 0.0f, time, color, data, false);
}

static bool _sampleVoxel(float x, float y, float z, float time, Color& color,
                         void* data) noexcept
{
    return _sample(x, y, z, time, color, data, true);
}

static int _sampleGen(lua_State* lua, bool spatial)
{
    auto name = spatial ? "voxel" : "cell";
    auto handle = _layoutHandle(lua);
    auto object = _object(lua, handle->scene->scene, 1);
    if (object->type() != Type::Space) return luaL_error(lua, "%s requires a Space", name);
    luaL_checktype(lua, 2, LUA_TFUNCTION);
    auto mode = CellMode::Padd;
    auto padding = 0.05f;
    SampleTime time;
    if (_optionTable(lua, 3, {"mode", "padding", "duration", "fps"})) {
        mode = _fieldEnum(lua, 3, "mode", mode, CELL_MODES, spatial ? "voxel mode" : "cell mode");
        padding = _fieldNumber(lua, 3, "padding", padding);
        time.duration = _fieldNumber(lua, 3, "duration", time.duration);
        auto fps = _fieldInteger(lua, 3, "fps", time.fps);
        if (fps <= 0 || static_cast<uint64_t>(fps) > std::numeric_limits<uint32_t>::max()) {
            return luaL_error(lua, "%s fps must be a positive 32-bit integer", name);
        }
        time.fps = static_cast<uint32_t>(fps);
    }

    auto space = static_cast<Space*>(object);
    float starts[3];
    uint32_t counts[3];
    auto status = spaceSamples(*space, spatial, starts, counts);
    if (status != Result::Success) {
        return luaL_error(lua, "could not sample %s: %s", name,
                          status == Result::InvalidArguments
                              ? "the space needs finite ranges with positive steps"
                              : "the ranges give no sample or more than 4096 on an axis, or more than "
                                "16384 in a slice or 65536 in all");
    }
    auto volume = static_cast<uint64_t>(counts[0]) * counts[1] * counts[2];
    uint32_t frames;
    status = timeFrames(time, volume, frames);
    if (status != Result::Success) {
        return luaL_error(lua, "could not sample %s over time: %s", name,
                          status == Result::InvalidArguments
                              ? "the duration must be non-negative"
                              : "the duration and fps give more than 4096 frames or 262144 colors in all");
    }
    auto scene = handle->scene;
    LuaCharge charge{512u + counts[2] * uint64_t{512} + volume * frames * sizeof(Color), counts[2] + 1u};
    _fit(lua, scene, charge);

    auto context = LuaSample{lua, scene, lua_absindex(lua, 2)};
    Group* field = nullptr;
    if (spatial) {
        status = space->voxel(_sampleVoxel, field, &context, mode, padding, time);
    } else {
        status = space->cell(_sampleCell, field, &context, mode, padding, time);
    }
    if (context.raised) return lua_error(lua);
    if (context.refused) return luaL_error(lua, "%s callback must return a %s", name, context.refused);
    if (status != Result::Success || !field) {
        return luaL_error(lua, "could not create %s field: %s", name, _reason(scene->scene, status));
    }

    auto owner = _owner(lua, scene);
    owner->object = field;
    owner->owner = false;
    luaL_getmetatable(lua, GROUP_METATABLE);
    lua_setmetatable(lua, -2);
    _remember(lua, -1);
    _spend(scene, charge);
    _evaluate(lua, scene);
    return 1;
}

static int _cellMethod(lua_State* lua)
{
    if (lua_type(lua, 2) == LUA_TFUNCTION) return _sampleGen(lua, false);
    return _cell(lua);
}

static int _voxel(lua_State* lua)
{
    return _sampleGen(lua, true);
}

// ---------- Layout methods ----------

static int _moveTo(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    auto result = handle->object->moveTo(_world(lua, 2, "point"));
    _check(lua, handle->scene->scene, result, "could not move object");
    return 0;
}

static int _nextTo(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    auto target = _object(lua, handle->scene->scene, 2);
    auto direction = _world(lua, 3, "direction");
    auto gap = lua_isnoneornil(lua, 4) ? 0.25f : _number(lua, 4, "gap");
    auto result = handle->object->nextTo(target, direction, gap);
    _check(lua, handle->scene->scene, result, "could not place object");
    return 0;
}

static int _alignTo(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    auto target = _object(lua, handle->scene->scene, 2);
    auto result = handle->object->alignTo(target, _world(lua, 3, "direction"));
    _check(lua, handle->scene->scene, result, "could not align object");
    return 0;
}

static int _maskObject(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    luaL_checkany(lua, 2);
    auto target = lua_isnil(lua, 2) ? nullptr : _object(lua, handle->scene->scene, 2);
    auto mode = lua_isnoneornil(lua, 3) ? MaskMode::Alpha : _maskMode(lua, 3);
    _check(lua, handle->scene->scene, handle->object->mask(target, mode), "could not mask object");
    return 0;
}

static int _arrange(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    if (handle->object->type() != Type::Group) return luaL_error(lua, "arrange requires a group");
    auto direction = lua_isnoneornil(lua, 2) ? Vec3{1.0f, 0.0f, 0.0f} : _world(lua, 2, "direction");
    auto gap = lua_isnoneornil(lua, 3) ? 0.25f : _number(lua, 3, "gap");
    auto result = static_cast<Group*>(handle->object)->arrange(direction, gap);
    _check(lua, handle->scene->scene, result, "could not arrange group");
    return 0;
}

static int _arrangeGrid(lua_State* lua)
{
    auto handle = _layoutHandle(lua);
    if (handle->object->type() != Type::Group) return luaL_error(lua, "arrange_grid requires a group");
    auto columns = _integer(lua, 2, "columns");
    if (columns <= 0 || columns > UINT32_MAX) return luaL_error(lua, "columns must be a positive 32-bit integer");
    auto columnGap = lua_isnoneornil(lua, 3) ? 0.25f : _number(lua, 3, "column gap");
    auto rowGap = lua_isnoneornil(lua, 4) ? 0.25f : _number(lua, 4, "row gap");
    auto result = static_cast<Group*>(handle->object)->arrangeGrid(static_cast<uint32_t>(columns), columnGap, rowGap);
    _check(lua, handle->scene->scene, result, "could not arrange group");
    return 0;
}

// ---------- Timing and motion options ----------

// The curves a name gives, as a string or as the preset of a curve table.
static constexpr LuaName<AnimCurvePreset> CURVE_PRESETS[] = {
    {"linear", AnimCurvePreset::Linear}, {"smooth", AnimCurvePreset::Smooth}, {"ease_in", AnimCurvePreset::EaseIn},
    {"ease_out", AnimCurvePreset::EaseOut}, {"ease_in_out", AnimCurvePreset::EaseInOut},
    {"gentle", AnimCurvePreset::Gentle}, {"snappy", AnimCurvePreset::Snappy}, {"back", AnimCurvePreset::Back},
    {"bounce", AnimCurvePreset::Bounce}, {"elastic", AnimCurvePreset::Elastic}};

static AnimCurve _curve(lua_State* lua, int index)
{
    if (lua_isnoneornil(lua, index)) return AnimCurve::preset(AnimCurvePreset::Smooth);
    if (lua_isstring(lua, index)) return AnimCurve::preset(_enum(lua, index, "animation curve preset", CURVE_PRESETS));
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    _options(lua, index, {"preset", "bezier", "strength", "reverse"}, false);
    auto hasPreset = _has(lua, index, "preset");
    auto hasBezier = _has(lua, index, "bezier");
    if (hasPreset == hasBezier) {
        luaL_error(lua, "animation curve requires exactly one of 'preset' or 'bezier'");
    }
    auto strength = _fieldNumber(lua, index, "strength", 1.0f);
    if (strength < 0.0f || strength > 2.0f) {
        luaL_error(lua, "animation curve strength must be between 0 and 2");
    }
    AnimCurve curve;
    if (hasPreset) {
        lua_getfield(lua, index, "preset");
        curve = AnimCurve::preset(_enum(lua, -1, "animation curve preset", CURVE_PRESETS), strength);
        lua_pop(lua, 1);
    } else {
        float bezier[4];
        lua_getfield(lua, index, "bezier");
        _matrix(lua, -1, bezier, 4, "animation curve bezier");
        lua_pop(lua, 1);
        curve = AnimCurve::cubicBezier({bezier[0], bezier[1]}, {bezier[2], bezier[3]}, strength);
    }
    curve.reverse = _fieldBool(lua, index, "reverse", false);
    if (!curve.valid()) {
        luaL_error(lua, "animation curve requires x controls in [0, 1] and y controls in [-2, 2]");
    }
    return curve;
}

AnimCurve detail::luaAnimCurve(lua_State* lua, int index)
{
    return _curve(lua, index);
}

// The curve the curve option of the table at index names, or fallback without it.
static AnimCurve _fieldCurve(lua_State* lua, int index,
                             const AnimCurve& fallback = AnimCurve::preset(AnimCurvePreset::Smooth))
{
    lua_getfield(lua, index, "curve");
    auto curve = lua_isnil(lua, -1) ? fallback : _curve(lua, -1);
    lua_pop(lua, 1);
    return curve;
}

bool detail::luaTiming(lua_State* lua, int index, int options, bool stagger,
                       std::initializer_list<const char*> fields)
{
    if (!lua_istable(lua, index)) return false;
    for (auto i = index + 1; i <= lua_gettop(lua); i++) {
        if (!lua_isnil(lua, i)) luaL_error(lua, "timing options cannot be combined with positional arguments");
    }
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "option names must be strings");
        auto name = lua_tostring(lua, -2);
        if (!_option(name, {"duration", "curve"}, false)
            && !(stagger && std::strcmp(name, "lag") == 0) && !_option(name, fields, false)) {
            luaL_error(lua, "unknown option '%s'", name);
        }
        lua_pop(lua, 1);
    }
    auto last = std::max(index + (stagger ? 2 : 1), options);
    lua_settop(lua, last);
    lua_pushvalue(lua, index);
    auto table = lua_gettop(lua);
    lua_getfield(lua, table, "duration");
    lua_replace(lua, index);
    lua_getfield(lua, table, "curve");
    lua_replace(lua, index + 1);
    if (stagger && options != index + 2) {
        lua_getfield(lua, table, "lag");
        lua_replace(lua, index + 2);
    }
    if (options) {
        lua_newtable(lua);
        for (auto field : fields) {
            lua_getfield(lua, table, field);
            lua_setfield(lua, -2, field);
        }
        lua_replace(lua, options);
    }
    lua_pop(lua, 1);
    return true;
}

static float _duration(lua_State* lua, int index, float fallback, bool zero = false)
{
    if (lua_isnoneornil(lua, index)) return fallback;
    auto duration = _number(lua, index, "duration");
    if (duration < 0.0f || (!zero && duration == 0.0f)) luaL_error(lua, "duration must be %s", zero ? "non-negative" : "positive");
    return duration;
}

static float _fieldDuration(lua_State* lua, int index, const char* name, float fallback)
{
    auto duration = _fieldNumber(lua, index, name, fallback);
    if (duration <= 0.0f) luaL_error(lua, "%s must be positive", name);
    return duration;
}

// lag, the share of the duration by which each later target starts after the one before, which
// cannot be negative.
static float _lag(lua_State* lua, float lag)
{
    if (lag < 0.0f) luaL_error(lua, "lag cannot be negative");
    return lag;
}

static DrawDirection _direction(lua_State* lua, int index)
{
    static constexpr LuaName<DrawDirection> DIRECTIONS[] = {
        {"forward", DrawDirection::Forward}, {"reverse", DrawDirection::Reverse},
        {"clockwise", DrawDirection::Clockwise}, {"counterclockwise", DrawDirection::CounterClockwise}};
    if (lua_isnoneornil(lua, index)) return DrawDirection::Forward;
    return _enum(lua, index, "direction", DIRECTIONS);
}

static GrowthEdge _growthEdge(lua_State* lua, int index)
{
    static constexpr LuaName<GrowthEdge> EDGES[] = {{"left", GrowthEdge::Left}, {"right", GrowthEdge::Right},
                                                    {"bottom", GrowthEdge::Bottom}, {"top", GrowthEdge::Top}};
    return _enum(lua, index, "growth edge", EDGES);
}

// Central angle of a travel arc in radians; a full turn cannot join two distinct points.
static float _fieldArc(lua_State* lua, int index)
{
    auto arc = _fieldNumber(lua, index, "arc", 0.0f);
    if (std::fabs(arc) >= TWO_PI) {
        luaL_error(lua, "arc must be greater than -2*pi and less than 2*pi radians");
    }
    return arc;
}

// Charges the clips a timeline call added since the scene held before of them, strings, the
// bytes of the strings and points its changes copy, and grown, the points by which plots may
// grow. A group that passes its stroke, fill or width down adds a clip for each shape that
// takes it, so a call may add more clips than it names targets; the scene is discarded when the
// script fails.
static int _charge(lua_State* lua, LuaScene* box, uint32_t before, size_t strings = 0, uint32_t grown = 0)
{
    auto added = box->scene->clipCount() - before;
    LuaCharge charge{uint64_t{added} * LUA_CLIP_CHARGE + strings, 0, grown, added};
    _fit(lua, box, charge);
    _spend(box, charge);
    return _evaluate(lua, box);
}

// ---------- Braces ----------

// Where a brace stands: its ends lie on the line from start along across, width apart, and its
// tip points along out, across and out being unit vectors in the xy plane of its space.
struct LuaBraceFrame
{
    Vec3 start;
    Vec3 across;
    Vec3 out;
    float width = 0.0f;
};

// How far what a brace spans reaches across it and out toward it, in the space it joins.
struct LuaBraceReach
{
    float low = 0.0f;
    float high = 0.0f;
    float out = 0.0f;
    float front = 0.0f;
    float back = 0.0f;
    bool defined = false;
};

// Grows reach by object, model taking its parent's space into the brace's: a group by each
// object in it, so that a slanted brace fits the boxes of the glyphs and shapes it spans rather
// than one box around them all, and any other object by the corners of its bounds. Along the
// axes this is the box scene:bounds reports.
static void _braceReach(const Object* object, const Mat4& model, const LuaBraceFrame& frame, LuaBraceReach& reach)
{
    if (object->type() == Type::Group) {
        auto inner = model * object->model;
        for (auto i = 0u; i < object->childCount(); i++) _braceReach(object->childAt(i), inner, frame, reach);
        return;
    }
    Bounds own;
    if (!object->bounds(own)) return;
    for (auto corner = 0u; corner < 8u; corner++) {
        auto point = model * Vec3{corner & 1u ? own.max.x : own.min.x, corner & 2u ? own.max.y : own.min.y,
                                  corner & 4u ? own.max.z : own.min.z};
        auto across = point.dot(frame.across);
        auto out = point.dot(frame.out);
        if (!reach.defined) {
            reach.low = reach.high = across;
            reach.out = out;
            reach.front = reach.back = point.z;
            reach.defined = true;
        }
        reach.low = std::min(reach.low, across);
        reach.high = std::max(reach.high, across);
        reach.out = std::max(reach.out, out);
        reach.front = std::min(reach.front, point.z);
        reach.back = std::max(reach.back, point.z);
    }
}

// The direction option of a brace at index: a name, or a vector in the xy plane, made unit.
// False when the options leave it out.
static bool _braceDirection(lua_State* lua, int index, Vec3& output)
{
    static constexpr auto SHAPE = "brace direction must be 'down', 'up', 'left', 'right' or a vector in the xy plane";
    lua_getfield(lua, index, "direction");
    auto type = lua_type(lua, -1);
    if (type == LUA_TNIL) {
        lua_pop(lua, 1);
        return false;
    }
    if (type == LUA_TSTRING) {
        auto name = lua_tostring(lua, -1);
        if (std::strcmp(name, "down") == 0) output = {0.0f, -1.0f, 0.0f};
        else if (std::strcmp(name, "up") == 0) output = {0.0f, 1.0f, 0.0f};
        else if (std::strcmp(name, "left") == 0) output = {-1.0f, 0.0f, 0.0f};
        else if (std::strcmp(name, "right") == 0) output = {1.0f, 0.0f, 0.0f};
        else luaL_error(lua, "%s", SHAPE);
    } else {
        if (type != LUA_TTABLE) luaL_error(lua, "%s", SHAPE);
        auto value = _world(lua, -1, "direction");
        auto length = std::sqrt(value.x * value.x + value.y * value.y);
        if (value.z != 0.0f || !(length > 1.0e-6f) || !std::isfinite(length)) luaL_error(lua, "%s", SHAPE);
        output = {value.x / length, value.y / length, 0.0f};
    }
    lua_pop(lua, 1);
    return true;
}

// The outline of a brace depth deep, as TeX draws one: from each end, on the line through
// start, a hook turns into an arm of even thickness, and the arms turn into the tip, depth out
// from the middle, where they meet in a point. Its ends and tip are exactly where the frame
// puts them. Writes at most 15 commands to output and returns how many it wrote.
static uint32_t _braceOutline(const LuaBraceFrame& frame, float depth, PathCommand* output)
{
    // A quarter turn is a cubic with its controls this share of the way along its tangents.
    static constexpr auto TURN = 0.5522848f;
    auto width = frame.width;
    auto middle = 0.5f * width;
    auto arm = 0.5f * depth;
    auto half = 0.09f * depth;
    // How far a hook, or half the tip, reaches across, shrinking on a short brace until its arms
    // are gone.
    auto reach = std::min(0.9f * depth, 0.25f * width);
    auto end = std::min(2.4f * half, 0.5f * reach);
    auto notch = depth - 2.4f * half;
    auto arms = middle - 2.0f * reach > 1.0e-4f * depth;
    auto count = 0u;
    auto at = [&](float u, float v) { return frame.start + frame.across * u + frame.out * v; };
    auto line = [&](float u, float v) {
        output[count].verb = count ? PathVerb::Line : PathVerb::Move;
        output[count++].to = at(u, v);
    };
    auto cubic = [&](float u1, float v1, float u2, float v2, float u, float v) {
        output[count].verb = PathVerb::Cubic;
        output[count].control1 = at(u1, v1);
        output[count].control2 = at(u2, v2);
        output[count++].to = at(u, v);
    };
    // The outer edge, from the left end through the tip to the right end.
    auto outer = arm + half;
    line(0.0f, 0.0f);
    cubic(0.0f, TURN * outer, reach * (1.0f - TURN), outer, reach, outer);
    if (arms) line(middle - reach, outer);
    cubic(middle - reach * (1.0f - TURN), outer, middle, depth - TURN * (depth - outer), middle, depth);
    cubic(middle, depth - TURN * (depth - outer), middle + reach * (1.0f - TURN), outer, middle + reach, outer);
    if (arms) line(width - reach, outer);
    cubic(width - reach * (1.0f - TURN), outer, width, TURN * outer, width, 0.0f);
    // Across the right end, then the inner edge back through the notch above the tip.
    auto inner = arm - half;
    line(width - end, 0.0f);
    cubic(width - end, TURN * inner, width - reach + TURN * (reach - end), inner, width - reach, inner);
    if (arms) line(middle + reach, inner);
    cubic(middle + reach * (1.0f - TURN), inner, middle, notch - TURN * (notch - inner), middle, notch);
    cubic(middle, notch - TURN * (notch - inner), middle - reach * (1.0f - TURN), inner, middle - reach, inner);
    if (arms) line(reach, inner);
    cubic(reach - TURN * (reach - end), inner, end, TURN * inner, end, 0.0f);
    output[count++].verb = PathVerb::Close;
    return count;
}

// Gives a text of one line the box its ink covers, from its extent in world units, moved off
// where align pins it; one of several lines, which align sets line by line, or a blank one
// keeps its line box, as measure = true gives it.
static void _inkBox(Text* text, const TextExtent& extent)
{
    auto wide = extent.right - extent.left;
    auto tall = extent.top - extent.bottom;
    if (std::strchr(text->text(), '\n') || !(wide > 0.0f) || !(tall > 0.0f)) {
        text->box = {extent.width, extent.height};
        return;
    }
    text->box = {wide, tall};
    text->boxOffset = {extent.left - text->align.x * (extent.width - wide),
                       text->align.y * (extent.height - tall) - extent.ascent + extent.top};
}

// Builds a filled brace from authored bounds or an explicit segment.
// Returns the brace and optional label at its tip; strings create Text and handles move.
static int _brace(lua_State* lua)
{
    auto space = _factory(lua, {"around", "from", "to", "direction", "offset", "depth", "label", "gap", "size", "font",
                                "role", "orientation"});
    lua_settop(lua, 2);
    auto box = space.scene;
    auto scene = box->scene;

    // The label, or the text that stands in for it, sets the size of the brace: a brace is set
    // with its text, as TeX sets one in its font.
    auto labelOwner = _owner(lua, box);
    lua_getfield(lua, 2, "label");
    auto labelType = lua_type(lua, 4);
    if (labelType != LUA_TNIL && labelType != LUA_TSTRING && labelType != LUA_TUSERDATA) {
        return luaL_error(lua, "brace label must be a string or a handle");
    }
    size_t labelSize = 0;
    auto labelText = labelType == LUA_TSTRING ? lua_tolstring(lua, 4, &labelSize) : "";
    if (labelSize > LUA_TEXT_LIMIT) return luaL_error(lua, "text exceeds the 1 MiB limit");
    auto text = _adopt(labelOwner, Text::gen(labelText, {}));
    if (!text) return luaL_error(lua, "could not create brace label: out of memory");
    _text(lua, 2, text);
    auto size = text->size > 0.0f ? text->size : _roleTheme(scene->theme(), text->role).size;
    auto em = text->orientation == TextOrientation::Plane ? size : size * pixelWorld(scene->config());
    auto offset = _fieldNumber(lua, 2, "offset", 0.25f * em);
    auto depth = _fieldNumber(lua, 2, "depth", 0.4f * em);
    auto gap = _fieldNumber(lua, 2, "gap", 0.2f * em);
    if (offset < 0.0f || gap < 0.0f) return luaL_error(lua, "brace offset and gap must be zero or more");
    if (!(depth > 0.0f)) return luaL_error(lua, "brace depth must be positive");

    LuaBraceFrame frame;
    Vec3 direction{0.0f, -1.0f, 0.0f};
    auto directed = _braceDirection(lua, 2, direction);
    auto around = _has(lua, 2, "around");
    auto from = _has(lua, 2, "from");
    auto to = _has(lua, 2, "to");
    if (around) {
        if (from || to) return luaL_error(lua, "brace takes around, or from and to, not both");
        frame.out = direction;
        frame.across = {-direction.y, direction.x, 0.0f};
        lua_getfield(lua, 2, "around");
        auto list = lua_gettop(lua);
        if (lua_istable(lua, list) && lua_rawlen(lua, list) > LUA_GROUP_LIMIT) {
            return luaL_error(lua, "animation group limit exceeded");
        }
        LuaBraceReach reach;
        _eachHandle(lua, list, "around must be a handle or a list of handles", [&](int item) {
            auto object = _object(lua, scene, item);
            auto model = Mat4::identity();
            auto parent = object->parent();
            for (; parent && parent != space.parent; parent = parent->parent()) model = parent->model * model;
            if (parent != space.parent) luaL_error(lua, "around takes objects below the group the brace joins");
            _braceReach(object, model, frame, reach);
        });
        lua_pop(lua, 1);
        if (!reach.defined) return luaL_error(lua, "around needs objects with bounds, such as texts set with measure = true");
        frame.width = reach.high - reach.low;
        frame.start = frame.across * reach.low + frame.out * (reach.out + offset)
                      + Vec3{0.0f, 0.0f, 0.5f * (reach.front + reach.back)};
    } else if (from || to) {
        if (!from || !to) return luaL_error(lua, "brace needs both from and to");
        auto start = _fieldWorld(lua, 2, "from");
        auto finish = _fieldWorld(lua, 2, "to");
        if (start.z != finish.z) return luaL_error(lua, "brace from and to must lie at one depth");
        auto span = finish - start;
        frame.width = std::sqrt(span.x * span.x + span.y * span.y);
        if (!(frame.width > 0.0f)) return luaL_error(lua, "brace from and to must be different points");
        frame.across = {span.x / frame.width, span.y / frame.width, 0.0f};
        // Below a segment drawn left to right, unless a direction says which side.
        if (!directed) direction = {frame.across.y, -frame.across.x, 0.0f};
        auto out = direction - frame.across * direction.dot(frame.across);
        auto length = out.length();
        if (!(length > 1.0e-4f)) return luaL_error(lua, "brace direction must not run along from and to");
        frame.out = out / length;
        frame.start = start + frame.out * offset;
    } else {
        return luaL_error(lua, "brace needs around, or from and to");
    }
    if (!(frame.width > 0.0f) || !std::isfinite(frame.width)) {
        return luaL_error(lua, "brace needs something to span across its direction");
    }

    PathCommand commands[16];
    auto count = _braceOutline(frame, depth, commands);
    auto path = Path::gen(commands, count, 24u);
    if (path) {
        // Filled as a glyph of its text is, unless the options say otherwise.
        path->stroke({0, 0, 0, 0});
        path->fill(_roleTheme(scene->theme(), text->role).color);
        path->gradient(false);
    }
    _add(lua, space, _owner(lua, box), path, 2, "brace");
    auto brace = lua_gettop(lua);
    if (labelType == LUA_TNIL) return 1;

    // The label stands gap out from the tip, as next_to sets one object beside another.
    auto tip = frame.start + frame.across * (0.5f * frame.width) + frame.out * depth;
    auto beside = [&](const Bounds& bounds) {
        auto half = bounds.size() * 0.5f;
        auto support = std::fabs(frame.out.x) * half.x + std::fabs(frame.out.y) * half.y;
        return tip + frame.out * (gap + support) - bounds.center();
    };
    if (labelType == LUA_TUSERDATA) {
        auto label = _object(lua, scene, 4);
        if (label->parent() != space.parent) {
            return luaL_error(lua, "a brace label handle must be a child of the group the brace joins");
        }
        Bounds bounds;
        if (!label->bounds(bounds)) return luaL_error(lua, "the brace label has no bounds yet");
        auto before = scene->clipCount();
        auto target = AnimationTarget::from(label).shift(beside(bounds));
        _check(lua, scene, scene->update(target), "could not place brace label");
        _charge(lua, box, before);
        lua_pushvalue(lua, brace);
        lua_pushvalue(lua, 4);
        return 2;
    }

    // A label text pins the side that faces the tip, so a new string grows away from it, and
    // takes the box its ink covers, so that the gap is the gap to its glyphs.
    auto side = [](float value) { return value > 0.5f ? 1.0f : value < -0.5f ? -1.0f : 0.0f; };
    text->align = {0.5f - 0.5f * side(frame.out.x), 0.5f + 0.5f * side(frame.out.y)};
    text->point = tip;
    _inkBox(text, _measured(lua, scene, text));
    Bounds bounds;
    if (text->bounds(bounds)) text->point = text->point + beside(bounds);
    lua_createtable(lua, 0, 6);
    auto style = lua_gettop(lua);
    for (auto name : {"color", "fill", "opacity", "progress", "layer"}) {
        lua_getfield(lua, 2, name);
        lua_setfield(lua, style, name);
    }
    lua_getfield(lua, 2, "id");
    if (lua_type(lua, -1) == LUA_TSTRING) {
        lua_pushfstring(lua, "%s/label", lua_tostring(lua, -1));
        lua_setfield(lua, style, "id");
    }
    lua_pop(lua, 1);
    lua_pushvalue(lua, 3);
    _add(lua, space, labelOwner, text, style, "brace label");
    lua_pushvalue(lua, brace);
    lua_insert(lua, -2);
    return 2;
}

// ---------- Playing animations and targets ----------

// Plays animations; copies is what the clones of the morphs from copies among them cost.
static int _play(lua_State* lua, LuaScene* box, const Animation* animations, uint32_t count,
                 float duration, const AnimCurve& curve, float lag = 0.0f, const LuaCharge& copies = {})
{
    if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    _fit(lua, box, {copies.native + uint64_t{count} * LUA_CLIP_CHARGE, copies.objects, copies.points, count});
    auto before = box->scene->clipCount();
    auto result = box->scene->play(animations, count, duration, curve, lag);
    _check(lua, box->scene, result, "could not play animation");
    _spend(box, copies);
    return _charge(lua, box, before);
}

static int _play(lua_State* lua, LuaScene* box, const AnimationTarget* targets, uint32_t count,
                 float duration, const AnimCurve& curve, float lag, size_t strings = 0, uint32_t grown = 0)
{
    if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    auto clips = _clipCharge(count);
    _fit(lua, box, {0, 0, grown});
    _fit(lua, box, clips);
    // The retained states take the blame only for bytes the clips leave room for.
    _fit(lua, box, {clips.native + strings}, "retained states share the 64 MiB budget");
    auto before = box->scene->clipCount();
    auto result = box->scene->play(targets, count, duration, curve, lag);
    _check(lua, box->scene, result, "could not play animation");
    return _charge(lua, box, before, strings, grown);
}

// Plays the one animation make builds for the handle at 2, for the duration and curve at index
// and index + 1; the methods that play one animation differ only in make.
template<typename Make>
static int _playOne(lua_State* lua, int index, Make make)
{
    detail::luaTiming(lua, index);
    auto box = _scene(lua);
    auto animation = make(box->scene, _object(lua, box->scene, 2));
    auto duration = _duration(lua, index, 1.0f);
    return _play(lua, box, &animation, 1, duration, _curve(lua, index + 1));
}

// A contour child keeps its grid and level, but no callback. Each state traces a new fn or
// values at authoring time and hands ordinary plot geometry to the timeline.
static bool _recontour(lua_State* lua, LuaScene* box, int index, const Plot* plot, const LuaPlotRecord& kept,
                       AnimationTarget& target, size_t* strings, uint32_t* grown, int anchor)
{
    auto top = lua_gettop(lua);
    auto function = _has(lua, index, "fn"), sampled = _has(lua, index, "values");
    if (_has(lua, index, "points") || _has(lua, index, "discontinuities")) {
        luaL_error(lua, "a contour state takes fn or values, not points or discontinuities");
    }
    if (!function && !sampled) {
        for (auto key : {"level", "x_range", "y_range"}) {
            if (_has(lua, index, key)) luaL_error(lua, "contour %s needs fn or values", key);
        }
        return false;
    }
    if (function && sampled) luaL_error(lua, "a contour state takes either fn or values, not both");
    auto xRange = _fieldRange(lua, index, "x_range", kept.range);
    auto yRange = _fieldRange(lua, index, "y_range", kept.contourY);
    auto level = kept.level;
    lua_getfield(lua, index, "level");
    if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) != LUA_TNUMBER || !(std::fabs(lua_tonumber(lua, -1)) <= FLT_MAX)) {
            luaL_error(lua, "contour level must be a finite number");
        }
        level = lua_tonumber(lua, -1);
    }
    lua_pop(lua, 1);
    auto columns = _samples(lua, xRange, 65536u, "x_range");
    auto rows = _samples(lua, yRange, 65536u, "y_range");
    if (columns < 2u || rows < 2u) luaL_error(lua, "contour needs at least two samples along each range");
    if (static_cast<uint64_t>(columns) * rows > 65536u) {
        luaL_error(lua, "a contour grid holds at most 65536 samples; widen the steps of x_range and y_range");
    }
    for (auto range : {xRange, yRange}) {
        auto n = _samples(lua, range, 65536u, "range");
        for (auto i = 1u; i < n; i++) {
            if (range.sampleAt(i) <= range.sampleAt(i - 1u)) {
                luaL_error(lua, "contour grid samples must have distinct coordinates");
            }
        }
    }
    auto count = columns * rows;
    auto available = _room(box).points;
    auto charged = grown ? *grown : 0u;
    if (charged > available || count > available - charged) {
        luaL_error(lua, "scene point limit exceeded: contour grids share the 1000000-point budget");
    }
    auto room = std::max(plot->count(), kept.room);
    auto endRoom = std::max(plot->contours() > 1u ? plot->contours() : 0u, kept.endRoom);
    auto budget = std::min<uint64_t>(LUA_POINT_LIMIT, static_cast<uint64_t>(room) + available - charged - count);
    lua_getfield(lua, index, function ? "fn" : "values");
    auto source = lua_gettop(lua);
    if (function && !lua_isfunction(lua, source)) luaL_error(lua, "fn must be a function");
    if (sampled && (!lua_istable(lua, source) || lua_rawlen(lua, source) != count)) {
        luaL_error(lua, "values must hold one finite number for each of the %d samples of x_range and y_range",
                   static_cast<int>(count));
    }
    auto values = static_cast<double*>(lua_newuserdatauv(lua, sizeof(double) * count, 0));
    LuaContour context{lua, box, source};
    for (auto i = 0u; i < count; i++) {
        auto x = xRange.sampleAt(i % columns), y = yRange.sampleAt(i / columns);
        if (sampled) {
            lua_rawgeti(lua, source, i + 1u);
            if (lua_type(lua, -1) != LUA_TNUMBER || !(std::fabs(lua_tonumber(lua, -1)) <= FLT_MAX)) {
                luaL_error(lua, "contour values must be finite numbers; at (%f, %f) one was not",
                           static_cast<lua_Number>(x), static_cast<lua_Number>(y));
            }
            values[i] = lua_tonumber(lua, -1);
            lua_pop(lua, 1);
        } else if (!_contourSample(x, y, values[i], &context)) _contourError(context);
    }
    auto owner = _owner(lua, box);
    Plot* traced = nullptr;
    auto result = Plot::contour(xRange, yRange, values, count, level, traced,
                               function ? _contourSample : nullptr, &context, static_cast<uint32_t>(budget));
    if (context.raised) _contourError(context);
    if (result == Result::NonSupport) luaL_error(lua, "scene point limit exceeded");
    if (result == Result::InvalidArguments && function) _contourError(context);
    owner->object = traced;
    if (result != Result::Success) luaL_error(lua, "could not trace contour: %s", _explain(result));
    auto vertices = traced->count(), pieces = traced->contours();
    auto points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * vertices, 0));
    if (vertices) std::memcpy(points, traced->points(), sizeof(Vec3) * vertices);
    lua_rawseti(lua, anchor, static_cast<lua_Integer>(lua_rawlen(lua, anchor)) + 1);
    uint32_t* ends = nullptr;
    if (pieces > 1u) {
        ends = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * pieces, 0));
        for (auto i = 0u; i < pieces; i++) ends[i] = traced->contourEnd(i);
        lua_rawseti(lua, anchor, static_cast<lua_Integer>(lua_rawlen(lua, anchor)) + 1);
    }
    auto grid = traced->contourGrid();
    target.geometry(points, vertices, ends, pieces, nullptr, &grid);
    if (auto labels = plot->contourLabels()) {
        result = traced->labelContours(*labels);
        if (result != Result::Success) luaL_error(lua, "could not label contour: %s", _explain(result));
        LuaCharge charge;
        for (auto i = plot->childCount(); i < traced->childCount(); i++) {
            if (!_treeCharge(traced->childAt(i), charge)) luaL_error(lua, "scene native memory limit exceeded");
        }
        _fit(lua, box, {0, charge.objects});
        _spend(box, {0, charge.objects});
        if (strings) *strings += static_cast<size_t>(charge.native);
    }
    if (strings) {
        *strings += sizeof(Vec3) * vertices + (pieces > 1u ? sizeof(uint32_t) * pieces : 0u);
        if (vertices > room) *strings += sizeof(Vec3) * (vertices - room);
        if (pieces > 1u && pieces > endRoom) *strings += sizeof(uint32_t) * (pieces - endRoom);
    }
    if (grown) *grown += count + (vertices > room ? vertices - room : 0u);
    lua_getfield(lua, index, "target");
    _contourRemember(lua, -1, xRange, yRange, level, std::max(room, vertices),
                      std::max(endRoom, pieces > 1u ? pieces : 0u));
    delete traced;
    owner->object = nullptr;
    lua_settop(lua, top);
    return true;
}

// Resamples a Plot descriptor with updated or retained ranges/discontinuities.
// Stores points/pieces in anchor userdata until core copies them; bytes charge strings,
// including expanded plot storage charged to grown. Returns whether fn/points was supplied.
static bool _replot(lua_State* lua, LuaScene* box, int index, const Plot* plot, AnimationTarget& target,
                    size_t* strings, uint32_t* grown, int anchor)
{
    auto top = lua_gettop(lua);
    lua_getfield(lua, index, "points");
    lua_getfield(lua, index, "fn");
    auto points = !lua_isnil(lua, top + 1);
    auto function = !lua_isnil(lua, top + 2);
    auto ranged = _has(lua, index, "x_range");
    auto banded = _has(lua, index, "y_range");
    auto broken = _has(lua, index, "discontinuities");
    // What the plot keeps of its cut, which stays on the stack while fn runs.
    lua_getfield(lua, index, "target");
    _sceneTable(lua, PLOT_REGISTRY, -1, false);
    lua_remove(lua, -2);
    if (lua_istable(lua, -1)) {
        lua_pushlightuserdata(lua, const_cast<Plot*>(plot));
        lua_rawget(lua, -2);
        lua_remove(lua, -2);
    }
    auto kept = static_cast<const LuaPlotRecord*>(lua_touserdata(lua, -1));
    if (kept && kept->contour) {
        auto redrawn = _recontour(lua, box, index, plot, *kept, target, strings, grown, anchor);
        lua_settop(lua, top);
        return redrawn;
    }
    if (_has(lua, index, "level") || _has(lua, index, "values")) luaL_error(lua, "level and values need a contour child");
    if (ranged && !function) luaL_error(lua, "x_range needs fn");
    if (!points && !function) {
        if (banded || broken) luaL_error(lua, "%s needs fn or points", banded ? "y_range" : "discontinuities");
        lua_settop(lua, top);
        return false;
    }
    if (points && function) luaL_error(lua, "a plot takes either points or fn, not both");
    if (points && !lua_istable(lua, top + 1)) luaL_error(lua, "points must be a table of points");
    if (function && !lua_isfunction(lua, top + 2) && !lua_istable(lua, top + 2)) luaL_error(lua, "fn must be a function");
    LuaPlotCut cut;
    cut.lua = lua;
    cut.scene = box;
    cut.fn = top + 2;
    if (banded) {
        _plotBand(lua, index, cut);
    } else if (kept && kept->band) {
        cut.band = true;
        cut.lo = kept->lo;
        cut.hi = kept->hi;
    }
    if (broken) {
        _plotDiscontinuities(lua, index, cut);
    } else if (kept && kept->broken) {
        cut.breaks = reinterpret_cast<const double*>(kept + 1);
        cut.breakCount = kept->breakCount;
        cut.broken = true;
    }
    Range range;
    uint32_t samples;
    if (function) {
        if (ranged) {
            range = _requiredRange(lua, index, "x_range", "plot fn");
        } else if (kept && kept->sampled) {
            range = kept->range;
        } else {
            luaL_error(lua, "plot fn requires x_range");
        }
        samples = _plotSamples(lua, range);
    } else {
        auto size = lua_rawlen(lua, top + 1);
        if (size > UINT32_MAX || size > LUA_MEMORY_LIMIT / sizeof(Vec3)) luaL_error(lua, "points array is too large");
        samples = static_cast<uint32_t>(size);
    }
    _plotCut(cut, points ? top + 1 : 0, range, samples);
    for (auto slot : {cut.pointSlot, cut.endSlot}) {
        lua_pushvalue(lua, slot);
        lua_rawseti(lua, anchor, static_cast<lua_Integer>(lua_rawlen(lua, anchor)) + 1);
    }
    target.geometry(cut.points, cut.count, cut.pieces > 1u ? cut.ends : nullptr, cut.pieces);
    if (function) {
        target.geometry(cut.points, cut.count, cut.pieces > 1u ? cut.ends : nullptr, cut.pieces, &cut.samples);
        lua_pushvalue(lua, -1);
        lua_rawseti(lua, anchor, static_cast<lua_Integer>(lua_rawlen(lua, anchor)) + 1);
    }
    // The plot keeps room for the most points and pieces it has held, so it grows only by those it
    // takes beyond them. What it shows holds no more than its room, which is all a plot that
    // never took a state of another count has held.
    auto room = std::max(plot->count(), kept ? kept->room : 0u);
    auto endRoom = std::max(plot->contours() > 1u ? plot->contours() : 0u, kept ? kept->endRoom : 0u);
    auto sampleRoom = std::max(plot->samples().count, kept ? kept->sampleRoom : 0u);
    if (strings) {
        *strings += sizeof(Vec3) * cut.samples.count;
        if (!kept || !kept->copied) *strings += sizeof(Vec3) * plot->samples().count;
        if (cut.samples.count > sampleRoom) *strings += sizeof(Vec3) * (cut.samples.count - sampleRoom);
        *strings += sizeof(Vec3) * cut.count;
        if (cut.pieces > 1u) *strings += sizeof(uint32_t) * cut.pieces;
        if (cut.count > room) {
            *strings += sizeof(Vec3) * (cut.count - room);
            if (grown) *grown += cut.count - room;
        }
        if (cut.pieces > 1u && cut.pieces > endRoom) *strings += sizeof(uint32_t) * (cut.pieces - endRoom);
    }
    lua_getfield(lua, index, "target");
    _sceneTable(lua, PLOT_REGISTRY, -1, true);
    lua_pushlightuserdata(lua, const_cast<Plot*>(plot));
    _plotKeep(lua, cut, function ? &range : kept && kept->sampled ? &kept->range : nullptr,
              std::max(room, cut.count), std::max(endRoom, cut.pieces > 1u ? cut.pieces : 0u),
              std::max(sampleRoom, cut.samples.count), true);
    lua_rawset(lua, -3);
    lua_settop(lua, top);
    return true;
}

// Reads factory-named geometry fields; omitted keys retain the latest authored geometry.
// Plot permits changed counts or fn resampling. Anchor userdata retains points until core
// copies them and charges their bytes to strings. Returns whether any field was supplied.
static bool _geometry(lua_State* lua, LuaScene* box, int index, Object* object, AnimationTarget& target,
                      size_t* strings, uint32_t* grown, int anchor)
{
    static constexpr const char* LINE[] = {"from", "to"};
    static constexpr const char* VECTOR[] = {"value", "origin"};
    static constexpr const char* CURVE[] = {"from", "control1", "control2", "to"};
    static constexpr const char* ARC[] = {"center"};
    static constexpr const char* ANGLE[] = {"vertex", "from", "to"};
    auto type = object->type();
    const char* const* keys = nullptr;
    auto count = 0u;
    switch (type) {
        case Type::Line:
        case Type::Arrow: keys = LINE; count = 2u; break;
        case Type::Vector: keys = VECTOR; count = 2u; break;
        case Type::Curve: keys = CURVE; count = 4u; break;
        case Type::Arc: keys = ARC; count = 1u; break;
        case Type::Angle: keys = ANGLE; count = 3u; break;
        case Type::Polygon: count = static_cast<Polygon*>(object)->count(); break;
        case Type::Route: count = static_cast<DirectedRoute*>(object)->count(); break;
        case Type::Path: count = static_cast<Path*>(object)->count(); break;
        default: break;
    }
    auto named = [keys, count](const char* key) {
        for (auto i = 0u; keys && i < count; i++) {
            if (std::strcmp(keys[i], key) == 0) return true;
        }
        return false;
    };
    auto listed = type == Type::Plot || (!keys && count);
    if (type != Type::Plot) {
        for (auto key : {"fn", "x_range", "y_range", "discontinuities", "level", "values"}) {
            if (_has(lua, index, key)) luaL_error(lua, "%s needs a plot", key);
        }
    }
    auto given = false;
    for (auto key : {"from", "to", "control1", "control2", "origin", "points", "center", "vertex"}) {
        if (!_has(lua, index, key)) continue;
        given = true;
        auto points = std::strcmp(key, "points") == 0;
        if (points ? listed : named(key)) continue;
        if (points) luaL_error(lua, "points needs a polygon, plot, route or path of one contour");
        if (key[0] == 'o') luaL_error(lua, "origin needs a vector");
        if (std::strcmp(key, "center") == 0) luaL_error(lua, "center needs an arc");
        if (std::strcmp(key, "vertex") == 0) luaL_error(lua, "vertex needs an angle");
        if (key[0] == 'c') luaL_error(lua, "%s needs a curve", key);
        luaL_error(lua, "%s needs a line, arrow, curve or angle; move other objects with shift", key);
    }
    if (type == Type::Plot) return _replot(lua, box, index, static_cast<Plot*>(object), target, strings, grown, anchor);
    if (type == Type::Vector && _has(lua, index, "value")) given = true;
    if (!given) return false;
    if (type == Type::Path && static_cast<Path*>(object)->contours() > 1u) {
        luaL_error(lua, "points cannot reshape a path of several contours");
    }
    auto buffer = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * count, 0));
    lua_rawseti(lua, anchor, static_cast<lua_Integer>(lua_rawlen(lua, anchor)) + 1);
    if (keys) {
        if (type == Type::Arc) {
            buffer[0] = static_cast<Arc*>(object)->center;
        } else if (type == Type::Angle) {
            auto angle = static_cast<Angle*>(object);
            buffer[0] = angle->vertex; buffer[1] = angle->from; buffer[2] = angle->to;
        } else if (type == Type::Curve) {
            std::memcpy(buffer, static_cast<Curve*>(object)->controls(), sizeof(Vec3) * count);
        } else if (type == Type::Vector) {
            buffer[0] = static_cast<Vector*>(object)->value;
            buffer[1] = static_cast<Vector*>(object)->origin;
        } else {
            auto line = type == Type::Line;
            buffer[0] = line ? static_cast<Line*>(object)->from : static_cast<Arrow*>(object)->from;
            buffer[1] = line ? static_cast<Line*>(object)->to : static_cast<Arrow*>(object)->to;
        }
        for (auto i = 0u; i < count; i++) buffer[i] = _fieldWorld(lua, index, keys[i], buffer[i]);
    } else {
        lua_getfield(lua, index, "points");
        if (!lua_istable(lua, -1)) luaL_error(lua, "points must be a table of points");
        auto size = lua_rawlen(lua, -1);
        if (size != count) {
            luaL_error(lua, "points must have %I points, as the %s has, not %I; a shape keeps its point count",
                       static_cast<lua_Integer>(count), vis::type(type), static_cast<lua_Integer>(size));
        }
        for (auto i = 0u; i < count; i++) {
            lua_rawgeti(lua, -1, static_cast<lua_Integer>(i) + 1);
            buffer[i] = _world(lua, -1, "point");
            lua_pop(lua, 1);
        }
        lua_pop(lua, 1);
    }
    target.geometry(buffer, count);
    if (strings) *strings += sizeof(Vec3) * count;
    return true;
}

// The values a number property of a play descriptor may take.
enum class LuaRange : uint8_t
{
    Any,
    NonNegative,
    Positive,
    Unit
};

// The number properties of a play descriptor that one setter each changes.
struct LuaNumberProperty
{
    const char* name;
    AnimationTarget& (AnimationTarget::*set)(float) noexcept;
    LuaRange range;
};

static constexpr LuaNumberProperty NUMBER_PROPERTIES[] = {
    {"opacity", &AnimationTarget::opacity, LuaRange::Unit},
    {"dash_offset", &AnimationTarget::dashOffset, LuaRange::Any},
    {"tail", &AnimationTarget::tail, LuaRange::NonNegative},
    {"tip", &AnimationTarget::tip, LuaRange::NonNegative},
    {"progress", &AnimationTarget::progress, LuaRange::Unit},
    {"width", &AnimationTarget::strokeWidth, LuaRange::NonNegative},
    {"start", &AnimationTarget::start, LuaRange::Any},
    {"angle", &AnimationTarget::angle, LuaRange::Any},
    {"radius", &AnimationTarget::radius, LuaRange::Positive},
    {"corner", &AnimationTarget::corner, LuaRange::NonNegative},
};

// Finds the first key of the table at index that is neither a state key nor one of more. Returns
// whether there is one, and then leaves the key with its name above it, or its type name for a
// key that is no string.
static bool _unknownKey(lua_State* lua, int index, std::initializer_list<const char*> more)
{
    index = lua_absindex(lua, index);
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        lua_pop(lua, 1);
        auto known = false;
        if (lua_type(lua, -1) == LUA_TSTRING) {
            auto name = lua_tostring(lua, -1);
            for (auto key : STATE_KEYS) known = known || std::strcmp(name, key) == 0;
            known = known || _option(name, more, false);
        }
        if (!known) {
            lua_pushstring(lua, lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : luaL_typename(lua, -1));
            return true;
        }
    }
    return false;
}

// Parses a play descriptor. A text change adds the bytes of its string to strings; the
// string stays in the descriptor, which the call's arguments hold until the scene copies it.
// The points of a geometry change go into the table at anchor (see _geometry), and those a plot
// may grow by add to grown.
static AnimationTarget _animationTarget(lua_State* lua, LuaScene* box, int index, Object*& object,
                                        int anchor, size_t* strings = nullptr, uint32_t* grown = nullptr)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    // A descriptor takes the state keys and the five of its own.
    if (_unknownKey(lua, index, {"target", "arc", "text", "retype", "layer"})) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "option names must be strings");
        luaL_error(lua, "unknown option '%s'", lua_tostring(lua, -1));
    }
    lua_getfield(lua, index, "target");
    if (lua_isnil(lua, -1)) luaL_error(lua, "animation descriptor requires a target");
    object = _object(lua, box->scene, -1);
    lua_pop(lua, 1);
    auto target = AnimationTarget::from(object);
    auto changed = false;
    auto shift = _has(lua, index, "shift");
    auto transform = _has(lua, index, "transform");
    if (shift && transform) luaL_error(lua, "animation descriptor cannot contain both shift and transform");
    if (shift) {
        lua_getfield(lua, index, "shift");
        target.shift(_world(lua, -1, "shift"));
        lua_pop(lua, 1);
        changed = true;
    }
    if (transform) {
        Mat4 matrix;
        lua_getfield(lua, index, "transform");
        _matrix(lua, -1, matrix.e, 16, "transform");
        lua_pop(lua, 1);
        target.transform(matrix);
        changed = true;
    }
    if (_has(lua, index, "arc")) {
        if (!shift && !transform) luaL_error(lua, "arc requires shift or transform");
        target.arc(_fieldArc(lua, index));
    }
    for (auto& property : NUMBER_PROPERTIES) {
        if (!_has(lua, index, property.name)) continue;
        auto value = _fieldNumber(lua, index, property.name, 0.0f);
        auto range = property.range;
        if (range == LuaRange::NonNegative && value < 0.0f) luaL_error(lua, "%s cannot be negative", property.name);
        if (range == LuaRange::Positive && value <= 0.0f) luaL_error(lua, "%s must be positive", property.name);
        if (range == LuaRange::Unit && (value < 0.0f || value > 1.0f)) {
            luaL_error(lua, "%s must be between 0 and 1", property.name);
        }
        (target.*property.set)(value);
        changed = true;
    }
    Color color;
    if (_fieldColor(lua, index, "stroke", color, &box->scene->theme())) {
        target.stroke(color);
        changed = true;
    }
    if (_fieldColor(lua, index, "fill", color, &box->scene->theme())) {
        target.fill(color);
        changed = true;
    }
    lua_getfield(lua, index, "size");
    if (lua_type(lua, -1) == LUA_TNUMBER) {
        auto size = _number(lua, -1, "size");
        if (size <= 0.0f) luaL_error(lua, "size must be positive");
        target.size(size);
        changed = true;
    } else if (lua_istable(lua, -1)) {
        auto size = _vec2(lua, -1, "size");
        if (size.x <= 0.0f || size.y <= 0.0f) luaL_error(lua, "size must have a positive width and height");
        target.size(size);
        changed = true;
    } else if (!lua_isnil(lua, -1)) {
        luaL_error(lua, "size must be a number for text or {width, height} for a rectangle");
    }
    lua_pop(lua, 1);
    if (_has(lua, index, "trim_distance")) {
        target.trimDistance(_trimDistance(lua, index));
        changed = true;
    }
    lua_getfield(lua, index, "gradient");
    if (lua_type(lua, -1) == LUA_TSTRING) {
        target.gradient(_color(lua, -1, "gradient", &box->scene->theme()));
        changed = true;
    } else if (lua_istable(lua, -1)) {
        target.gradient(_gradient(lua, -1, &box->scene->theme()));
        changed = true;
    } else if (!lua_isnil(lua, -1)) {
        luaL_error(lua, "gradient must be an end-stop color or a gradient table");
    }
    lua_pop(lua, 1);
    lua_getfield(lua, index, "blur");
    if (!lua_isnil(lua, -1)) {
        target.blur(_blur(lua, -1, "blur"));
        changed = true;
    }
    lua_pop(lua, 1);
    lua_getfield(lua, index, "shadow");
    if (!lua_isnil(lua, -1)) {
        target.shadow(_shadow(lua, -1, &box->scene->theme(), object->shadow));
        changed = true;
    }
    lua_pop(lua, 1);
    if (_geometry(lua, box, index, object, target, strings, grown, anchor)) changed = true;
    if (object->type() != Type::Vector && _has(lua, index, "value")) {
        if (object->type() != Type::Text || !static_cast<Text*>(object)->counting()) {
            luaL_error(lua, "value needs a number from scene:number, or a vector's value");
        }
        target.value(_fieldNumber(lua, index, "value", 0.0f));
        changed = true;
    }
    lua_pushstring(lua, "text");
    lua_rawget(lua, index);
    if (!lua_isnil(lua, -1)) {
        if (object->type() == Type::Group && static_cast<Group*>(object)->typeset) {
            luaL_error(lua, "text changes one text, and a spans row or other block of texts holds several; "
                            "target one of its texts");
        }
        if (object->type() != Type::Text) luaL_error(lua, "text needs a text or label");
        if (static_cast<Text*>(object)->counting()) {
            luaL_error(lua, "text cannot change a number from scene:number; animate its value");
        }
        // A number would turn into a string that only the stack holds.
        if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "text must be a string");
        size_t length;
        auto value = lua_tolstring(lua, -1, &length);
        if (std::strlen(value) != length) luaL_error(lua, "text cannot contain zero bytes");
        if (length > LUA_TEXT_LIMIT) luaL_error(lua, "text exceeds the 1 MiB limit");
        auto retype = _fieldBool(lua, index, "retype", false);
        target.text(value, retype ? TextChange::Retype : TextChange::Sweep);
        if (strings) *strings += length + 1u;
        changed = true;
    } else if (_has(lua, index, "retype")) {
        luaL_error(lua, "retype needs text");
    }
    lua_pop(lua, 1);
    if (_has(lua, index, "layer")) {
        auto layer = _fieldInteger(lua, index, "layer", 0);
        if (layer < INT32_MIN || layer > INT32_MAX) luaL_error(lua, "layer is out of range");
        target.layer(static_cast<int32_t>(layer));
        changed = true;
    }
    if (!changed) luaL_error(lua, "animation descriptor requires at least one property");
    return target;
}

// Stops the script when objects[i] is one of the objects before it.
static void _distinct(lua_State* lua, Object* const* objects, uint32_t i)
{
    for (auto j = 0u; j < i; j++) {
        if (objects[j] == objects[i]) luaL_error(lua, "animation targets must be distinct");
    }
}

// The objects at index, a handle or a list of at most LUA_GROUP_LIMIT handles, in userdata above
// the arguments; with distinct, no object may come twice.
static Object** _handles(lua_State* lua, Scene* scene, int index, uint32_t& count, bool distinct = false)
{
    if (!lua_istable(lua, index)) {
        // Checked first, so that the userdata cannot stand in for an absent handle.
        auto object = _object(lua, scene, index);
        auto objects = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*), 0));
        objects[0] = object;
        count = 1u;
        return objects;
    }
    auto size = lua_rawlen(lua, index);
    if (!size) luaL_error(lua, "handle array cannot be empty");
    if (size > LUA_GROUP_LIMIT) luaL_error(lua, "animation group limit exceeded");
    count = static_cast<uint32_t>(size);
    auto objects = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * count, 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, index, static_cast<lua_Integer>(i) + 1);
        objects[i] = _object(lua, scene, -1);
        lua_pop(lua, 1);
        if (distinct) _distinct(lua, objects, i);
    }
    return objects;
}

static int _playTargets(lua_State* lua)
{
    detail::luaTiming(lua, 3, 0, true);
    auto box = _scene(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto duration = _duration(lua, 3, 1.0f);
    auto curve = _curve(lua, 4);
    auto lag = _lag(lua, lua_isnoneornil(lua, 5) ? 0.0f : _number(lua, 5, "lag"));
    auto single = _has(lua, 2, "target");
    auto size = single ? size_t{1} : lua_rawlen(lua, 2);
    if (!size) return luaL_error(lua, "animation descriptor array cannot be empty");
    if (size > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    auto targets = static_cast<AnimationTarget*>(lua_newuserdatauv(lua, sizeof(AnimationTarget) * size, 0));
    auto objects = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * size, 0));
    lua_newtable(lua);
    auto anchor = lua_gettop(lua);
    size_t strings = 0;
    uint32_t grown = 0;
    for (auto i = 0u; i < size; i++) {
        if (!single) lua_rawgeti(lua, 2, i + 1u);
        auto index = single ? 2 : -1;
        auto target = _animationTarget(lua, box, index, objects[i], anchor, &strings, &grown);
        _distinct(lua, objects, i);
        new (targets + i) AnimationTarget(target);
        if (!single) lua_pop(lua, 1);
    }
    return _play(lua, box, targets, static_cast<uint32_t>(size), duration, curve, lag, strings, grown);
}

// scene:set(descriptor_or_array) gives objects the values of play descriptors at once: as their
// starting state before the timeline starts, and at the cursor afterwards.
static int _setTargets(lua_State* lua)
{
    auto box = _scene(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    if (!lua_isnone(lua, 3)) return luaL_error(lua, "set takes only descriptors; use play to animate the change");
    auto single = _has(lua, 2, "target");
    auto size = single ? size_t{1} : lua_rawlen(lua, 2);
    if (!size) return luaL_error(lua, "animation descriptor array cannot be empty");
    if (size > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    auto objects = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * size, 0));
    lua_newtable(lua);
    auto anchor = lua_gettop(lua);
    auto before = box->scene->clipCount();
    size_t strings = 0;
    uint32_t grown = 0;
    for (auto i = 0u; i < size; i++) {
        if (!single) lua_rawgeti(lua, 2, i + 1u);
        auto target = _animationTarget(lua, box, single ? 2 : -1, objects[i], anchor, &strings, &grown);
        _distinct(lua, objects, i);
        _fit(lua, box, {strings});
        _fit(lua, box, {0, 0, grown});
        _check(lua, box->scene, box->scene->update(target), "could not set values");
        if (!single) lua_pop(lua, 1);
    }
    return _charge(lua, box, before, strings, grown);
}

// The native bytes a save of object holds: a record of it and of each object below it, and a
// copy of the string each text among them shows, which a restore may copy again for the text.
// Like clips, saves stay charged after a restore takes them.
static uint64_t _saveCharge(const Object* object)
{
    uint64_t bytes = LUA_SAVE_CHARGE;
    if (object->type() == Type::Text) {
        auto text = static_cast<const Text*>(object);
        if (!text->counting() && text->text()) bytes += std::strlen(text->text()) + 1u;
    }
    for (auto i = 0u; i < object->childCount(); i++) bytes += _saveCharge(object->childAt(i));
    return bytes;
}

// scene:save(target_or_list) records the state each target shows at the cursor, with the objects
// below it, on a stack of its own, for scene:restore to bring back.
static int _save(lua_State* lua)
{
    auto box = _scene(lua);
    if (lua_gettop(lua) > 2) return luaL_error(lua, "save takes one handle or a list of handles");
    uint32_t count;
    auto targets = _handles(lua, box->scene, 2, count);
    uint64_t bytes = 0u;
    for (auto i = 0u; i < count; i++) bytes += _saveCharge(targets[i]);
    _fit(lua, box, {bytes});
    _check(lua, box->scene, box->scene->save(targets, count), "could not save");
    _spend(box, {bytes});
    return 0;
}

// scene:restore(target_or_list, duration, curve, lag) takes the newest state scene:save recorded
// for each target and brings the target and the objects below it back to it: over duration
// seconds, each later target lag of the duration later, or at the cursor without a duration.
static int _restore(lua_State* lua)
{
    detail::luaTiming(lua, 3, 0, true);
    auto box = _scene(lua);
    // The targets are stored above the arguments, so absent ones must read as nil.
    lua_settop(lua, 5);
    auto duration = _duration(lua, 3, 0.0f, true);
    auto curve = _curve(lua, 4);
    auto lag = _lag(lua, lua_isnil(lua, 5) ? 0.0f : _number(lua, 5, "lag"));
    uint32_t count;
    auto targets = _handles(lua, box->scene, 2, count);
    auto before = box->scene->clipCount();
    auto result = box->scene->restore(targets, count, duration, curve, lag);
    _check(lua, box->scene, result, "could not restore");
    return _charge(lua, box, before);
}

// ---------- Reveal, transform and warp animations ----------

static Animation _revealAnimation(Object* object, AnimationKind kind,
                                  DrawDirection direction)
{
    if (kind == AnimationKind::Create) return Animation::create(object, direction);
    if (kind == AnimationKind::Uncreate) return Animation::uncreate(object, direction);
    if (kind == AnimationKind::DrawBorderThenFill) {
        return Animation::drawBorderThenFill(object, direction);
    }
    if (kind == AnimationKind::Write) return Animation::write(object, direction);
    return Animation::fillReveal(object);
}

static int _reveal(lua_State* lua, AnimationKind kind)
{
    if (kind == AnimationKind::FillReveal) detail::luaTiming(lua, 3, 0, true);
    else if (detail::luaTiming(lua, 3, 6, true, {"direction"})) {
        lua_getfield(lua, 6, "direction");
        lua_replace(lua, 6);
    }
    auto box = _scene(lua);
    auto duration = _duration(lua, 3, 1.0f);
    auto curve = _curve(lua, 4);
    auto lag = lua_isnoneornil(lua, 5) ? 0.0f : _number(lua, 5, "lag");
    auto direction = kind != AnimationKind::FillReveal ? _direction(lua, 6) : DrawDirection::Forward;
    _lag(lua, lag);
    uint32_t count;
    auto objects = _handles(lua, box->scene, 2, count);
    auto animations = static_cast<Animation*>(lua_newuserdatauv(lua, sizeof(Animation) * count, 0));
    for (auto i = 0u; i < count; i++) new (animations + i) Animation(_revealAnimation(objects[i], kind, direction));
    return _play(lua, box, animations, count, duration, curve, lag);
}

static int _create(lua_State* lua)
{
    return _reveal(lua, AnimationKind::Create);
}

static int _uncreate(lua_State* lua)
{
    return _reveal(lua, AnimationKind::Uncreate);
}

static int _fillReveal(lua_State* lua)
{
    return _reveal(lua, AnimationKind::FillReveal);
}

static int _drawBorderThenFill(lua_State* lua)
{
    return _reveal(lua, AnimationKind::DrawBorderThenFill);
}

static int _write(lua_State* lua)
{
    return _reveal(lua, AnimationKind::Write);
}

static int _shift(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene*, Object* object) {
        return Animation::shift(object, _world(lua, 3, "shift"));
    });
}

static int _transform(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene*, Object* object) {
        Mat4 matrix;
        _matrix(lua, 3, matrix.e, 16, "matrix");
        return Animation::transform(object, matrix);
    });
}

// The timing a motion call takes from its options: seconds, the curve, and the lag after which
// each later target starts, as a share of the duration.
struct LuaMotion
{
    float duration = 1.0f;
    AnimCurve curve;
    float lag = 0.0f;
};

static LuaMotion _motionOptions(lua_State* lua, int index, std::initializer_list<const char*> fields)
{
    LuaMotion options;
    if (!_optionTable(lua, index, fields)) return options;
    options.duration = _fieldDuration(lua, index, "duration", options.duration);
    options.lag = _lag(lua, _fieldNumber(lua, index, "lag", options.lag));
    options.curve = _fieldCurve(lua, index);
    return options;
}

// One animation, with only its target, for each of the distinct handles at index; the arrays live
// on the Lua stack until the call returns.
static Animation* _motionTargets(lua_State* lua, Scene* scene, int index, uint32_t& count)
{
    auto objects = _handles(lua, scene, index, count, true);
    auto animations = static_cast<Animation*>(lua_newuserdatauv(lua, sizeof(Animation) * count, 0));
    for (auto i = 0u; i < count; i++) {
        new (animations + i) Animation();
        animations[i].target = objects[i];
    }
    return animations;
}

// Pivot of rotate and scale: a named anchor of the target's bounds, a parent-space point,
// or another object's center, which the engine maps into the target's parent space.
static void _about(lua_State* lua, Scene* scene, int index, Animation& animation)
{
    switch (lua_type(lua, index)) {
        case LUA_TNIL: return;
        case LUA_TTABLE:
            animation.pivot = _world(lua, index, "about");
            animation.centered = false;
            return;
        case LUA_TUSERDATA:
            animation.related = _object(lua, scene, index);
            animation.centered = false;
            return;
        case LUA_TSTRING: break;
        default: luaL_error(lua, "about must be an anchor name, a point or an object handle");
    }
    struct Anchor
    {
        const char* name;
        int x;
        int y;
    };
    static constexpr Anchor anchors[] = {
        {"center", 0, 0}, {"left", -1, 0}, {"right", 1, 0}, {"top", 0, 1}, {"bottom", 0, -1},
        {"top_left", -1, 1}, {"top_right", 1, 1}, {"bottom_left", -1, -1}, {"bottom_right", 1, -1}};
    size_t size;
    auto name = lua_tolstring(lua, index, &size);
    for (const auto& anchor : anchors) {
        if (std::strlen(anchor.name) != size || std::memcmp(anchor.name, name, size) != 0) continue;
        if (!anchor.x && !anchor.y) return;
        Bounds bounds;
        if (!animation.target->bounds(bounds)) luaL_error(lua, "about '%s' needs an object with bounds", anchor.name);
        auto center = bounds.center();
        animation.pivot = {anchor.x < 0 ? bounds.min.x : anchor.x > 0 ? bounds.max.x : center.x,
                           anchor.y < 0 ? bounds.min.y : anchor.y > 0 ? bounds.max.y : center.y, center.z};
        animation.centered = false;
        return;
    }
    luaL_error(lua, "about must be 'center', 'left', 'right', 'top', 'bottom', 'top_left', "
                    "'top_right', 'bottom_left' or 'bottom_right', a point, or an object handle");
}

// Shared rotate and scale options: the pivot, and a shift of that pivot along an optional arc.
static void _pivotOptions(lua_State* lua, Scene* scene, int index, Animation& animation)
{
    if (lua_isnoneornil(lua, index)) return;
    animation.offset = _fieldWorld(lua, index, "shift", {});
    animation.arc = _fieldArc(lua, index);
    lua_getfield(lua, index, "about");
    _about(lua, scene, -1, animation);
    lua_pop(lua, 1);
}

static int _rotate(lua_State* lua)
{
    auto box = _scene(lua);
    lua_settop(lua, 4);
    auto scene = box->scene;
    auto angle = _number(lua, 3, "angle");
    uint32_t count;
    auto animations = _motionTargets(lua, scene, 2, count);
    auto options = _motionOptions(lua, 4, {"about", "axis", "shift", "arc", "duration", "curve", "lag"});
    auto axis = Vec3{0.0f, 0.0f, 1.0f};
    if (!lua_isnoneornil(lua, 4)) axis = _fieldWorld(lua, 4, "axis", axis);
    if (!(axis.length() > 0.0f)) return luaL_error(lua, "axis must be a non-zero vector");
    for (auto i = 0u; i < count; i++) {
        animations[i] = Animation::rotate(animations[i].target, angle, axis);
        _pivotOptions(lua, scene, 4, animations[i]);
    }
    return _play(lua, box, animations, count, options.duration, options.curve, options.lag);
}

// Uniform factor, or per-axis factors where an omitted z keeps depth unchanged.
static Vec3 _factors(lua_State* lua, int index)
{
    if (lua_type(lua, index) == LUA_TNUMBER) {
        auto factor = _number(lua, index, "scale");
        return {factor, factor, factor};
    }
    if (!lua_istable(lua, index)) luaL_error(lua, "scale must be a number or a vector");
    auto factors = _world(lua, index, "scale");
    auto planar = _has(lua, index, "x") ? !_has(lua, index, "z") : lua_rawlen(lua, index) == 2u;
    if (planar) factors.z = 1.0f;
    return factors;
}

static int _scale(lua_State* lua)
{
    auto box = _scene(lua);
    lua_settop(lua, 4);
    auto scene = box->scene;
    auto factors = _factors(lua, 3);
    uint32_t count;
    auto animations = _motionTargets(lua, scene, 2, count);
    auto options = _motionOptions(lua, 4, {"about", "shift", "arc", "duration", "curve", "lag"});
    for (auto i = 0u; i < count; i++) {
        animations[i] = Animation::rescale(animations[i].target, factors);
        _pivotOptions(lua, scene, 4, animations[i]);
    }
    return _play(lua, box, animations, count, options.duration, options.curve, options.lag);
}

// A connector is a live path; ordinary outlines are copied when the clip is scheduled.
static int _moveAlong(lua_State* lua)
{
    auto box = _scene(lua);
    lua_settop(lua, 4);
    auto scene = box->scene;
    uint32_t count;
    auto animations = _motionTargets(lua, scene, 2, count);
    auto path = _object(lua, scene, 3);
    auto options = _motionOptions(lua, 4, {"from", "to", "orient", "duration", "curve", "lag"});
    auto from = 0.0f;
    auto to = 1.0f;
    auto orient = false;
    if (!lua_isnoneornil(lua, 4)) {
        from = _fieldNumber(lua, 4, "from", from);
        to = _fieldNumber(lua, 4, "to", to);
        orient = _fieldBool(lua, 4, "orient", orient);
        if (from < 0.0f || from > 1.0f || to < 0.0f || to > 1.0f) {
            return luaL_error(lua, "from and to must be fractions of the path length between 0 and 1");
        }
    }
    for (auto i = 0u; i < count; i++) {
        if (animations[i].target == path) return luaL_error(lua, "an object cannot move along itself");
        animations[i] = Animation::moveAlong(animations[i].target, path, orient);
        animations[i].from = from;
        animations[i].to = to;
    }
    return _play(lua, box, animations, count, options.duration, options.curve, options.lag);
}

// Runge-Kutta steps a frame of advect takes.
static constexpr uint32_t LUA_ADVECT_SUBSTEPS = 4;

// Bakes parent-space field advection as one linear shift/frame, using Runge-Kutta
// integration for speed * duration field time. Keeps the standard seekable timeline.
static int _advect(lua_State* lua)
{
    auto box = _scene(lua);
    lua_settop(lua, 4);
    auto scene = box->scene;
    uint32_t count;
    auto animations = _motionTargets(lua, scene, 2, count);
    if (!lua_isfunction(lua, 3)) return luaL_error(lua, "advect requires fn, a function of x and y");
    auto options = _motionOptions(lua, 4, {"duration", "speed", "curve"});
    auto timed = lua_istable(lua, 4) && _has(lua, 4, "curve");
    auto curve = timed ? options.curve : AnimCurve::preset(AnimCurvePreset::Linear);
    auto speed = lua_istable(lua, 4) ? _fieldNumber(lua, 4, "speed", 1.0f) : 1.0f;
    if (speed == 0.0f) return luaL_error(lua, "speed must be a nonzero number; a negative one runs the flow backward");
    // Where each target stands now, in its parent's space.
    auto points = static_cast<LuaFieldVector*>(lua_newuserdatauv(lua, sizeof(LuaFieldVector) * count, 0));
    auto moving = static_cast<bool*>(lua_newuserdatauv(lua, sizeof(bool) * count, 0));
    for (auto i = 0u; i < count; i++) {
        Bounds bounds;
        if (!animations[i].target->bounds(bounds)) {
            return luaL_error(lua, "advect moves objects by their bounds; target %d has none", static_cast<int>(i + 1u));
        }
        auto center = bounds.center();
        points[i] = {center.x, center.y};
        moving[i] = true;
    }
    auto frames = std::ceil(static_cast<double>(options.duration) * scene->config().fps - 1.0e-6);
    if (frames > LUA_CLIP_LIMIT) return luaL_error(lua, "scene animation limit exceeded: advect needs one sample per frame within the 65536-clip budget");
    auto steps = static_cast<uint32_t>(std::fmax(frames, 1.0));
    auto targets = static_cast<AnimationTarget*>(lua_newuserdatauv(lua, sizeof(AnimationTarget) * count, 0));
    auto span = static_cast<double>(speed) * options.duration;
    auto reached = 0.0;
    for (auto step = 1u; step <= steps; step++) {
        // The field time the curve reaches by the end of this frame.
        auto time = span * ease(curve, static_cast<float>(step) / static_cast<float>(steps));
        auto h = (time - reached) / LUA_ADVECT_SUBSTEPS;
        reached = time;
        auto played = 0u;
        for (auto i = 0u; i < count; i++) {
            if (!moving[i]) continue;
            auto x = points[i].x;
            auto y = points[i].y;
            // A target the field stops being finite at stays where it is from then on.
            for (auto sub = 0u; sub < LUA_ADVECT_SUBSTEPS && moving[i]; sub++) {
                LuaFieldVector start;
                moving[i] = _rungeKutta(lua, box, 3, x, y, h, "advect fn", start);
            }
            if (!moving[i]) continue;
            auto by = Vec3{static_cast<float>(x - points[i].x), static_cast<float>(y - points[i].y), 0.0f};
            points[i] = {x, y};
            if (by.x == 0.0f && by.y == 0.0f) continue;
            new (targets + played++) AnimationTarget(AnimationTarget::from(animations[i].target).shift(by));
        }
        auto duration = options.duration / static_cast<float>(steps);
        if (!played) {
            _check(lua, scene, scene->wait(duration), "could not wait");
            continue;
        }
        _play(lua, box, targets, played, duration, AnimCurve::preset(AnimCurvePreset::Linear), 0.0f);
    }
    return 0;
}

// Why a warp stopped mapping: an error fn raised, which is left on the Lua stack, a result of fn
// that is not a point, more points than the scene may keep, or a point outside the samples.
enum struct LuaWarpFailure : uint8_t
{
    None,
    Raised,
    Value,
    Limit,
    Outside
};

// The most images a warp takes sampled on a grid, 8 MB of them.
static constexpr uint32_t LUA_WARP_SAMPLE_LIMIT = 1048576;

// The images a warp takes sampled on a grid in place of fn: one for each sample of x_range and
// y_range, row by row from the bottom left, as the values of vector_field hold a field.
struct LuaWarpSamples
{
    Range x;
    Range y;
    uint32_t columns = 0;
    uint32_t rows = 0;
    const Vec2* values = nullptr;
};

// A pair of numbers a sampled warp adds up in double precision.
struct LuaWarpPair
{
    double x = 0.0;
    double y = 0.0;
};

// The weights of the four samples around share t of the interval between the middle two, on a
// Catmull-Rom spline.
static void _catmullRom(double t, double weights[4])
{
    auto t2 = t * t;
    auto t3 = t2 * t;
    weights[0] = 0.5 * (-t3 + 2.0 * t2 - t);
    weights[1] = 0.5 * (3.0 * t3 - 5.0 * t2 + 2.0);
    weights[2] = 0.5 * (-3.0 * t3 + 4.0 * t2 + t);
    weights[3] = 0.5 * (t3 - t2);
}

// The image samples give point: a Catmull-Rom spline through them along x and along y, which a
// map at most quadratic in x and in y, such as z -> z^2, follows exactly. Past an edge the spline
// leans on a sample continued from the three nearest, or in a straight line from two. A point
// keeps its z. False for a point outside the samples.
static bool _sampledImage(const LuaWarpSamples& samples, const Vec3& point, Vec3& image)
{
    auto locate = [](double value, const Range& range, uint32_t count, int& index, double& share) {
        auto at = (value - static_cast<double>(range.min)) / static_cast<double>(range.step);
        auto last = static_cast<double>(count - 1u);
        if (!(at >= -1.0e-4 && at <= last + 1.0e-4)) return false;
        at = std::fmin(std::fmax(at, 0.0), last);
        index = std::min(static_cast<int>(at), static_cast<int>(count) - 2);
        share = at - static_cast<double>(index);
        return true;
    };
    int column;
    int row;
    double across;
    double up;
    if (!locate(point.x, samples.x, samples.columns, column, across)) return false;
    if (!locate(point.y, samples.y, samples.rows, row, up)) return false;
    auto sample = [&samples](int i, int j) {
        auto& value = samples.values[static_cast<size_t>(j) * samples.columns + static_cast<size_t>(i)];
        return LuaWarpPair{value.x, value.y};
    };
    // One sample past an edge, from the edge sample and those after it.
    auto beyond = [](const LuaWarpPair& edge, const LuaWarpPair& next, const LuaWarpPair* after) {
        if (!after) return LuaWarpPair{2.0 * edge.x - next.x, 2.0 * edge.y - next.y};
        return LuaWarpPair{3.0 * edge.x - 3.0 * next.x + after->x, 3.0 * edge.y - 3.0 * next.y + after->y};
    };
    auto inColumn = [&](int i, int j) {
        auto last = static_cast<int>(samples.rows) - 1;
        if (j >= 0 && j <= last) return sample(i, j);
        auto edge = j < 0 ? 0 : last;
        auto inward = j < 0 ? 1 : -1;
        auto after = last >= 2 ? sample(i, edge + 2 * inward) : LuaWarpPair{};
        return beyond(sample(i, edge), sample(i, edge + inward), last >= 2 ? &after : nullptr);
    };
    auto at = [&](int i, int j) {
        auto last = static_cast<int>(samples.columns) - 1;
        if (i >= 0 && i <= last) return inColumn(i, j);
        auto edge = i < 0 ? 0 : last;
        auto inward = i < 0 ? 1 : -1;
        auto after = last >= 2 ? inColumn(edge + 2 * inward, j) : LuaWarpPair{};
        return beyond(inColumn(edge, j), inColumn(edge + inward, j), last >= 2 ? &after : nullptr);
    };
    double xWeights[4];
    double yWeights[4];
    _catmullRom(across, xWeights);
    _catmullRom(up, yWeights);
    LuaWarpPair sum;
    for (auto b = 0; b < 4; b++) {
        for (auto a = 0; a < 4; a++) {
            auto value = at(column - 1 + a, row - 1 + b);
            auto weight = xWeights[a] * yWeights[b];
            sum.x += weight * value.x;
            sum.y += weight * value.y;
        }
    }
    image = {static_cast<float>(sum.x), static_cast<float>(sum.y), point.z};
    return true;
}

// Reads the images a warp takes sampled, {x_range = r, y_range = r, values = {{x, y}, ...}}, from
// the table at index into samples, and leaves the copy they keep on the stack.
static void _warpSamples(lua_State* lua, int index, LuaWarpSamples& samples)
{
    index = lua_absindex(lua, index);
    _options(lua, index, {"x_range", "y_range", "values"}, false);
    lua_getfield(lua, index, "x_range");
    if (lua_isnil(lua, -1)) luaL_error(lua, "warp samples require x_range");
    samples.x = _range(lua, -1, "x_range");
    lua_pop(lua, 1);
    lua_getfield(lua, index, "y_range");
    if (lua_isnil(lua, -1)) luaL_error(lua, "warp samples require y_range");
    samples.y = _range(lua, -1, "y_range");
    lua_pop(lua, 1);
    samples.columns = _samples(lua, samples.x, LUA_WARP_SAMPLE_LIMIT, "x_range");
    samples.rows = _samples(lua, samples.y, LUA_WARP_SAMPLE_LIMIT, "y_range");
    if (samples.columns < 2u || samples.rows < 2u) {
        luaL_error(lua, "warp samples need at least two samples along x_range and two along y_range");
    }
    if (static_cast<uint64_t>(samples.columns) * samples.rows > LUA_WARP_SAMPLE_LIMIT) {
        luaL_error(lua, "warp samples hold at most %d images; widen the steps of x_range and y_range",
                   static_cast<int>(LUA_WARP_SAMPLE_LIMIT));
    }
    auto count = samples.columns * samples.rows;
    lua_getfield(lua, index, "values");
    if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != count) {
        luaL_error(lua, "warp values must hold one {x, y} for each of the %d samples of x_range and y_range",
                   static_cast<int>(count));
    }
    auto values = static_cast<Vec2*>(lua_newuserdatauv(lua, sizeof(Vec2) * count, 0));
    for (auto k = 0u; k < count; k++) {
        lua_rawgeti(lua, -2, static_cast<lua_Integer>(k) + 1);
        if (!lua_istable(lua, -1)) luaL_error(lua, "warp values must be {x, y} images");
        values[k] = _vec2(lua, -1, "a warp value");
        lua_pop(lua, 1);
    }
    lua_remove(lua, -2);
    samples.values = values;
}

// What a warp maps points with: warp fn, or the images it takes sampled; how many points it has
// mapped and how many more the scene may keep; and why it stopped, at which point.
struct LuaWarp
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    int callback = 0;
    const LuaWarpSamples* samples = nullptr;
    uint32_t points = 0;
    uint32_t budget = 0;
    LuaWarpFailure failure = LuaWarpFailure::None;
    Vec3 at;
};

// The image warp fn gives a point: x and y, keeping its z, or x, y and z, as numbers or a list;
// or the image its samples give. An image that is not finite leaves the scene to say where the
// map has none. Nothing here may raise a Lua error, so a failure waits for the warp to return.
static bool _warpPoint(const Vec3& point, Vec3& image, void* data) noexcept
{
    auto context = static_cast<LuaWarp*>(data);
    auto lua = context->lua;
    if (context->points >= context->budget) {
        context->failure = LuaWarpFailure::Limit;
        return false;
    }
    context->points++;
    if (context->samples) {
        if (_sampledImage(*context->samples, point, image)) return true;
        context->failure = LuaWarpFailure::Outside;
        context->at = point;
        return false;
    }
    lua_pushvalue(lua, context->callback);
    lua_pushnumber(lua, point.x);
    lua_pushnumber(lua, point.y);
    lua_pushnumber(lua, point.z);
    if (_sampleCall(lua, context->scene, 3, 3) != LUA_OK) {
        context->failure = LuaWarpFailure::Raised;
        return false;
    }
    auto listed = lua_istable(lua, -3);
    if (listed) {
        lua_rawgeti(lua, -3, 1);
        lua_rawgeti(lua, -4, 2);
        lua_rawgeti(lua, -5, 3);
    }
    auto depth = lua_type(lua, -1);
    auto numbers = lua_type(lua, -3) == LUA_TNUMBER && lua_type(lua, -2) == LUA_TNUMBER
                   && (depth == LUA_TNUMBER || depth == LUA_TNIL);
    double values[3] = {lua_tonumber(lua, -3), lua_tonumber(lua, -2),
                        depth == LUA_TNUMBER ? lua_tonumber(lua, -1) : static_cast<double>(point.z)};
    lua_pop(lua, listed ? 6 : 3);
    if (!numbers) {
        context->failure = LuaWarpFailure::Value;
        context->at = point;
        return false;
    }
    for (auto value : values) {
        if (!(std::fabs(value) <= FLT_MAX)) return false;
    }
    image = {static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2])};
    return true;
}

// Samples a parent-space map once per subdivided point and bakes straight travel to its image.
// Accepts a callback or a sampled grid from the JavaScript builder.
static int _warp(lua_State* lua)
{
    detail::luaTiming(lua, 4, 6, false, {"step"});
    auto box = _scene(lua);
    lua_settop(lua, 6);
    auto scene = box->scene;
    uint32_t count;
    auto targets = _handles(lua, scene, 2, count, true);
    LuaWarpSamples samples;
    auto sampled = lua_istable(lua, 3);
    if (!sampled && !lua_isfunction(lua, 3)) {
        return luaL_error(lua, "warp requires fn, a function of x, y and z that returns the image point, "
                               "or its images sampled as {x_range, y_range, values}");
    }
    auto duration = _duration(lua, 4, 1.0f);
    auto curve = _curve(lua, 5);
    auto step = 0.05f;
    if (_optionTable(lua, 6, {"step"})) {
        step = _fieldNumber(lua, 6, "step", step);
        if (!(step > 0.0f)) return luaL_error(lua, "warp step must be a positive length");
    }
    if (sampled) _warpSamples(lua, 3, samples);
    // Each point keeps where it starts and its image.
    auto perPoint = 2u * sizeof(Vec3);
    auto room = _room(box);
    LuaWarp context;
    context.lua = lua;
    context.scene = box;
    context.callback = 3;
    context.samples = sampled ? &samples : nullptr;
    context.budget = static_cast<uint32_t>(std::min<uint64_t>(room.points, room.native / perPoint));
    auto clips = scene->clipCount();
    auto result = scene->warp(targets, count, _warpPoint, &context, duration, curve, step);
    switch (context.failure) {
        case LuaWarpFailure::Raised: return lua_error(lua);
        case LuaWarpFailure::Value:
            return luaL_error(lua, "warp fn must return x and y, or x, y and z, as numbers or {x, y, z}; at (%f, %f, %f) it did not",
                              static_cast<lua_Number>(context.at.x), static_cast<lua_Number>(context.at.y),
                              static_cast<lua_Number>(context.at.z));
        case LuaWarpFailure::Limit:
            return luaL_error(lua, "scene point limit exceeded; raise the warp step or warp fewer objects");
        case LuaWarpFailure::Outside: {
            auto last = [](const Range& range, uint32_t count) {
                return static_cast<lua_Number>(range.sampleAt(count - 1u));
            };
            return luaL_error(lua, "the warp point (%f, %f, %f) lies outside its samples, "
                                   "x from %f to %f and y from %f to %f",
                              static_cast<lua_Number>(context.at.x), static_cast<lua_Number>(context.at.y),
                              static_cast<lua_Number>(context.at.z), static_cast<lua_Number>(samples.x.min),
                              last(samples.x, samples.columns), static_cast<lua_Number>(samples.y.min),
                              last(samples.y, samples.rows));
        }
        default: break;
    }
    _check(lua, scene, result, "could not warp");
    _spend(box, {context.points * perPoint, 0, context.points});
    return _charge(lua, box, clips);
}

// ---------- Fades, emphasis and morphs ----------

static int _fade(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene*, Object* object) {
        auto opacity = _number(lua, 3, "opacity");
        if (opacity < 0.0f || opacity > 1.0f) luaL_error(lua, "opacity must be between 0 and 1");
        return Animation::fade(object, opacity);
    });
}

static int _fadeEffect(lua_State* lua, bool fadeIn)
{
    auto box = _scene(lua);
    auto object = _object(lua, box->scene, 2);
    Vec3 shift;
    auto scale = 1.0f;
    auto duration = 1.0f;
    auto curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    if (_optionTable(lua, 3, {"shift", "scale", "duration", "curve"})) {
        shift = _fieldWorld(lua, 3, "shift", {});
        scale = _fieldNumber(lua, 3, "scale", scale);
        if (scale <= 0.0f) return luaL_error(lua, "scale must be positive");
        duration = _fieldDuration(lua, 3, "duration", duration);
        curve = _fieldCurve(lua, 3);
    }
    auto animation = fadeIn ? Animation::fadeIn(object, shift, scale) : Animation::fadeOut(object, shift, scale);
    return _play(lua, box, &animation, 1, duration, curve);
}

static int _fadeIn(lua_State* lua)
{
    return _fadeEffect(lua, true);
}

static int _fadeOut(lua_State* lua)
{
    return _fadeEffect(lua, false);
}

static int _growFromCenter(lua_State* lua)
{
    return _playOne(lua, 3, [](Scene*, Object* object) { return Animation::growFromCenter(object); });
}

static int _growFromEdge(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene*, Object* object) {
        return Animation::growFromEdge(object, _growthEdge(lua, 3));
    });
}

static int _shrinkToCenter(lua_State* lua)
{
    return _playOne(lua, 3, [](Scene*, Object* object) { return Animation::shrinkToCenter(object); });
}

// scene:indicate(target, {...}): a handle, or an array of handles, such as the tokens a search
// finds, which indicate together, each in its own place.
static int _indicate(lua_State* lua)
{
    auto box = _scene(lua);
    // Asked before the objects are stored above the arguments, where they could pass for options.
    auto styled = _optionTable(lua, 3, {"color", "scale", "duration", "curve"});
    uint32_t count;
    auto objects = _handles(lua, box->scene, 2, count);
    auto color = box->scene->theme().colors.focus;
    auto scale = 1.2f;
    auto duration = 1.0f;
    auto curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    if (styled) {
        _fieldColor(lua, 3, "color", color, &box->scene->theme());
        scale = _fieldNumber(lua, 3, "scale", scale);
        if (scale <= 1.0f) return luaL_error(lua, "indicate scale must be greater than 1");
        duration = _fieldDuration(lua, 3, "duration", duration);
        curve = _fieldCurve(lua, 3);
    }
    auto animations = static_cast<Animation*>(lua_newuserdatauv(lua, sizeof(Animation) * count, 0));
    for (auto i = 0u; i < count; i++) new (animations + i) Animation(Animation::indicate(objects[i], color, scale));
    return _play(lua, box, animations, count, duration, curve);
}

static int _wiggle(lua_State* lua)
{
    auto box = _scene(lua);
    auto object = _object(lua, box->scene, 2);
    auto scale = 1.1f;
    auto angle = 0.0628318531f;
    auto swings = 6.0f;
    auto duration = 2.0f;
    auto curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    if (_optionTable(lua, 3, {"scale", "angle", "swings", "duration", "curve"})) {
        scale = _fieldNumber(lua, 3, "scale", scale);
        if (scale <= 0.0f) return luaL_error(lua, "wiggle scale must be positive");
        angle = _fieldNumber(lua, 3, "angle", angle);
        swings = _fieldNumber(lua, 3, "swings", swings);
        if (swings <= 0.0f) return luaL_error(lua, "wiggle swings must be positive");
        duration = _fieldDuration(lua, 3, "duration", duration);
        curve = _fieldCurve(lua, 3);
    }
    auto animation = Animation::wiggle(object, scale, angle, swings);
    return _play(lua, box, &animation, 1, duration, curve);
}

// Plays an emphasis in the theme's focus color with the options of the table at 3 that fields
// lets through: the shape of a circumscribe, the color, width, padding, window and timing, and
// the size of a flash or a ripple and the lines of a flash; an option the kind does not take
// reads its default. Charges the shapes and clips it added, as adding and playing them would
// have; the scene is discarded when the script fails.
static int _emphasize(lua_State* lua, LuaScene* box, const char* name, Emphasis emphasis,
                      std::initializer_list<const char*> fields)
{
    auto scene = box->scene;
    emphasis.color = scene->theme().colors.focus;
    auto duration = 1.0f;
    // Ripple growth decelerates after its initial burst.
    auto curve = AnimCurve::preset(emphasis.kind == EmphasisKind::Ripple ? AnimCurvePreset::Snappy
                                                                          : AnimCurvePreset::Smooth);
    if (_optionTable(lua, 3, fields)) {
        static constexpr LuaName<EmphasisShape> SHAPES[] = {
            {"rectangle", EmphasisShape::Rectangle}, {"ellipse", EmphasisShape::Ellipse}};
        emphasis.shape = _fieldEnum(lua, 3, "shape", emphasis.shape, SHAPES, "circumscribe shape");
        _fieldColor(lua, 3, "color", emphasis.color, &scene->theme());
        emphasis.width = _fieldNumber(lua, 3, "width", emphasis.width);
        if (emphasis.width <= 0.0f) luaL_error(lua, "%s width must be positive", name);
        emphasis.padding = _fieldNumber(lua, 3, "padding", emphasis.padding);
        if (emphasis.padding < 0.0f) luaL_error(lua, "%s padding must be zero or more", name);
        emphasis.window = _fieldNumber(lua, 3, "window", emphasis.window);
        if (emphasis.window <= 0.0f || emphasis.window > 1.0f) {
            luaL_error(lua, "%s window must be greater than 0 and at most 1", name);
        }
        duration = _fieldDuration(lua, 3, "duration", duration);
        curve = _fieldCurve(lua, 3, curve);
        emphasis.size = _fieldNumber(lua, 3, "size", emphasis.size);
        if (emphasis.size <= 0.0f) luaL_error(lua, "%s size must be positive", name);
        auto lines = _fieldInteger(lua, 3, "lines", emphasis.count);
        if (lines < 1 || lines > 64) luaL_error(lua, "flash lines must be from 1 to 64");
        emphasis.count = static_cast<uint32_t>(lines);
    }
    auto objects = scene->count();
    auto clips = scene->clipCount();
    _check(lua, scene, scene->emphasize(emphasis, duration, curve), "could not play %s", name);
    LuaCharge charge;
    for (auto i = objects; i < scene->count(); i++) {
        uint32_t points;
        charge.native += _nativeSize(scene->objectAt(i), points);
        charge.points += points;
        charge.objects++;
    }
    _fit(lua, box, charge);
    _spend(box, charge);
    return _charge(lua, box, clips);
}

static int _circumscribe(lua_State* lua)
{
    auto box = _scene(lua);
    return _emphasize(lua, box, "circumscribe", Emphasis::circumscribe(_object(lua, box->scene, 2)),
                      {"shape", "color", "width", "padding", "window", "duration", "curve"});
}

static int _passingFlash(lua_State* lua)
{
    auto box = _scene(lua);
    return _emphasize(lua, box, "passing_flash", Emphasis::passingFlash(_object(lua, box->scene, 2)),
                      {"color", "width", "window", "duration", "curve"});
}

static int _flash(lua_State* lua)
{
    auto box = _scene(lua);
    // A flash bursts out of an object, or out of a point in world coordinates.
    auto emphasis = lua_type(lua, 2) == LUA_TTABLE ? Emphasis::flash(_world(lua, 2, "flash point"))
                                                   : Emphasis::flash(_object(lua, box->scene, 2));
    return _emphasize(lua, box, "flash", emphasis,
                      {"color", "width", "padding", "size", "lines", "duration", "curve"});
}

static int _ripple(lua_State* lua)
{
    auto box = _scene(lua);
    return _emphasize(lua, box, "ripple", Emphasis::ripple(_object(lua, box->scene, 2)),
                      {"color", "width", "padding", "size", "duration", "curve"});
}

// The steps of the progress of a morph of two surfaces at which their color functions are
// called again, at the points between the two; the colors blend linearly between them.
static constexpr uint32_t MORPH_COLOR_STEPS = 8;

// A morph of two surfaces on one grid calls the color function of each side that has one at the
// points between the two at MORPH_COLOR_STEPS + 1 even steps of its progress, so the colors
// follow the shape rather than fade from one end's to the other's. A side whose function is the
// other's shares its colors. The colors stay on the stack until the morph is played.
static void _morphColors(lua_State* lua, LuaScene* box, Animation& animation)
{
    auto source = animation.target;
    auto target = animation.related;
    if (!source || !target || source->type() != Type::SurfaceMesh || target->type() != Type::SurfaceMesh) return;
    auto from = static_cast<const SurfaceMesh*>(source);
    auto to = static_cast<const SurfaceMesh*>(target);
    // Surfaces on different grids are left to the play call to refuse.
    if (from->columns() != to->columns() || from->rows() != to->rows()) return;
    luaL_checkstack(lua, 8, "too many morphs");
    if (lua_getfield(lua, LUA_REGISTRYINDEX, SURFACE_COLORS) != LUA_TTABLE) {
        lua_pop(lua, 1);
        return;
    }
    auto table = lua_gettop(lua);
    lua_rawgetp(lua, table, from);
    lua_rawgetp(lua, table, to);
    auto count = from->columns() * from->rows();
    auto total = (MORPH_COLOR_STEPS + 1u) * count;
    Color* sides[2] = {};
    for (auto side = 0u; side < 2u; side++) {
        auto function = table + 1 + static_cast<int>(side);
        if (!lua_isfunction(lua, function)) continue;
        if (side == 1u && sides[0] && lua_rawequal(lua, table + 1, table + 2)) {
            sides[1] = sides[0];
            continue;
        }
        auto colors = static_cast<Color*>(lua_newuserdatauv(lua, sizeof(Color) * total, 0));
        for (auto step = 0u; step <= MORPH_COLOR_STEPS; step++) {
            auto progress = static_cast<float>(step) / static_cast<float>(MORPH_COLOR_STEPS);
            for (auto i = 0u; i < count; i++) {
                auto& a = from->points()[i];
                auto& b = to->points()[i];
                colors[step * count + i] = _callColors(lua, box, function, a + (b - a) * progress);
            }
        }
        sides[side] = colors;
    }
    // The colors stay; the table and the functions go.
    lua_remove(lua, table);
    lua_remove(lua, table);
    lua_remove(lua, table);
    if (!sides[0] && !sides[1]) return;
    animation.sourceColors = sides[0];
    animation.targetColors = sides[1];
    animation.colorSteps = MORPH_COLOR_STEPS;
    animation.colorCount = total;
}

static int _morph(lua_State* lua)
{
    detail::luaTiming(lua, 4, 6, true, {"lag", "copy", "pairs"});
    auto box = _scene(lua);
    auto sourceArray = lua_istable(lua, 2);
    auto targetArray = lua_istable(lua, 3);
    if (sourceArray != targetArray) {
        return luaL_error(lua, "morph source and target must both be handles or arrays");
    }

    auto duration = _duration(lua, 4, 1.0f);
    auto curve = _curve(lua, 5);
    // The lag, or options: with copy, a copy of each source morphs in its place.
    auto lag = 0.0f;
    auto copy = false;
    const uint32_t* pairs = nullptr;
    auto pairCount = 0u;
    if (lua_istable(lua, 6)) {
        _options(lua, 6, {"lag", "copy", "pairs"}, false);
        lag = _fieldNumber(lua, 6, "lag", 0.0f);
        copy = _fieldBool(lua, 6, "copy", false);
        lua_getfield(lua, 6, "pairs");
        if (!lua_isnil(lua, -1)) {
            if (!lua_istable(lua, -1)) return luaL_error(lua, "morph pairs must be an array of {source_index, target_index}");
            auto table = lua_gettop(lua);
            auto count = lua_rawlen(lua, table);
            if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
            auto indices = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * 2u * (count ? count : 1u), 0));
            for (auto i = 0u; i < count; i++) {
                lua_rawgeti(lua, table, i + 1u);
                if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2u) {
                    return luaL_error(lua, "morph pairs must be an array of {source_index, target_index}");
                }
                for (auto side = 0u; side < 2u; side++) {
                    lua_rawgeti(lua, -1, side + 1u);
                    auto index = _integer(lua, -1, "morph piece index");
                    if (index < 1 || index > UINT32_MAX) return luaL_error(lua, "morph piece index must be positive and within range");
                    indices[2u * i + side] = static_cast<uint32_t>(index - 1);
                    lua_pop(lua, 1);
                }
                lua_pop(lua, 1);
            }
            pairs = count ? indices : nullptr;
            pairCount = static_cast<uint32_t>(count);
        }
    } else if (!lua_isnoneornil(lua, 6)) {
        lag = _number(lua, 6, "lag");
    }
    _lag(lua, lag);

    // Two handles morph as a pair, and two arrays pair by pair.
    auto size = sourceArray ? lua_rawlen(lua, 2) : size_t{1};
    if (!size || (sourceArray && size != lua_rawlen(lua, 3))) {
        return luaL_error(lua, "morph arrays must be non-empty and equally sized");
    }
    if (size > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    LuaCharge copies;
    auto animations = static_cast<Animation*>(lua_newuserdatauv(lua, sizeof(Animation) * size, 0));
    for (auto i = 0u; i < size; i++) {
        if (sourceArray) {
            lua_rawgeti(lua, 2, i + 1u);
            lua_rawgeti(lua, 3, i + 1u);
        } else {
            lua_pushvalue(lua, 2);
            lua_pushvalue(lua, 3);
        }
        auto source = _object(lua, box->scene, -2);
        auto target = _object(lua, box->scene, -1);
        lua_pop(lua, 2);
        auto animation = Animation::morph(source, target);
        animation.copy = copy;
        animation.pairs = pairs;
        animation.pairCount = pairCount;
        if (copy && !_treeCharge(source, copies)) return luaL_error(lua, "the copy is too deep or large");
        _morphColors(lua, box, animation);
        new (animations + i) Animation(animation);
    }
    return _play(lua, box, animations, static_cast<uint32_t>(size), duration, curve, lag, copies);
}

static int _fadeTransform(lua_State* lua)
{
    detail::luaTiming(lua, 4);
    auto box = _scene(lua);
    auto source = _object(lua, box->scene, 2);
    auto target = _object(lua, box->scene, 3);
    auto duration = _duration(lua, 4, 1.0f);
    auto charge = _clipCharge(2u);
    _fit(lua, box, charge);
    auto result = box->scene->fadeTransform(source, target, duration, _curve(lua, 5));
    _check(lua, box->scene, result, "could not fade transform");
    _spend(box, charge);
    return _evaluate(lua, box);
}

// Pieces of a match: groups open into their children; every other object is one piece.
static uint32_t _pieceCount(const Object* object)
{
    if (object->type() != Type::Group) return 1u;
    auto count = 0u;
    for (auto i = 0u; i < object->childCount(); i++) count += _pieceCount(object->childAt(i));
    return count;
}

// scene:match_transform(source, target, duration, curve, {arc = 0, lag = 0, pairs = {{from, to}},
//                       strings = true, diff = false, fade = 1, copy = false})
static int _matchTransform(lua_State* lua)
{
    detail::luaTiming(lua, 4, 6, true, {"arc", "lag", "pairs", "strings", "diff", "shapes", "fade", "copy", "path"});
    auto box = _scene(lua);
    auto scene = box->scene;
    auto source = _object(lua, scene, 2);
    auto target = _object(lua, scene, 3);
    auto duration = _duration(lua, 4, 1.0f);
    auto curve = _curve(lua, 5);
    MatchOptions options;
    if (_optionTable(lua, 6, {"arc", "lag", "pairs", "strings", "diff", "shapes", "fade", "copy", "path"})) {
        options.arc = _fieldArc(lua, 6);
        lua_getfield(lua, 6, "path");
        if (!lua_isnil(lua, -1)) options.path = _object(lua, scene, -1);
        lua_pop(lua, 1);
        options.strings = _fieldBool(lua, 6, "strings", true);
        options.diff = _fieldBool(lua, 6, "diff", false);
        options.shapes = _fieldBool(lua, 6, "shapes", false);
        options.copy = _fieldBool(lua, 6, "copy", false);
        options.lag = _lag(lua, _fieldNumber(lua, 6, "lag", 0.0f));
        options.fade = _fieldNumber(lua, 6, "fade", 1.0f);
        if (!(options.fade > 0.0f && options.fade <= 1.0f)) {
            return luaL_error(lua, "fade must be a share of the duration greater than 0 and at most 1");
        }
        lua_getfield(lua, 6, "pairs");
        if (!lua_isnil(lua, -1)) {
            if (!lua_istable(lua, -1)) return luaL_error(lua, "pairs must be an array of {source_piece, target_piece}");
            auto table = lua_gettop(lua);
            auto count = lua_rawlen(lua, table);
            if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
            auto pairs = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * 2u * (count ? count : 1u), 0));
            for (auto i = 0u; i < count; i++) {
                lua_rawgeti(lua, table, static_cast<lua_Integer>(i) + 1);
                if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2u) {
                    return luaL_error(lua, "pairs must be an array of {source_piece, target_piece}");
                }
                lua_rawgeti(lua, -1, 1);
                pairs[2u * i] = _object(lua, scene, -1);
                lua_rawgeti(lua, -2, 2);
                pairs[2u * i + 1u] = _object(lua, scene, -1);
                lua_pop(lua, 3);
            }
            options.pairs = pairs;
            options.pairCount = static_cast<uint32_t>(count);
        }
    }
    auto sourcePieces = _pieceCount(source);
    auto targetPieces = _pieceCount(target);
    if (sourcePieces > LUA_GROUP_LIMIT || targetPieces > LUA_GROUP_LIMIT) {
        return luaL_error(lua, "animation group limit exceeded");
    }
    auto count = sourcePieces + targetPieces;
    _fit(lua, box, {0, 0, 0, count});
    // A match from a copy adds a clone of source first.
    LuaCharge copies;
    if (options.copy && !_treeCharge(source, copies)) return luaL_error(lua, "the copy is too deep or large");
    _fit(lua, box, {copies.native + uint64_t{count} * LUA_CLIP_CHARGE, copies.objects, copies.points});
    auto before = scene->clipCount();
    auto result = scene->matchTransform(source, target, duration, curve, options);
    _check(lua, scene, result, "could not match transform");
    _spend(box, copies);
    return _charge(lua, box, before);
}

// ---------- Code edits, highlights and group transitions ----------

// The kinds of edit, each named by a key of its edit table, and the keys each kind takes.
static constexpr const char* CODE_EDITS[] = {"replace", "insert", "remove", "append", "prepend"};
static constexpr const char* CODE_EDIT_KEYS[][6] = {
    {"replace", "with", "match", "lines", "whole", nullptr},
    {"insert", "before", "after", "match", "lines", "whole"},
    {"remove", "match", "lines", "whole", nullptr, nullptr},
    {"append", nullptr, nullptr, nullptr, nullptr, nullptr},
    {"prepend", nullptr, nullptr, nullptr, nullptr, nullptr}};

// Pushes text with value set in front of line number, counting from 1, or after the last line
// when number is one past it, as whole lines: a line feed at the end of value opens none.
static void _codeInsertLines(lua_State* lua, const char* text, size_t length, uint32_t number, const char* value,
                             size_t size)
{
    if (size && value[size - 1u] == '\n') size--;
    luaL_Buffer buffer;
    luaL_buffinit(lua, &buffer);
    if (number <= _codeLineCount(text, length)) {
        auto at = _codeLineStart(text, length, number);
        luaL_addlstring(&buffer, text, at);
        luaL_addlstring(&buffer, value, size);
        luaL_addchar(&buffer, '\n');
        luaL_addlstring(&buffer, text + at, length - at);
    } else if (length && text[length - 1u] == '\n') {
        luaL_addlstring(&buffer, text, length);
        luaL_addlstring(&buffer, value, size);
        luaL_addchar(&buffer, '\n');
    } else {
        luaL_addlstring(&buffer, text, length);
        luaL_addchar(&buffer, '\n');
        luaL_addlstring(&buffer, value, size);
    }
    luaL_pushresult(&buffer);
}

// Pushes text with lines first to last, counting from 1, replaced by value as whole lines, or
// removed with their line feeds when value is null.
static void _codeReplaceLines(lua_State* lua, const char* text, size_t length, uint32_t first, uint32_t last,
                              const char* value, size_t size)
{
    auto lines = _codeLineCount(text, length);
    auto body = length && text[length - 1u] == '\n' ? length - 1u : length;
    size_t at;
    size_t resume;
    if (value) {
        if (size && value[size - 1u] == '\n') size--;
        at = _codeLineStart(text, length, first);
        resume = last < lines ? _codeLineStart(text, length, last + 1u) - 1u : body;
    } else if (last < lines) {
        at = _codeLineStart(text, length, first);
        resume = _codeLineStart(text, length, last + 1u);
    } else {
        // The last line goes with the line feed before it, which ends the line above.
        at = first > 1u ? _codeLineStart(text, length, first) - 1u : 0u;
        resume = first > 1u ? body : length;
    }
    luaL_Buffer buffer;
    luaL_buffinit(lua, &buffer);
    luaL_addlstring(&buffer, text, at);
    if (value) luaL_addlstring(&buffer, value, size);
    luaL_addlstring(&buffer, text + resume, length - resume);
    luaL_pushresult(&buffer);
}

// Applies the edit table at index to the text at working, which the edited text replaces.
static void _codeEdit(lua_State* lua, int edit, int working, detail::CodeLanguage language, const char* prefix)
{
    auto kind = -1;
    for (auto i = 0; i < 5; i++) {
        if (!_has(lua, edit, CODE_EDITS[i])) continue;
        if (kind >= 0) luaL_error(lua, "%san edit takes one of replace, insert, remove, append and prepend", prefix);
        kind = i;
    }
    if (kind < 0) luaL_error(lua, "%san edit takes one of replace, insert, remove, append and prepend", prefix);
    lua_pushnil(lua);
    while (lua_next(lua, edit)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "%soption names must be strings", prefix);
        auto name = lua_tostring(lua, -2);
        auto known = false;
        for (auto key : CODE_EDIT_KEYS[kind]) known = known || (key && std::strcmp(name, key) == 0);
        if (!known) luaL_error(lua, "%sunknown option '%s' for %s", prefix, name, CODE_EDITS[kind]);
        lua_pop(lua, 1);
    }
    size_t length;
    auto text = lua_tolstring(lua, working, &length);
    auto lines = _codeLineCount(text, length);

    // The text the edit puts in: with for replace, the value of insert, append, or prepend.
    size_t size = 0;
    const char* value = nullptr;
    lua_getfield(lua, edit, kind == 0 ? "with" : CODE_EDITS[kind]);
    if (kind != 2) {
        if (lua_type(lua, -1) != LUA_TSTRING) {
            if (kind == 0) luaL_error(lua, "%sreplace needs with, the text to put in its place", prefix);
            luaL_error(lua, "%s%s takes a string", prefix, CODE_EDITS[kind]);
        }
        value = lua_tolstring(lua, -1, &size);
    }
    if (kind == 3 || kind == 4) {
        _codeInsertLines(lua, text, length, kind == 3 ? lines + 1u : 1u, value, size);
        lua_replace(lua, working);
        return;
    }

    // The target: a text to find, or for insert a line number and for replace and remove lines.
    auto after = false;
    if (kind == 1) {
        auto before = _has(lua, edit, "before");
        after = _has(lua, edit, "after");
        if (before == after) luaL_error(lua, "%sinsert needs one of before and after", prefix);
    }
    lua_getfield(lua, edit, kind == 1 ? (after ? "after" : "before") : CODE_EDITS[kind]);
    auto target = lua_gettop(lua);
    if (lua_type(lua, target) != LUA_TSTRING) {
        if (_has(lua, edit, "match") || _has(lua, edit, "lines") || _has(lua, edit, "whole")) {
            luaL_error(lua, "%smatch, lines and whole need a text to find, not line numbers", prefix);
        }
        char message[96];
        if (kind == 1) std::snprintf(message, sizeof(message), "%sbefore and after take a text to find or a line number", prefix);
        else std::snprintf(message, sizeof(message), "%s%s takes a text to find, a line number or {first, last}", prefix, CODE_EDITS[kind]);
        if (kind == 1 && lua_type(lua, target) != LUA_TNUMBER) luaL_error(lua, "%s", message);
        uint32_t first;
        uint32_t last;
        _codeLines(lua, target, first, last, message);
        if (last > lines) {
            luaL_error(lua, "%sline %d is past the last of the code's %d lines", prefix, static_cast<int>(last),
                       static_cast<int>(lines));
        }
        if (kind == 1) _codeInsertLines(lua, text, length, after ? first + 1u : first, value, size);
        else _codeReplaceLines(lua, text, length, first, last, value, size);
        lua_replace(lua, working);
        return;
    }
    LuaCodeQuery query;
    query.text = lua_tolstring(lua, target, &query.length);
    if (!query.length) luaL_error(lua, "%sthe text to find cannot be empty", prefix);
    _codeQuery(lua, edit, query);
    uint32_t spanCount = 0;
    auto spans = query.whole ? _codeSpans(lua, language, text, length, spanCount) : nullptr;
    auto failures = _codeFailures(lua, query);
    luaL_Buffer buffer;
    luaL_buffinit(lua, &buffer);
    size_t cursor = 0;
    _codeFind(lua, text, length, query, failures, spans, spanCount, prefix, [&](size_t found) {
        auto end = found + query.length;
        // insert after keeps the match before the text and insert before after it.
        luaL_addlstring(&buffer, text + cursor, (kind == 1 && after ? end : found) - cursor);
        if (value) luaL_addlstring(&buffer, value, size);
        if (kind == 1 && !after) luaL_addlstring(&buffer, text + found, query.length);
        cursor = end;
    });
    luaL_addlstring(&buffer, text + cursor, length - cursor);
    luaL_pushresult(&buffer);
    lua_replace(lua, working);
}

// Pushes the id of a block edited from the block with id: the id without a number after its
// last point, then a point and the first number from 2 that no object's id has, as listing.2.
static void _codeNextId(lua_State* lua, const Scene* scene, const char* id)
{
    auto length = std::strlen(id);
    auto digits = length;
    while (digits > 0 && id[digits - 1u] >= '0' && id[digits - 1u] <= '9') digits--;
    auto base = digits < length && digits > 0 && id[digits - 1u] == '.' ? digits - 1u : length;
    for (auto number = 2;; number++) {
        lua_pushlstring(lua, id, base);
        lua_pushfstring(lua, ".%d", number);
        lua_concat(lua, 2);
        if (!scene->object(lua_tostring(lua, -1))) return;
        lua_pop(lua, 1);
    }
}

// scene:edit(block, edits, duration, curve, {id, arc, lag, fade}): makes the code block that the
// source of block turns into after edits, which apply in order, each to the text the one before
// left, with the look and place of block, and carries block into it as match_transform with
// diff = true does. The new block joins the parent of block in its transform, at the cursor.
static int _edit(lua_State* lua)
{
    detail::luaTiming(lua, 4, 6, true, {"id", "arc", "lag", "fade"});
    auto box = _scene(lua);
    auto scene = box->scene;
    auto source = _object(lua, scene, 2);
    luaL_checktype(lua, 3, LUA_TTABLE);
    lua_settop(lua, 6);
    _optionTable(lua, 6, {"id", "arc", "lag", "fade"});
    _codeSource(lua, 2, "edit");
    auto entry = lua_gettop(lua);
    auto language = _codeLanguageOf(lua, entry);
    lua_getfield(lua, entry, "text");
    auto working = lua_gettop(lua);

    // One edit table, or a list of them.
    auto count = lua_rawlen(lua, 3);
    auto list = count > 0u;
    if (list) {
        lua_pushnil(lua);
        while (lua_next(lua, 3)) {
            auto key = lua_isinteger(lua, -2) ? lua_tointeger(lua, -2) : 0;
            if (key < 1 || static_cast<size_t>(key) > count || !lua_istable(lua, -1)) {
                return luaL_error(lua, "edit takes an edit table or a list of edit tables");
            }
            lua_pop(lua, 1);
        }
        if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "edit takes at most %d edits", static_cast<int>(LUA_GROUP_LIMIT));
    } else {
        count = 1u;
    }
    for (size_t i = 1; i <= count; i++) {
        if (list) lua_rawgeti(lua, 3, static_cast<lua_Integer>(i));
        else lua_pushvalue(lua, 3);
        char prefix[32] = "";
        if (count > 1u) std::snprintf(prefix, sizeof(prefix), "edit %d: ", static_cast<int>(i));
        _codeEdit(lua, lua_gettop(lua), working, language, prefix);
        size_t length;
        lua_tolstring(lua, working, &length);
        if (length > LUA_TEXT_LIMIT) return luaL_error(lua, "%sthe code would exceed the 1 MiB text limit", prefix);
        lua_settop(lua, working);
    }

    // The options of the new block: the kept ones of block, the edited text, and an id.
    lua_createtable(lua, 0, 16);
    auto options = lua_gettop(lua);
    lua_getfield(lua, entry, "options");
    for (auto name : CODE_KEPT) {
        lua_getfield(lua, -1, name);
        lua_setfield(lua, options, name);
    }
    lua_pop(lua, 1);
    lua_pushvalue(lua, working);
    lua_setfield(lua, options, "text");
    if (lua_isnil(lua, 6)) lua_pushnil(lua);
    else lua_getfield(lua, 6, "id");
    if (lua_isnil(lua, -1) && source->tag()) {
        lua_pop(lua, 1);
        _codeNextId(lua, scene, source->tag());
    }
    lua_setfield(lua, options, "id");

    auto model = source->model;
    lua_pushlightuserdata(lua, &model);
    lua_pushcclosure(lua, _codeMoved, 1);
    if (auto parent = source->parent()) _knownHandle(lua, box, 1, parent);
    else lua_pushvalue(lua, 1);
    lua_pushvalue(lua, options);
    detail::luaCall(lua, 2, 1);

    // The match runs in this call, as match_transform(scene, block, new block, duration, curve,
    // options) would, so that its errors name the script's line and edit.
    lua_replace(lua, 3);
    lua_createtable(lua, 0, 4);
    lua_pushboolean(lua, 1);
    lua_setfield(lua, -2, "diff");
    if (!lua_isnil(lua, 6)) {
        for (auto name : {"arc", "lag", "fade"}) {
            lua_getfield(lua, 6, name);
            lua_setfield(lua, -2, name);
        }
    }
    lua_replace(lua, 6);
    lua_settop(lua, 6);
    _matchTransform(lua);
    lua_settop(lua, 3);
    return 1;
}

// scene:highlight(target, first, last, {dim = 0.25, duration = 0.5, curve = "smooth"}), with
// first and last counting target's children from 1; or a handle or a list of handles below
// target in place of first and last; or neither, to bring everything back.
static int _highlight(lua_State* lua)
{
    auto box = _scene(lua);
    auto scene = box->scene;
    auto target = _object(lua, scene, 2);
    // The selection is stored above the arguments, so absent ones must read as nil.
    lua_settop(lua, 5);
    auto optionsIndex = 4;
    Object** selection = nullptr;
    uint32_t count = 0;
    if (lua_type(lua, 3) == LUA_TNUMBER) {
        auto children = static_cast<lua_Integer>(target->childCount());
        auto first = lua_isinteger(lua, 3) ? lua_tointeger(lua, 3) : 0;
        auto last = first;
        if (lua_type(lua, 4) == LUA_TNUMBER) {
            last = lua_isinteger(lua, 4) ? lua_tointeger(lua, 4) : 0;
            optionsIndex = 5;
        }
        if (first < 1 || last < first || last > children) {
            return luaL_error(lua, "first and last must count the target's children from 1 to %d, first no later than last",
                              static_cast<int>(children));
        }
        count = static_cast<uint32_t>(last - first + 1);
        selection = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * count, 0));
        for (auto i = 0u; i < count; i++) selection[i] = target->childAt(static_cast<uint32_t>(first - 1) + i);
    } else if (lua_type(lua, 3) == LUA_TUSERDATA) {
        selection = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*), 0));
        selection[0] = _object(lua, scene, 3);
        count = 1;
    } else if (lua_istable(lua, 3) && lua_rawlen(lua, 3) > 0) {
        auto size = lua_rawlen(lua, 3);
        if (size > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
        count = static_cast<uint32_t>(size);
        selection = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * count, 0));
        lua_pushnil(lua);
        while (lua_next(lua, 3)) {
            auto key = lua_isinteger(lua, -2) ? lua_tointeger(lua, -2) : 0;
            if (key < 1 || static_cast<size_t>(key) > size) {
                return luaL_error(lua, "a highlight takes a list of handles, and its options in a table of their own");
            }
            selection[key - 1] = _object(lua, scene, -1);
            lua_pop(lua, 1);
        }
    } else if (lua_istable(lua, 3)) {
        optionsIndex = 3;
    } else if (!lua_isnoneornil(lua, 3)) {
        return luaL_error(lua, "a highlight selects children by number, a handle or a list of handles");
    }
    auto dim = 0.25f;
    auto duration = 0.5f;
    auto curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    if (_optionTable(lua, optionsIndex, {"dim", "duration", "curve"})) {
        dim = _fieldNumber(lua, optionsIndex, "dim", dim);
        if (dim < 0.0f || dim > 1.0f) return luaL_error(lua, "dim must be an opacity from 0 to 1");
        duration = _fieldDuration(lua, optionsIndex, "duration", duration);
        curve = _fieldCurve(lua, optionsIndex);
    }
    // Every object below the target that is not a group may change.
    auto pieces = _pieceCount(target);
    if (pieces > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
    _fit(lua, box, _clipCharge(pieces));
    auto before = scene->clipCount();
    auto result = scene->highlight(target, selection, count, dim, duration, curve);
    _check(lua, scene, result, "could not highlight");
    return _charge(lua, box, before);
}

// scene:arrange(items, duration, curve, {direction = {1, 0}, gap = 0.25, center = {0, 0}}): lays
// items, a list of sibling handles in order or a group whose children they are, one after
// another over duration; an item added at the cursor grows into its place.
static int _arrangeOver(lua_State* lua)
{
    detail::luaTiming(lua, 3, 5, false, {"direction", "gap", "center"});
    auto box = _scene(lua);
    auto scene = box->scene;
    // The items are stored above the arguments, so absent ones must read as nil.
    lua_settop(lua, 5);
    Object** items = nullptr;
    uint32_t count = 0;
    if (lua_type(lua, 2) == LUA_TUSERDATA) {
        auto group = _object(lua, scene, 2);
        if (group->type() != Type::Group) return luaL_error(lua, "arrange takes a group or a list of handles");
        count = group->childCount();
        if (!count) return luaL_error(lua, "arrange needs a group with children");
        if (count > LUA_GROUP_LIMIT) return luaL_error(lua, "animation group limit exceeded");
        items = static_cast<Object**>(lua_newuserdatauv(lua, sizeof(Object*) * count, 0));
        for (auto i = 0u; i < count; i++) items[i] = group->childAt(i);
    } else if (lua_istable(lua, 2) && lua_rawlen(lua, 2) > 0) {
        items = _handles(lua, scene, 2, count);
    } else {
        return luaL_error(lua, "arrange takes a group or a list of handles");
    }
    auto duration = _duration(lua, 3, 1.0f);
    auto curve = _curve(lua, 4);
    auto direction = Vec3{1.0f, 0.0f, 0.0f};
    auto gap = 0.25f;
    auto center = Vec3{};
    if (_optionTable(lua, 5, {"direction", "gap", "center"})) {
        direction = _fieldWorld(lua, 5, "direction", direction);
        gap = _fieldNumber(lua, 5, "gap", gap);
        center = _fieldWorld(lua, 5, "center", center);
    }
    _fit(lua, box, _clipCharge(count));
    auto before = scene->clipCount();
    auto result = scene->arrange(items, count, direction, gap, center, duration, curve);
    _check(lua, scene, result, "could not arrange");
    return _charge(lua, box, before);
}

// scene:transition(outgoing, incoming, duration = 1, curve = "smooth"): fades out a handle or a
// list of handles while the incoming ones, which must stand at opacity 0, fade in.
static int _transition(lua_State* lua)
{
    detail::luaTiming(lua, 4);
    auto box = _scene(lua);
    // The handles are stored above the arguments, so absent ones must read as nil.
    lua_settop(lua, 5);
    auto duration = _duration(lua, 4, 1.0f);
    auto curve = _curve(lua, 5);
    uint32_t outgoing, incoming;
    auto from = _handles(lua, box->scene, 2, outgoing);
    auto to = _handles(lua, box->scene, 3, incoming);
    auto animations = static_cast<Animation*>(lua_newuserdatauv(lua, sizeof(Animation) * (outgoing + incoming), 0));
    for (auto i = 0u; i < outgoing; i++) new (animations + i) Animation(Animation::fade(from[i], 0.0f));
    for (auto i = 0u; i < incoming; i++) {
        if (to[i]->opacity != 0.0f) return luaL_error(lua, "incoming transition objects must have opacity 0");
        new (animations + outgoing + i) Animation(Animation::fade(to[i], 1.0f));
    }
    return _play(lua, box, animations, outgoing + incoming, duration, curve);
}

// ---------- Object styles and queries ----------

static int _stroke(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene* scene, Object* object) {
        return Animation::stroke(object, _color(lua, 3, "stroke", &scene->theme()));
    });
}

static int _fill(lua_State* lua)
{
    return _playOne(lua, 4, [lua](Scene* scene, Object* object) {
        return Animation::fill(object, _color(lua, 3, "fill", &scene->theme()));
    });
}

// scene:find(id): the handle of the object with that id, or nil.
static int _find(lua_State* lua)
{
    auto box = _sceneQuiet(lua);
    auto object = box->scene->object(luaL_checkstring(lua, 2));
    if (!object) {
        lua_pushnil(lua);
        return 1;
    }
    _knownHandle(lua, box, 1, object);
    return 1;
}

// handle:children(): the handles of the object's direct children, in the order they were added.
static int _children(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    auto object = handle->object;
    auto count = object->childCount();
    lua_getiuservalue(lua, 1, 1);
    auto sceneIndex = lua_gettop(lua);
    lua_createtable(lua, static_cast<int>(count), 0);
    for (auto i = 0u; i < count; i++) {
        _knownHandle(lua, handle->scene, sceneIndex, object->childAt(i));
        lua_rawseti(lua, -2, static_cast<lua_Integer>(i) + 1);
    }
    return 1;
}

// A whole number from 1 that counts rows or entries, or stops the script.
static uint32_t _ordinal(lua_State* lua, int index, const char* name)
{
    if (lua_type(lua, index) != LUA_TNUMBER) luaL_typeerror(lua, index, "number");
    int whole;
    auto value = lua_tointegerx(lua, index, &whole);
    if (!whole || value < 1) luaL_error(lua, "%s takes a whole number from 1", name);
    return value > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(value);
}

// The rows of a group are the groups directly below it, in order, such as the lines of a
// code block; whatever else it holds lies between them uncounted.
static Object* _row(const Object* group, uint32_t number, uint32_t& rows)
{
    rows = 0;
    Object* found = nullptr;
    for (auto i = 0u; i < group->childCount(); i++) {
        auto child = group->childAt(i);
        if (child->type() != Type::Group) continue;
        if (++rows == number) found = child;
    }
    return found;
}

// Stops the script when a group has no row of that number.
static Object* _rowAt(lua_State* lua, const Object* group, uint32_t number)
{
    uint32_t rows;
    auto row = _row(group, number, rows);
    if (!rows) luaL_error(lua, "the group has no rows; its rows are the groups directly below it");
    if (!row) {
        luaL_error(lua, "row %I is past the last of the group's %I rows", static_cast<lua_Integer>(number),
                   static_cast<lua_Integer>(rows));
    }
    return row;
}

// group:row(r): the rth group directly below the group, counting from 1, as a line of a code
// block or a row of a matrix.
static int _rowOf(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    auto row = _rowAt(lua, handle->object, _ordinal(lua, 2, "row"));
    lua_getiuservalue(lua, 1, 1);
    _knownHandle(lua, handle->scene, -1, row);
    return 1;
}

// group:entry(r, c): the cth object directly below row r of the group.
static int _entryOf(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    auto number = _ordinal(lua, 2, "entry row");
    auto column = _ordinal(lua, 3, "entry column");
    auto row = _rowAt(lua, handle->object, number);
    if (column > row->childCount()) {
        return luaL_error(lua, "entry %I is past the last of the %I objects in row %I", static_cast<lua_Integer>(column),
                          static_cast<lua_Integer>(row->childCount()), static_cast<lua_Integer>(number));
    }
    lua_getiuservalue(lua, 1, 1);
    _knownHandle(lua, handle->scene, -1, row->childAt(column - 1u));
    return 1;
}

// group:column(c): the cth object directly below each row of the group that has one, in order.
static int _columnOf(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    auto group = handle->object;
    auto column = _ordinal(lua, 2, "column");
    uint32_t rows;
    _row(group, 1u, rows);
    if (!rows) return luaL_error(lua, "the group has no rows; its rows are the groups directly below it");
    lua_getiuservalue(lua, 1, 1);
    auto sceneIndex = lua_gettop(lua);
    lua_createtable(lua, static_cast<int>(rows), 0);
    lua_Integer count = 0;
    for (auto i = 0u; i < group->childCount(); i++) {
        auto row = group->childAt(i);
        if (row->type() != Type::Group || column > row->childCount()) continue;
        _knownHandle(lua, handle->scene, sceneIndex, row->childAt(column - 1u));
        lua_rawseti(lua, -2, ++count);
    }
    if (!count) return luaL_error(lua, "no row of the group has an object %I", static_cast<lua_Integer>(column));
    return 1;
}

// text:measure(): the width and height the text takes in its font, in world units. Billboard
// text keeps its size in pixels, so it is measured at the scene's camera as authored so far.
static int _measure(lua_State* lua)
{
    auto handle = _handle(lua, 1);
    if (handle->object->type() != Type::Text) return luaL_error(lua, "measure requires a text");
    auto extent = _measured(lua, handle->scene->scene, static_cast<const Text*>(handle->object));
    lua_pushnumber(lua, extent.width);
    lua_pushnumber(lua, extent.height);
    return 2;
}

// ---------- Cameras ----------

// The projections a look table and the camera of vis.scene name, besides their CAMERA_VIEWS.
static constexpr LuaName<Projection> PROJECTIONS[] = {
    {"perspective", Projection::Perspective}, {"orthographic", Projection::Orthographic}};

// A camera move or a fade of a view costs what a clip of an object animation costs; a frame reads
// them once, not for each object, so they add nothing to the evaluation.
static constexpr LuaCharge LUA_CAMERA_CHARGE = {LUA_CLIP_CHARGE, 0, 0, 1};

// Charges a move of the scene's camera that succeeded; home and view are the camera before it.
static void _cameraMoved(LuaScene* box, const Camera& home, CameraView view)
{
    if (!box->cameraMoved) {
        box->cameraMoved = true;
        box->homeCamera = home;
        box->homeView = view;
    }
    _spend(box, LUA_CAMERA_CHARGE);
}

// The camera that look, focus and orbit move: the scene's own or the camera of one of its
// views, which take the same calls.
struct LuaCamera
{
    LuaScene* box = nullptr;
    uint32_t view = UINT32_MAX;  // the index of the view, or UINT32_MAX for the scene's camera
    Camera camera;               // the camera as the moves so far leave it
    CameraView cameraView = CameraView::TwoD;
    Camera opening;              // the camera before the first move
    CameraView openingView = CameraView::TwoD;
    float aspect = 1.0f;         // the width over the height of the picture the camera fills
};

static LuaCamera _sceneCamera(LuaScene* box)
{
    auto& config = box->scene->config();
    LuaCamera camera;
    camera.box = box;
    camera.camera = config.camera;
    camera.cameraView = config.cameraView;
    camera.opening = box->cameraMoved ? box->homeCamera : config.camera;
    camera.openingView = box->cameraMoved ? box->homeView : config.cameraView;
    camera.aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    return camera;
}

// The view at index 1 of a view method, while its scene can still be authored.
static LuaView* _viewHandle(lua_State* lua)
{
    auto view = static_cast<LuaView*>(luaL_checkudata(lua, 1, VIEW_METATABLE));
    if (!view->scene || !view->scene->scene) luaL_error(lua, "scene is no longer available");
    _authoring(lua, view->scene, "scene");
    // A view's camera may frame objects, so the tracks that wait play first, as for the scene's.
    _flushTracks(lua);
    return view;
}

// The camera of the view at index 1; it fills the view's rectangle.
static LuaCamera _viewCamera(lua_State* lua)
{
    auto view = _viewHandle(lua);
    auto scene = view->scene->scene;
    SceneView settings;
    if (!scene->viewAt(view->index, settings)) luaL_error(lua, "the view is no longer available");
    auto& config = scene->config();
    LuaCamera camera;
    camera.box = view->scene;
    camera.view = view->index;
    camera.camera = settings.camera;
    camera.cameraView = settings.cameraView;
    camera.opening = view->opening;
    camera.openingView = view->openingView;
    camera.aspect = settings.viewport.width * static_cast<float>(config.width)
                    / (settings.viewport.height * static_cast<float>(config.height));
    return camera;
}

// Moves target to camera, shown in view, and charges the move.
static void _moveCamera(lua_State* lua, const LuaCamera& target, const Camera& camera, CameraView view,
                        float duration, const AnimCurve& curve)
{
    auto box = target.box;
    _fit(lua, box, LUA_CAMERA_CHARGE);
    auto own = target.view == UINT32_MAX;
    auto result = own ? box->scene->look(camera, view, duration, curve)
                      : box->scene->look(target.view, camera, view, duration, curve);
    _check(lua, box->scene, result, "could not animate %s", own ? "camera" : "view camera");
    if (own) _cameraMoved(box, target.camera, target.cameraView);
    else _spend(box, LUA_CAMERA_CHARGE);
}

// The world bounds of the targets of what at index: an object or a list of objects.
static Bounds _framed(lua_State* lua, Scene* scene, int index, const char* what)
{
    index = lua_absindex(lua, index);
    Bounds bounds;
    auto defined = false;
    if (lua_istable(lua, index)) {
        auto count = lua_rawlen(lua, index);
        for (auto i = 1u; i <= count; i++) {
            lua_rawgeti(lua, index, static_cast<lua_Integer>(i));
            _surround(_object(lua, scene, -1), nullptr, bounds, defined);
            lua_pop(lua, 1);
        }
    } else {
        _surround(_object(lua, scene, index), nullptr, bounds, defined);
    }
    // An empty list has no bounds either.
    if (!defined) luaL_error(lua, "%s targets have no bounds", what);
    return bounds;
}

// A look target given as an object or a list of objects, the forms focus takes, aims at the
// center of the bounds focus frames; a point leaves center as it is and returns false.
static bool _lookCenter(lua_State* lua, LuaScene* box, int index, Vec3& center)
{
    lua_getfield(lua, index, "target");
    // The target, or the first item of a list, tells objects from a point.
    if (lua_istable(lua, -1)) lua_rawgeti(lua, -1, 1);
    else lua_pushvalue(lua, -1);
    auto objects = lua_isuserdata(lua, -1);
    lua_pop(lua, 1);
    if (objects) center = _framed(lua, box->scene, -1, "look").center();
    lua_pop(lua, 1);
    return objects;
}

// Reads the look table at index into camera and view, which hold the camera it starts from.
static void _lookFields(lua_State* lua, LuaScene* box, int index, Camera& camera, CameraView& view)
{
    index = lua_absindex(lua, index);
    luaL_checktype(lua, index, LUA_TTABLE);
    _options(lua, index, {"view", "eye", "target", "up", "height", "projection", "fov", "near", "far"}, false);
    Vec3 center;
    auto centered = _lookCenter(lua, box, index, center);
    auto target = centered ? center : _fieldWorld(lua, index, "target", camera.target);
    // A 2D pan keeps the viewing direction: the eye follows the target unless given. A 2D eye
    // stands straight above its target, so it takes the target's x and y as they are, which
    // adding the shift does not always round back to. Centering on objects moves a 3D eye with
    // the target too, so the camera keeps its direction and distance.
    auto eye = camera.eye;
    if (view == CameraView::TwoD) eye = {target.x, target.y, eye.z + (target.z - camera.target.z)};
    else if (centered) eye = eye + (target - camera.target);
    camera.eye = _fieldWorld(lua, index, "eye", eye);
    camera.target = target;
    camera.up = _fieldWorld(lua, index, "up", camera.up);
    camera.orthoHeight = _fieldNumber(lua, index, "height", camera.orthoHeight);
    camera.fov = _fieldNumber(lua, index, "fov", camera.fov);
    camera.near = _fieldNumber(lua, index, "near", camera.near);
    camera.far = _fieldNumber(lua, index, "far", camera.far);
    view = _fieldEnum(lua, index, "view", view, CAMERA_VIEWS, "camera view");
    camera.projection = _fieldEnum(lua, index, "projection", camera.projection, PROJECTIONS);
}

// look(camera, duration = 1, curve = "smooth") on the scene or on one of its views.
static int _lookCamera(lua_State* lua, const LuaCamera& target)
{
    detail::luaTiming(lua, 3);
    auto camera = target.camera;
    auto view = target.cameraView;
    _lookFields(lua, target.box, 2, camera, view);
    auto duration = _duration(lua, 3, 1.0f);
    _moveCamera(lua, target, camera, view, duration, _curve(lua, 4));
    return 0;
}

static int _look(lua_State* lua)
{
    return _lookCamera(lua, _sceneCamera(_scene(lua)));
}

// orbit(radians, {axis = camera up, duration = 1, curve = "smooth"}) on the scene or a view.
static int _orbitCamera(lua_State* lua, const LuaCamera& target)
{
    auto box = target.box;
    lua_settop(lua, 3);
    auto angle = _number(lua, 2, "angle");
    auto options = _motionOptions(lua, 3, {"axis", "duration", "curve"});
    auto axis = target.camera.up;
    if (!lua_isnil(lua, 3)) axis = _fieldWorld(lua, 3, "axis", axis);
    _fit(lua, box, LUA_CAMERA_CHARGE);
    auto own = target.view == UINT32_MAX;
    auto result = own ? box->scene->orbit(angle, axis, options.duration, options.curve)
                      : box->scene->orbit(target.view, angle, axis, options.duration, options.curve);
    _check(lua, box->scene, result, "could not orbit %s", own ? "camera" : "view camera");
    if (own) _cameraMoved(box, target.camera, target.cameraView);
    else _spend(box, LUA_CAMERA_CHARGE);
    return 0;
}

static int _orbit(lua_State* lua)
{
    return _orbitCamera(lua, _sceneCamera(_scene(lua)));
}

// ---------- Waiting, removing and cloning ----------

static int _wait(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    _check(lua, scene, scene->wait(_duration(lua, 2, 1.0f, true)), "could not wait");
    return 0;
}

static int _remove(lua_State* lua)
{
    auto scene = _scene(lua)->scene;
    _check(lua, scene, scene->remove(_object(lua, scene, 2)), "could not remove object");
    return 0;
}

// What scene:clone() applies to the copy before it is attached, and what the copy costs.
struct LuaClone
{
    lua_State* lua = nullptr;
    LuaScene* scene = nullptr;
    // The options on the stack of scene:clone(), or zero.
    int options = 0;
    LuaCharge charge;
    // Set when the overrides or the limits stopped the clone; the message is on the stack.
    bool failed = false;
};

// Run protected with the clone, the copy and the options: applies the overrides of the
// options to the copy and checks that the copy fits the scene's limits.
static int _cloneEdit(lua_State* lua)
{
    auto context = static_cast<LuaClone*>(lua_touserdata(lua, 1));
    auto copy = static_cast<Object*>(lua_touserdata(lua, 2));
    auto box = context->scene;
    if (lua_istable(lua, 3)) {
        _looks(lua, box->scene, copy, 3);
        lua_getfield(lua, 3, "shift");
        if (!lua_isnil(lua, -1)) {
            if (copy->type() == Type::Connector) luaL_error(lua, "a connector follows its endpoints and takes no shift");
            copy->shift(_world(lua, -1, "shift"));
        }
        lua_pop(lua, 1);
    }
    if (!_treeCharge(copy, context->charge)) return luaL_error(lua, "the copy is too deep or large");
    _fit(lua, box, context->charge);
    return 0;
}

static Result _cloneEdited(Object* copy, void* data) noexcept
{
    auto context = static_cast<LuaClone*>(data);
    auto lua = context->lua;
    lua_pushcfunction(lua, _cloneEdit);
    lua_pushlightuserdata(lua, context);
    lua_pushlightuserdata(lua, copy);
    if (context->options) lua_pushvalue(lua, context->options);
    else lua_pushnil(lua);
    if (lua_pcall(lua, 3, 0, 0) == LUA_OK) return Result::Success;
    context->failed = true;
    return Result::InvalidArguments;
}

// Records in the table at map the id of the copy of every object of the tree of source.
static void _cloneMap(lua_State* lua, int map, Object* source, const Object* copy)
{
    lua_pushlightuserdata(lua, source);
    lua_pushinteger(lua, copy->id());
    lua_rawset(lua, map);
    // A copy attached below source adds a child that has no copy of its own.
    for (auto i = 0u; i < source->childCount() && i < copy->childCount(); i++)
        _cloneMap(lua, map, source->childAt(i), copy->childAt(i));
}

// Gives each copy of a plot in the table at map, which maps originals to the ids of their copies,
// the cut its original keeps, so that the states of the copy cut fn and points as the original's
// do. The copy has room for only the points and pieces it shows (see Scene::clone()).
static void _plotCopies(lua_State* lua, int map, const LuaScene* box)
{
    _sceneTable(lua, PLOT_REGISTRY, 1, false);
    if (!lua_istable(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    auto records = lua_gettop(lua);
    lua_pushnil(lua);
    while (lua_next(lua, map)) {
        auto copy = lua_isinteger(lua, -1) ? box->scene->object(static_cast<uint32_t>(lua_tointeger(lua, -1))) : nullptr;
        lua_pop(lua, 1);
        if (!copy || copy->type() != Type::Plot) continue;
        lua_pushvalue(lua, -1);
        lua_rawget(lua, records);
        if (auto kept = static_cast<const LuaPlotRecord*>(lua_touserdata(lua, -1))) {
            auto plot = static_cast<const Plot*>(copy);
            auto record = new (lua_newuserdatauv(lua, sizeof(LuaPlotRecord) + sizeof(double) * kept->breakCount, 0))
                LuaPlotRecord(*kept);
            if (kept->breakCount) std::memcpy(static_cast<void*>(record + 1), kept + 1, sizeof(double) * kept->breakCount);
            record->room = plot->count();
            record->endRoom = plot->contours() > 1u ? plot->contours() : 0u;
            record->sampleRoom = plot->samples().count;
            record->copied = false;
            lua_pushlightuserdata(lua, copy);
            lua_insert(lua, -2);
            lua_rawset(lua, records);
        }
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
}

// copies[handle]: the handle of the copy of the object of handle, or nil for an object that
// was not copied.
static int _cloneCopy(lua_State* lua)
{
    auto handle = _testHandle(lua, 2);
    auto box = static_cast<LuaScene*>(lua_touserdata(lua, lua_upvalueindex(1)));
    if (!handle || !handle->object || !box || !box->scene) {
        lua_pushnil(lua);
        return 1;
    }
    lua_pushlightuserdata(lua, handle->object);
    lua_rawget(lua, 1);
    auto object = lua_isinteger(lua, -1) ? box->scene->object(static_cast<uint32_t>(lua_tointeger(lua, -1))) : nullptr;
    if (!object) {
        lua_pushnil(lua);
        return 1;
    }
    _knownHandle(lua, box, lua_upvalueindex(1), object);
    return 1;
}

// scene:clone(source[, options]): copies source and what is below it in the state they show
// at the cursor (see Scene::clone()), and returns the copy's handle and a table that gives
// the copy of every object below source, source too, by its handle.
static int _clone(lua_State* lua)
{
    auto box = _scene(lua);
    auto scene = box->scene;
    auto source = _object(lua, scene, 2);
    CloneOptions options;
    LuaClone context;
    context.lua = lua;
    context.scene = box;
    if (_optionTable(lua, 3, {"parent", "shift"}, true)) {
        // The id stays on the stack until the scene copies it.
        options.tag = _fieldId(lua, scene, 3);
        lua_getfield(lua, 3, "parent");
        if (!lua_isnil(lua, -1)) options.parent = _object(lua, scene, -1);
        lua_pop(lua, 1);
        context.options = 3;
    }
    options.edit = _cloneEdited;
    options.data = &context;
    Object* copy = nullptr;
    auto result = scene->clone(source, copy, options);
    if (context.failed) return _raise(lua);
    _check(lua, scene, result, "could not clone %s", type(source->type()));
    _spend(box, context.charge);
    _evaluate(lua, box);
    if (context.options) _mask(lua, scene, copy, 3);
    _knownHandle(lua, box, 1, copy);
    lua_createtable(lua, 0, 0);
    auto map = lua_gettop(lua);
    _cloneMap(lua, map, source, copy);
    _plotCopies(lua, map, box);
    lua_createtable(lua, 0, 1);
    lua_pushvalue(lua, 1);
    lua_pushcclosure(lua, _cloneCopy, 1);
    lua_setfield(lua, -2, "__index");
    lua_setmetatable(lua, map);
    return 2;
}

// ---------- Timeline control and viewports ----------

// Time arguments are seconds from the scene start or the name of a marker.
static float _time(lua_State* lua, const Scene* scene, int index)
{
    if (lua_type(lua, index) == LUA_TSTRING) {
        size_t size;
        auto name = lua_tolstring(lua, index, &size);
        auto time = 0.0f;
        if (std::strlen(name) != size || !scene->marker(name, time)) {
            luaL_error(lua, "unknown marker '%s'", name);
        }
        return time;
    }
    auto time = _number(lua, index, "time");
    if (time < 0.0f) luaL_error(lua, "time cannot be negative");
    return time;
}

static void _seek(lua_State* lua, Scene* scene, float time)
{
    _check(lua, scene, scene->seek(time), "could not move the timeline");
}

// scene:time() is the authoring time; scene:time(marker) is the time of a marker.
static int _sceneTime(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    lua_pushnumber(lua, lua_isnoneornil(lua, 2) ? scene->cursor() : _time(lua, scene, 2));
    return 1;
}

// scene:seek(time_or_marker) moves the authoring time; prefer at/parallel, which return.
static int _sceneSeek(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    _seek(lua, scene, _time(lua, scene, 2));
    return 0;
}

// scene:duration() is how far the timeline reaches: to its last clip, the cursor or the end of a
// viewport, whichever is latest.
static int _sceneDuration(lua_State* lua)
{
    lua_pushnumber(lua, _sceneQuiet(lua)->scene->duration());
    return 1;
}

// scene:at(time_or_marker, fn) authors fn from that time, then returns to the current
// time, so a block can overlay an earlier or later beat without moving the main track.
static int _at(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    auto time = _time(lua, scene, 2);
    luaL_checktype(lua, 3, LUA_TFUNCTION);
    auto resume = scene->cursor();
    _seek(lua, scene, time);
    lua_pushvalue(lua, 3);
    lua_pushvalue(lua, 1);
    lua_call(lua, 1, 0);
    _seek(lua, scene, resume);
    return 0;
}

static void _track(lua_State* lua, bool list, lua_Unsigned index)
{
    if (list) lua_rawgeti(lua, 2, static_cast<lua_Integer>(index));
    else lua_pushvalue(lua, static_cast<int>(index) + 1);
}

// scene:parallel(fn, ...) or scene:parallel({fn, ...}) starts every track at the
// current time and continues after the longest one.
static int _parallel(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    auto list = lua_gettop(lua) == 2 && lua_type(lua, 2) == LUA_TTABLE;
    auto count = list ? lua_rawlen(lua, 2) : static_cast<lua_Unsigned>(lua_gettop(lua) - 1);
    if (!count) return luaL_error(lua, "parallel requires at least one function");
    for (lua_Unsigned i = 1u; i <= count; i++) {
        _track(lua, list, i);
        if (lua_type(lua, -1) != LUA_TFUNCTION) {
            return luaL_error(lua, "parallel track %d must be a function", static_cast<int>(i));
        }
        lua_pop(lua, 1);
    }
    auto start = scene->cursor();
    auto end = start;
    for (lua_Unsigned i = 1u; i <= count; i++) {
        _seek(lua, scene, start);
        _track(lua, list, i);
        lua_pushvalue(lua, 1);
        lua_call(lua, 1, 0);
        if (scene->cursor() > end) end = scene->cursor();
    }
    _seek(lua, scene, end);
    return 0;
}

static int _mark(lua_State* lua)
{
    auto scene = _sceneQuiet(lua)->scene;
    size_t size;
    auto name = luaL_checklstring(lua, 2, &size);
    if (std::strlen(name) != size) return luaL_error(lua, "marker name cannot contain zero bytes");
    _check(lua, scene, scene->mark(name), "could not add marker '%s'", name);
    return 0;
}

static Viewport _viewportValue(lua_State* lua, int index)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    _options(lua, index, {"x", "y", "width", "height"}, false);
    Viewport viewport;
    viewport.x = _fieldNumber(lua, index, "x", viewport.x);
    viewport.y = _fieldNumber(lua, index, "y", viewport.y);
    viewport.width = _fieldNumber(lua, index, "width", viewport.width);
    viewport.height = _fieldNumber(lua, index, "height", viewport.height);
    return viewport;
}

static int _viewport(lua_State* lua)
{
    auto parent = _scene(lua);
    auto child = static_cast<LuaScene*>(luaL_checkudata(lua, 2, SCENE_METATABLE));
    if (!child->scene) return luaL_error(lua, "viewport scene is no longer available");
    if (!child->owner) return luaL_error(lua, "viewport scene already has an owner");
    if (child == parent) return luaL_error(lua, "scene cannot contain itself as a viewport");
    auto viewport = _viewportValue(lua, 3);
    if (child->scenes > LUA_SCENE_LIMIT - parent->scenes) {
        return luaL_error(lua, "scene viewport limit exceeded");
    }
    auto memory = parent->memory;
    if (child->nativeUsed > LUA_NATIVE_MEMORY_LIMIT - LUA_VIEWPORT_CHARGE
        || child->nativeUsed + LUA_VIEWPORT_CHARGE > LUA_NATIVE_MEMORY_LIMIT - parent->nativeUsed
        || memory->nativeUsed > LUA_NATIVE_MEMORY_LIMIT - LUA_VIEWPORT_CHARGE) {
        return luaL_error(lua, "scene native memory limit exceeded");
    }
    if (child->points > LUA_POINT_LIMIT - parent->points) return luaL_error(lua, "scene point limit exceeded");
    if (child->clips > LUA_CLIP_LIMIT - parent->clips) return luaL_error(lua, "scene animation limit exceeded");
    if (child->objects > LUA_OBJECT_LIMIT - parent->objects) return luaL_error(lua, "scene object limit exceeded");
    auto result = parent->scene->viewport(child->scene, viewport);
    _check(lua, parent->scene, result, "could not add viewport");
    parent->nativeUsed += child->nativeUsed + LUA_VIEWPORT_CHARGE;
    parent->objects += child->objects;
    parent->points += child->points;
    parent->clips += child->clips;
    parent->scenes += child->scenes;
    memory->nativeUsed += LUA_VIEWPORT_CHARGE;
    child->owner = false;
    child->scene = nullptr;
    child->memory = nullptr;
    return _evaluate(lua, parent);
}

// ---------- Scene transitions ----------

static TransitionKind _transitionKind(lua_State* lua, int index)
{
    static constexpr LuaName<TransitionKind> KINDS[] = {
        {"match", TransitionKind::Match}, {"fade", TransitionKind::Fade}, {"slide", TransitionKind::Slide},
        {"zoom_in", TransitionKind::ZoomIn}, {"zoom_out", TransitionKind::ZoomOut}};
    if (lua_type(lua, index) != LUA_TSTRING) luaL_error(lua, "a transition style must be a name such as 'fade'");
    size_t size;
    auto name = lua_tolstring(lua, index, &size);
    if (auto kind = _findName(name, size, KINDS)) return kind->value;
    return static_cast<TransitionKind>(luaL_error(
        lua, "unknown transition style '%s'; use 'match', 'fade', 'slide', 'zoom_in' or 'zoom_out'", name));
}

static TransitionSide _transitionSide(lua_State* lua, int index)
{
    static constexpr LuaName<TransitionSide> SIDES[] = {
        {"left", TransitionSide::Left}, {"right", TransitionSide::Right}, {"bottom", TransitionSide::Bottom},
        {"top", TransitionSide::Top}};
    if (lua_type(lua, index) != LUA_TSTRING) luaL_error(lua, "from must be a side such as 'left'");
    size_t size;
    auto name = lua_tolstring(lua, index, &size);
    if (auto side = _findName(name, size, SIDES)) return side->value;
    return static_cast<TransitionSide>(luaL_error(
        lua, "unknown slide direction '%s'; use 'left', 'right', 'top' or 'bottom'", name));
}

// The style of a transition at index: a style name, or a table whose style field names it
// beside the options of that style. An area given as an object id comes back in id for the
// caller, which knows the scenes to look in.
static TransitionStyle _transitionStyle(lua_State* lua, int index, const char*& id)
{
    TransitionStyle style;
    id = nullptr;
    index = lua_absindex(lua, index);
    auto named = lua_type(lua, index) == LUA_TSTRING;
    if (named) {
        style.kind = _transitionKind(lua, index);
    } else {
        lua_getfield(lua, index, "style");
        if (!lua_isnil(lua, -1)) style.kind = _transitionKind(lua, -1);
        lua_pop(lua, 1);
        lua_getfield(lua, index, "from");
        if (!lua_isnil(lua, -1)) {
            if (style.kind != TransitionKind::Slide) luaL_error(lua, "from applies only to the slide style");
            style.from = _transitionSide(lua, -1);
        }
        lua_pop(lua, 1);
    }
    auto zoom = style.kind == TransitionKind::ZoomIn || style.kind == TransitionKind::ZoomOut;
    if (!named) {
        lua_getfield(lua, index, "match");
        if (!lua_isnil(lua, -1) && !lua_isboolean(lua, -1)) luaL_error(lua, "match must be true or false");
        style.match = lua_toboolean(lua, -1);
        lua_pop(lua, 1);
        if (style.match && !zoom && style.kind != TransitionKind::Slide) {
            luaL_error(lua, "match applies only to the slide and zoom styles");
        }
    }
    if (named) lua_pushnil(lua);
    else lua_getfield(lua, index, "area");
    auto area = lua_absindex(lua, -1);
    if (lua_isnil(lua, area)) {
        if (zoom) luaL_error(lua, "a zoom needs an area: an object, its id or {x, y, width, height}");
    } else if (!zoom) {
        luaL_error(lua, "area applies only to the zoom styles");
    } else if (lua_type(lua, area) == LUA_TSTRING) {
        id = lua_tostring(lua, area);
    } else if (lua_istable(lua, area)) {
        _options(lua, area, {"x", "y", "width", "height"}, false);
        style.area = {_fieldNumber(lua, area, "x", 0.0f), _fieldNumber(lua, area, "y", 0.0f),
                      _fieldNumber(lua, area, "width", 0.0f), _fieldNumber(lua, area, "height", 0.0f)};
    } else if (lua_type(lua, area) == LUA_TUSERDATA) {
        style.object = _handle(lua, area)->object;
    } else {
        luaL_error(lua, "area must be an object, its id or {x, y, width, height}");
    }
    lua_pop(lua, 1);
    if (!named) {
        lua_getfield(lua, index, "area_margin");
        if (!lua_isnil(lua, -1)) {
            if (!zoom || (!style.object && !id)) {
                luaL_error(lua, "area_margin applies only to an object zoom area");
            }
            style.areaMargin = _number(lua, -1, "area_margin");
            if (style.areaMargin < 0.0f) luaL_error(lua, "area_margin must be non-negative");
        }
        lua_pop(lua, 1);
    }
    return style;
}

static int _sceneTransition(lua_State* lua)
{
    auto parent = _scene(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto size = lua_rawlen(lua, 2);
    if (size < 2u || size > LUA_VIEWPORT_LIMIT) {
        return luaL_error(lua, "scene transition requires 2 to %d stages",
                          static_cast<int>(LUA_VIEWPORT_LIMIT));
    }
    // Options left out or given as nil read as an empty table, which takes their place.
    lua_settop(lua, 3);
    if (lua_isnil(lua, 3)) {
        lua_newtable(lua);
        lua_replace(lua, 3);
    } else {
        luaL_checktype(lua, 3, LUA_TTABLE);
    }
    auto options = 3;
    _options(lua, options,
             {"duration", "hold", "curve", "viewport", "style", "from", "area", "area_margin", "match",
              "styles", "live"}, false);
    auto duration = _fieldDuration(lua, options, "duration", 1.0f);
    float holds[LUA_VIEWPORT_LIMIT] = {};
    auto holdCount = 1u;
    lua_getfield(lua, options, "hold");
    if (lua_istable(lua, -1)) {
        if (lua_rawlen(lua, -1) != size) {
            return luaL_error(lua, "hold must list one duration for each of the %d stages",
                              static_cast<int>(size));
        }
        holdCount = static_cast<uint32_t>(size);
        for (auto i = 0u; i < holdCount; i++) {
            lua_rawgeti(lua, -1, i + 1u);
            holds[i] = _number(lua, -1, "hold");
            lua_pop(lua, 1);
        }
    } else if (!lua_isnil(lua, -1)) {
        holds[0] = _number(lua, -1, "hold");
    }
    lua_pop(lua, 1);
    for (auto i = 0u; i < holdCount; i++) {
        if (holds[i] < 0.0f) return luaL_error(lua, "hold must be non-negative");
    }
    auto live = _fieldBool(lua, options, "live", true);
    auto curve = _fieldCurve(lua, options);
    Viewport viewport;
    lua_getfield(lua, options, "viewport");
    if (!lua_isnil(lua, -1)) viewport = _viewportValue(lua, -1);
    lua_pop(lua, 1);
    // One style for every transition, or styles with one for each.
    TransitionStyle styles[LUA_VIEWPORT_LIMIT - 1u];
    const char* ids[LUA_VIEWPORT_LIMIT - 1u] = {};
    auto styleCount = 1u;
    lua_getfield(lua, options, "styles");
    if (lua_isnil(lua, -1)) {
        styles[0] = _transitionStyle(lua, options, ids[0]);
    } else {
        for (auto key : {"style", "from", "area", "area_margin", "match"}) {
            lua_getfield(lua, options, key);
            if (!lua_isnil(lua, -1)) return luaL_error(lua, "use either %s or styles, not both", key);
            lua_pop(lua, 1);
        }
        styleCount = static_cast<uint32_t>(size - 1u);
        if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != styleCount) {
            return luaL_error(lua, "styles must list one style for each of the %d transitions",
                              static_cast<int>(styleCount));
        }
        for (auto i = 0u; i < styleCount; i++) {
            lua_rawgeti(lua, -1, i + 1u);
            if (lua_istable(lua, -1)) {
                _options(lua, -1, {"style", "from", "area", "area_margin", "match"}, false);
            } else if (lua_type(lua, -1) != LUA_TSTRING) {
                return luaL_error(lua, "each entry of styles must be a style name or a table with a style");
            }
            styles[i] = _transitionStyle(lua, -1, ids[i]);
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);

    auto scenes = static_cast<Scene**>(lua_newuserdatauv(lua, sizeof(Scene*) * size, 0));
    auto boxes = static_cast<LuaScene**>(lua_newuserdatauv(lua, sizeof(LuaScene*) * size, 0));
    auto totalNative = LUA_TRANSITION_CHARGE * (size + 1u);
    auto totalObjects = 0u;
    auto totalPoints = 0u;
    auto totalClips = 0u;
    auto totalScenes = 0u;
    for (auto i = 0u; i < size; i++) {
        lua_rawgeti(lua, 2, i + 1u);
        auto child = static_cast<LuaScene*>(luaL_testudata(lua, -1, SCENE_METATABLE));
        if (!child) return luaL_error(lua, "scene transition stages must be scenes, got %s", luaL_typename(lua, -1));
        lua_pop(lua, 1);
        if (!child->scene) return luaL_error(lua, "scene transition stage is no longer available");
        if (!child->owner) return luaL_error(lua, "scene transition stage already has an owner");
        if (child == parent) return luaL_error(lua, "scene cannot transition to itself");
        for (auto j = 0u; j < i; j++) {
            if (boxes[j] == child) return luaL_error(lua, "scene transition stages must be unique");
        }
        if (i + 1u < size) {
            auto matchBytes = static_cast<size_t>(child->objects) * sizeof(uint32_t);
            if (matchBytes > LUA_NATIVE_MEMORY_LIMIT - totalNative) {
                return luaL_error(lua, "scene native memory limit exceeded");
            }
            totalNative += matchBytes;
        }
        if (child->nativeUsed > LUA_NATIVE_MEMORY_LIMIT - totalNative) {
            return luaL_error(lua, "scene native memory limit exceeded");
        }
        totalNative += child->nativeUsed;
        if (child->objects > LUA_OBJECT_LIMIT - totalObjects) return luaL_error(lua, "scene object limit exceeded");
        if (child->points > LUA_POINT_LIMIT - totalPoints) return luaL_error(lua, "scene point limit exceeded");
        if (child->clips > LUA_CLIP_LIMIT - totalClips) return luaL_error(lua, "scene animation limit exceeded");
        if (child->scenes > LUA_SCENE_LIMIT - totalScenes) return luaL_error(lua, "scene transition limit exceeded");
        totalObjects += child->objects;
        totalPoints += child->points;
        totalClips += child->clips;
        totalScenes += child->scenes;
        scenes[i] = child->scene;
        boxes[i] = child;
    }
    if (totalScenes > LUA_SCENE_LIMIT - parent->scenes) return luaL_error(lua, "scene transition limit exceeded");
    if (totalObjects > LUA_OBJECT_LIMIT - parent->objects) return luaL_error(lua, "scene object limit exceeded");
    if (totalPoints > LUA_POINT_LIMIT - parent->points) return luaL_error(lua, "scene point limit exceeded");
    if (totalClips > LUA_CLIP_LIMIT - parent->clips) return luaL_error(lua, "scene animation limit exceeded");
    if (totalNative > LUA_NATIVE_MEMORY_LIMIT - parent->nativeUsed) {
        return luaL_error(lua, "scene native memory limit exceeded");
    }
    auto transitionNative = LUA_TRANSITION_CHARGE * (size + 1u);
    for (auto i = 0u; i + 1u < size; i++) {
        transitionNative += static_cast<size_t>(boxes[i]->objects) * sizeof(uint32_t);
    }
    auto memory = parent->memory;
    if (memory->nativeUsed > LUA_NATIVE_MEMORY_LIMIT - transitionNative) {
        return luaL_error(lua, "scene native memory limit exceeded");
    }
    // An area named by id is the object with that id in the scene the area is a part of: the
    // outgoing scene zooming in, as the incoming one grows out of it, and the incoming scene
    // zooming out, as the outgoing one shrinks into it. Failing that, it is the object in the
    // other scene, and a style for every transition looks in each pair.
    if (styleCount == 1u && ids[0] && size > 2u) {
        styleCount = static_cast<uint32_t>(size - 1u);
        for (auto i = 1u; i < styleCount; i++) {
            styles[i] = styles[0];
            ids[i] = ids[0];
        }
    }
    for (auto i = 0u; i < styleCount; i++) {
        if (!ids[i]) continue;
        auto in = styles[i].kind == TransitionKind::ZoomIn;
        styles[i].object = scenes[in ? i : i + 1u]->object(ids[i]);
        if (!styles[i].object) styles[i].object = scenes[in ? i + 1u : i]->object(ids[i]);
        if (!styles[i].object) {
            return luaL_error(lua, "zoom area '%s' is the id of no object in either scene of its transition",
                              ids[i]);
        }
    }
    auto result = parent->scene->transition(scenes, static_cast<uint32_t>(size), viewport,
                                            duration, holds, holdCount, curve, styles, styleCount, live);
    _check(lua, parent->scene, result, "could not add scene transition");
    parent->nativeUsed += totalNative;
    parent->objects += totalObjects;
    parent->points += totalPoints;
    parent->clips += totalClips;
    parent->scenes += totalScenes;
    memory->nativeUsed += transitionNative;
    for (auto i = 0u; i < size; i++) {
        boxes[i]->owner = false;
        boxes[i]->scene = nullptr;
        boxes[i]->memory = nullptr;
    }
    return _evaluate(lua, parent);
}

// ---------- Trackers ----------

// Tracker helpers, reachable only from the tracker prelude in visLuaTracker.h, which
// luaOpenApi hands them to as arguments.
static int _trackerEase(lua_State* lua)
{
    auto curve = _curve(lua, 1);
    auto progress = _number(lua, 2, "progress");
    if (curve.kind == AnimCurvePreset::Linear || curve.strength == 0.0f) {
        auto precise = lua_tonumber(lua, 2);
        lua_pushnumber(lua, precise < 0.0 ? 0.0 : (precise > 1.0 ? 1.0 : precise));
    } else lua_pushnumber(lua, ease(curve, progress));
    return 1;
}

static int _trackerFps(lua_State* lua)
{
    lua_pushinteger(lua, _sceneQuiet(lua)->scene->config().fps);
    return 1;
}

// tracker:where(level): the place of the call at level, as error messages begin with it.
static int _trackerWhere(lua_State* lua)
{
    luaL_where(lua, static_cast<int>(luaL_checkinteger(lua, 1)));
    return 1;
}

// Calls the bound function at 2 with the values after it, as a sampling callback of the scene at
// 1, and returns the state it gives.
static int _trackerSample(lua_State* lua)
{
    auto box = static_cast<LuaScene*>(luaL_checkudata(lua, 1, SCENE_METATABLE));
    if (_sampleCall(lua, box, lua_gettop(lua) - 2, 1) != LUA_OK) return lua_error(lua);
    return 1;
}

// Tells the scene calls that tracks wait to play, so the next one flushes them first.
static int _trackerPending(lua_State* lua)
{
    void* data = nullptr;
    lua_getallocf(lua, &data);
    if (data) static_cast<LuaMemory*>(data)->tracksPending = true;
    return 0;
}

// Applies a binding's first state like scene:set: as the starting state before the timeline
// starts, and at the cursor afterwards.
static int _trackerApply(lua_State* lua)
{
    auto box = _sceneQuiet(lua);
    Object* object;
    lua_newtable(lua);
    auto anchor = lua_gettop(lua);
    size_t strings = 0;
    uint32_t grown = 0;
    // Validates the descriptor exactly like a composite play entry.
    auto target = _animationTarget(lua, box, 2, object, anchor, &strings, &grown);
    _fit(lua, box, {strings});
    _fit(lua, box, {0, 0, grown});
    auto before = box->scene->clipCount();
    _check(lua, box->scene, box->scene->update(target), "could not apply the bound state");
    return _charge(lua, box, before, strings, grown);
}

// Returns the first key a bound state may not set, or nothing.
static int _trackerUnknown(lua_State* lua)
{
    luaL_checktype(lua, 1, LUA_TTABLE);
    return _unknownKey(lua, 1, {}) ? 1 : 0;
}

// Called from scene:bind; reports failures at the author's bind call (stack level 2).
static int _trackerSettle(lua_State* lua)
{
    lua_pushcfunction(lua, _trackerApply);
    lua_insert(lua, 1);
    if (lua_pcall(lua, lua_gettop(lua) - 1, 0, 0) == LUA_OK) return 0;
    luaL_where(lua, 2);
    lua_insert(lua, -2);
    lua_concat(lua, 2);
    return lua_error(lua);
}

// vis.spring_response(seconds, {mass, stiffness, damping, displacement, velocity}) is the
// displacement of a damped spring seconds after it starts, for a tracker to follow.
static int _springResponse(lua_State* lua)
{
    if (lua_type(lua, 1) != LUA_TNUMBER || !std::isfinite(lua_tonumber(lua, 1)) || lua_tonumber(lua, 1) < 0.0) {
        return luaL_error(lua, "seconds must be a finite non-negative number");
    }
    auto seconds = static_cast<double>(lua_tonumber(lua, 1));
    luaL_checktype(lua, 2, LUA_TTABLE);
    _options(lua, 2, {"mass", "stiffness", "damping", "displacement", "velocity"}, false);
    const char* fields[] = {"mass", "stiffness", "damping", "displacement", "velocity"};
    double values[5];
    for (auto i = 0u; i < 5u; i++) {
        lua_getfield(lua, 2, fields[i]);
        if (i == 4u && lua_isnil(lua, -1)) values[i] = 0.0;
        else {
            if (lua_type(lua, -1) != LUA_TNUMBER || !std::isfinite(lua_tonumber(lua, -1))) {
                return luaL_error(lua, "%s must be a finite number", fields[i]);
            }
            values[i] = static_cast<double>(lua_tonumber(lua, -1));
        }
        lua_pop(lua, 1);
    }
    if (values[0] <= 0.0 || values[1] <= 0.0 || values[2] < 0.0) {
        return luaL_error(lua, "mass and stiffness must be positive and damping non-negative");
    }
    auto value = springResponse(seconds, values[0], values[1], values[2], values[3], values[4]);
    if (!std::isfinite(value)) return luaL_error(lua, "spring response is outside the finite numeric range");
    lua_pushnumber(lua, value);
    return 1;
}

// ---------- Focus and views ----------

// Pushes a look table that gives every field of camera, shown in view.
static void _pushCamera(lua_State* lua, const Camera& camera, CameraView view)
{
    auto point = [&](const char* name, const Vec3& value) {
        lua_createtable(lua, 3, 0);
        lua_pushnumber(lua, value.x);
        lua_rawseti(lua, -2, 1);
        lua_pushnumber(lua, value.y);
        lua_rawseti(lua, -2, 2);
        lua_pushnumber(lua, value.z);
        lua_rawseti(lua, -2, 3);
        lua_setfield(lua, -2, name);
    };
    auto number = [&](const char* name, float value) {
        lua_pushnumber(lua, value);
        lua_setfield(lua, -2, name);
    };
    lua_createtable(lua, 0, 9);
    lua_pushstring(lua, view == CameraView::TwoD ? "2d" : "3d");
    lua_setfield(lua, -2, "view");
    point("eye", camera.eye);
    point("target", camera.target);
    point("up", camera.up);
    lua_pushstring(lua, camera.projection == Projection::Perspective ? "perspective" : "orthographic");
    lua_setfield(lua, -2, "projection");
    number("height", camera.orthoHeight);
    number("fov", camera.fov);
    number("near", camera.near);
    number("far", camera.far);
}

// focus(target_or_array, {padding, duration, curve}) on the scene or on one of its views frames
// the targets' world bounds with a 2D pan and zoom in the picture its camera fills, so authors
// never compute camera numbers.
static int _focusCamera(lua_State* lua, const LuaCamera& target)
{
    // focus() or focus(nil, options) returns to the opening camera, the one before the first
    // camera move, from either view.
    auto home = lua_isnoneornil(lua, 2);
    if (!home && target.cameraView != CameraView::TwoD) {
        return luaL_error(lua, "focus requires a 2D camera; use look in 3D");
    }
    auto& opening = target.opening;
    auto openingView = target.openingView;
    auto bounds = home ? Bounds{opening.target, opening.target} : _framed(lua, target.box->scene, 2, "focus");
    auto padding = 0.15f, duration = 1.0f;
    lua_settop(lua, 3);
    lua_pushnil(lua);
    if (_optionTable(lua, 3, {"padding", "duration", "curve"})) {
        padding = _fieldNumber(lua, 3, "padding", padding);
        duration = _fieldNumber(lua, 3, "duration", duration);
        if (padding < 0.0f) return luaL_error(lua, "padding must be a non-negative number");
        lua_getfield(lua, 3, "curve");
        lua_replace(lua, 4);
    }
    if (home && (target.cameraView != CameraView::TwoD || openingView != CameraView::TwoD)) {
        // Outside the 2D view the whole opening camera comes back, its view and lens included.
        _pushCamera(lua, opening, openingView);
    } else {
        auto size = bounds.max - bounds.min;
        auto aspect = target.aspect;
        auto height = std::fmax(std::fmax(size.y, size.x / aspect), 0.5f) * (1.0f + 2.0f * padding);
        if (home) height = opening.orthoHeight;
        // Rewrite the arguments as look({target = center, height = height}, duration, curve).
        lua_createtable(lua, 0, 2);
        lua_createtable(lua, 2, 0);
        lua_pushnumber(lua, 0.5f * (bounds.min.x + bounds.max.x));
        lua_rawseti(lua, -2, 1);
        lua_pushnumber(lua, 0.5f * (bounds.min.y + bounds.max.y));
        lua_rawseti(lua, -2, 2);
        lua_setfield(lua, -2, "target");
        lua_pushnumber(lua, height);
        lua_setfield(lua, -2, "height");
    }
    lua_replace(lua, 2);
    lua_pushnumber(lua, duration);
    lua_replace(lua, 3);
    return _lookCamera(lua, target);
}

static int _focus(lua_State* lua)
{
    return _focusCamera(lua, _sceneCamera(_scene(lua)));
}

// scene:view(rect, {camera, optical, background, opacity, layer, hide, follow}) draws the
// scene's own objects a second time in rect, given as a viewport's, through a camera of its
// own: a magnifier, an inset or a picture in picture. camera is a look table over the scene's
// camera; the background defaults to the scene's. Returns the view, whose look, focus, orbit
// and fade take the arguments of the scene's.
static int _sceneView(lua_State* lua)
{
    auto box = _scene(lua);
    auto scene = box->scene;
    lua_settop(lua, 3);
    auto& config = scene->config();
    SceneView view;
    view.viewport = _viewportValue(lua, 2);
    view.camera = config.camera;
    view.cameraView = config.cameraView;
    view.background = config.background;
    if (_optionTable(lua, 3, {"camera", "optical", "background", "opacity", "layer", "hide", "follow"})) {
        lua_getfield(lua, 3, "camera");
        if (!lua_isnil(lua, -1)) _lookFields(lua, box, -1, view.camera, view.cameraView);
        lua_pop(lua, 1);
        view.optical = _fieldBool(lua, 3, "optical", view.optical);
        _fieldColor(lua, 3, "background", view.background, &scene->theme());
        view.opacity = _fieldNumber(lua, 3, "opacity", view.opacity);
        if (view.opacity < 0.0f || view.opacity > 1.0f) return luaL_error(lua, "opacity must be between 0 and 1");
        auto layer = _fieldInteger(lua, 3, "layer", view.layer);
        if (layer < INT32_MIN || layer > INT32_MAX) return luaL_error(lua, "layer is out of range");
        view.layer = static_cast<int32_t>(layer);
        lua_getfield(lua, 3, "follow");
        if (!lua_isnil(lua, -1)) view.follow = _object(lua, scene, -1);
        lua_pop(lua, 1);
        // hide takes one object or a list of them; the list stays on the stack until the call.
        lua_getfield(lua, 3, "hide");
        auto hide = lua_gettop(lua);
        if (!lua_isnil(lua, hide)) {
            auto single = !lua_istable(lua, hide);
            auto count = single ? lua_Unsigned{1} : lua_rawlen(lua, hide);
            if (count > LUA_VIEW_HIDDEN_LIMIT) return luaL_error(lua, "a view hides at most 256 objects");
            if (count) {
                auto hidden = static_cast<const Object**>(lua_newuserdatauv(lua, sizeof(Object*) * count, 0));
                for (auto i = 0u; i < count; i++) {
                    if (!single) lua_rawgeti(lua, hide, static_cast<lua_Integer>(i) + 1);
                    hidden[i] = _object(lua, scene, single ? hide : -1);
                    if (!single) lua_pop(lua, 1);
                }
                view.hidden = hidden;
                view.hiddenCount = static_cast<uint32_t>(count);
            }
        }
    }
    if (scene->viewCount() >= LUA_VIEW_LIMIT) {
        return luaL_error(lua, "a scene holds at most 8 views in a script, as each draws the scene again");
    }
    LuaCharge charge{LUA_VIEW_CHARGE + sizeof(Object*) * view.hiddenCount};
    _fit(lua, box, charge);
    // The handle comes first, so the view never exists without one.
    auto handle = new (lua_newuserdatauv(lua, sizeof(LuaView), 1)) LuaView;
    luaL_getmetatable(lua, VIEW_METATABLE);
    lua_setmetatable(lua, -2);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    uint32_t index = 0;
    _check(lua, scene, scene->view(view, &index), "could not add view");
    _spend(box, charge);
    handle->scene = box;
    handle->index = index;
    handle->opening = view.camera;
    handle->openingView = view.cameraView;
    return 1;
}

// view:look, view:focus and view:orbit move the view's camera as the scene's methods of those
// names move the scene's, and frame targets in the view's rectangle.
static int _viewLook(lua_State* lua)
{
    return _lookCamera(lua, _viewCamera(lua));
}

static int _viewFocus(lua_State* lua)
{
    return _focusCamera(lua, _viewCamera(lua));
}

static int _viewOrbit(lua_State* lua)
{
    return _orbitCamera(lua, _viewCamera(lua));
}

// view:fade(opacity, duration = 1, curve = "smooth") fades the whole view; the fades of a view
// follow one another.
static int _viewFade(lua_State* lua)
{
    detail::luaTiming(lua, 3);
    auto view = _viewHandle(lua);
    auto box = view->scene;
    auto opacity = _number(lua, 2, "opacity");
    if (opacity < 0.0f || opacity > 1.0f) return luaL_error(lua, "opacity must be between 0 and 1");
    auto duration = _duration(lua, 3, 1.0f);
    auto curve = _curve(lua, 4);
    _fit(lua, box, LUA_CAMERA_CHARGE);
    auto result = box->scene->fade(view->index, opacity, duration, curve);
    _check(lua, box->scene, result, "could not fade view");
    _spend(box, LUA_CAMERA_CHARGE);
    return 0;
}

// ---------- Themes ----------

static ThemePreset _themePreset(lua_State* lua, int index)
{
    static constexpr LuaName<ThemePreset> PRESETS[] = {
        {"pro_white", ThemePreset::ProWhite}, {"pro_black", ThemePreset::ProBlack},
        {"adaptive_vscode", ThemePreset::AdaptiveVscode}};
    return _enum(lua, index, "theme preset", PRESETS);
}

static Theme _themePresetValue(lua_State* lua, int index)
{
    auto preset = _themePreset(lua, index);
    if (preset != ThemePreset::AdaptiveVscode) return Theme::preset(preset);
    void* memoryData = nullptr;
    lua_getallocf(lua, &memoryData);
    auto memory = static_cast<LuaMemory*>(memoryData);
    memory->adaptiveThemeUsed = true;
    return memory->hasAdaptiveTheme ? memory->adaptiveTheme : Theme::preset(preset);
}

static void _themeText(lua_State* lua, int index, const char* name, TextTheme& text)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    _checkTable(lua, -1, name);
    auto style = lua_absindex(lua, -1);
    _options(lua, style, {"font", "size", "color"}, false);
    lua_getfield(lua, style, "font");
    if (!lua_isnil(lua, -1)) {
        // The theme keeps the name until the scene copies it, and the options hold a string for
        // that long; a number would turn into a string that only the stack holds.
        if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "theme font must be a string");
        size_t size;
        auto font = lua_tolstring(lua, -1, &size);
        if (!size || size > LUA_PATH_LIMIT || std::strlen(font) != size) {
            luaL_error(lua, "theme font must be 1 to 4096 bytes without zero bytes");
        }
        text.font = font;
    }
    lua_pop(lua, 1);
    text.size = _fieldNumber(lua, style, "size", text.size);
    _fieldColor(lua, style, "color", text.color);
    if (text.size <= 0.0f) luaL_error(lua, "theme text size must be positive");
    lua_pop(lua, 1);
}

static Theme _theme(lua_State* lua, int index)
{
    // The colors of the colors table, by the names it gives them.
    static constexpr LuaName<Color SemanticTheme::*> COLORS[] = {
        {"foreground", &SemanticTheme::foreground}, {"muted", &SemanticTheme::muted},
        {"accent", &SemanticTheme::accent}, {"secondary", &SemanticTheme::secondary},
        {"success", &SemanticTheme::success}, {"warning", &SemanticTheme::warning},
        {"danger", &SemanticTheme::danger}, {"info", &SemanticTheme::info},
        {"surface", &SemanticTheme::surface}, {"border", &SemanticTheme::border},
        {"result", &SemanticTheme::result}, {"focus", &SemanticTheme::focus}};
    if (lua_type(lua, index) == LUA_TSTRING) {
        return _themePresetValue(lua, index);
    }
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    _options(lua, index,
             {"preset", "background", "text", "objects", "object_width", "roundness", "gradient",
              "end_gradient_stop", "axis", "colors"}, false);

    auto theme = Theme::preset(ThemePreset::ProWhite);
    lua_getfield(lua, index, "preset");
    if (!lua_isnil(lua, -1)) theme = _themePresetValue(lua, -1);
    lua_pop(lua, 1);
    _fieldColor(lua, index, "background", theme.background);
    theme.gradient = _fieldBool(lua, index, "gradient", theme.gradient);
    _fieldColor(lua, index, "end_gradient_stop", theme.endGradientStop);

    lua_getfield(lua, index, "text");
    if (!lua_isnil(lua, -1)) {
        _checkTable(lua, -1, "theme text");
        auto text = lua_absindex(lua, -1);
        _options(lua, text, {"h1", "h2", "h3", "text", "code"}, false);
        _themeText(lua, text, "h1", theme.h1);
        _themeText(lua, text, "h2", theme.h2);
        _themeText(lua, text, "h3", theme.h3);
        _themeText(lua, text, "text", theme.text);
        _themeText(lua, text, "code", theme.code);
    }
    lua_pop(lua, 1);

    lua_getfield(lua, index, "colors");
    if (!lua_isnil(lua, -1)) {
        _checkTable(lua, -1, "theme colors");
        auto colors = lua_absindex(lua, -1);
        _options(lua, colors,
                 {"foreground", "muted", "accent", "secondary", "success", "warning",
                  "danger", "info", "surface", "border", "result", "focus"}, false);
        for (auto& color : COLORS) _fieldColor(lua, colors, color.name, theme.colors.*color.value);
    }
    lua_pop(lua, 1);

    lua_getfield(lua, index, "objects");
    if (!lua_isnil(lua, -1)) {
        _checkTable(lua, -1, "theme objects");
        auto objects = lua_absindex(lua, -1);
        auto count = lua_rawlen(lua, objects);
        if (!count || count > Theme::ColorLimit) {
            luaL_error(lua, "theme objects must contain 1 to %d colors",
                       static_cast<int>(Theme::ColorLimit));
        }
        lua_pushnil(lua);
        while (lua_next(lua, objects)) {
            if (!lua_isinteger(lua, -2)) luaL_error(lua, "theme object colors must use array indices");
            auto key = lua_tointeger(lua, -2);
            if (key < 1 || static_cast<size_t>(key) > count) {
                luaL_error(lua, "theme object colors must be a dense array");
            }
            lua_pop(lua, 1);
        }
        for (auto i = 0u; i < count; i++) {
            lua_rawgeti(lua, objects, i + 1u);
            theme.objects[i] = _color(lua, -1, "theme object color");
            lua_pop(lua, 1);
        }
        theme.objectCount = static_cast<uint32_t>(count);
    }
    lua_pop(lua, 1);

    theme.objectWidth = _fieldNumber(lua, index, "object_width", theme.objectWidth);
    if (theme.objectWidth <= 0.0f) luaL_error(lua, "theme object_width must be positive");
    theme.roundness = _fieldNumber(lua, index, "roundness", theme.roundness);
    if (theme.roundness < 0.0f || theme.roundness > 0.5f) {
        luaL_error(lua, "theme roundness must be between 0 and 0.5");
    }

    lua_getfield(lua, index, "axis");
    if (!lua_isnil(lua, -1)) {
        _checkTable(lua, -1, "theme axis");
        auto axis = lua_absindex(lua, -1);
        _options(lua, axis, {"x", "y", "z", "grid", "label"}, false);
        _fieldColor(lua, axis, "x", theme.axis.x);
        _fieldColor(lua, axis, "y", theme.axis.y);
        _fieldColor(lua, axis, "z", theme.axis.z);
        _fieldColor(lua, axis, "grid", theme.axis.grid);
        _fieldColor(lua, axis, "label", theme.axis.label);
    }
    lua_pop(lua, 1);
    return theme;
}

// ---------- Scene creation, method tables and opening ----------

static int _sceneGen(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "scene authoring is unavailable after loading");
    }
    luaL_checktype(lua, 1, LUA_TTABLE);
    _options(lua, 1, {"width", "height", "fps", "background", "camera", "antialiasing", "loop", "theme"}, false);
    Config config;
    auto width = _fieldInteger(lua, 1, "width", config.width);
    auto height = _fieldInteger(lua, 1, "height", config.height);
    auto fps = _fieldInteger(lua, 1, "fps", config.fps);
    if (width <= 0 || width > UINT32_MAX || height <= 0 || height > UINT32_MAX || fps <= 0 || fps > UINT32_MAX) {
        return luaL_error(lua, "width, height and fps must be positive 32-bit integers");
    }
    config.width = static_cast<uint32_t>(width);
    config.height = static_cast<uint32_t>(height);
    config.fps = static_cast<uint32_t>(fps);
    config.antialiasing = _fieldBool(lua, 1, "antialiasing", config.antialiasing);
    config.loop = _fieldBool(lua, 1, "loop", config.loop);
    if (config.height > LUA_PIXEL_LIMIT / config.width) return luaL_error(lua, "scene pixel limit exceeded");
    auto backgroundAuthored = _fieldColor(lua, 1, "background", config.background);
    auto hasTheme = _has(lua, 1, "theme");
    Theme theme;
    if (hasTheme) {
        lua_getfield(lua, 1, "theme");
        theme = _theme(lua, -1);
        lua_pop(lua, 1);
    }

    if (_has(lua, 1, "camera")) {
        lua_getfield(lua, 1, "camera");
        _checkTable(lua, -1, "camera");
        auto index = lua_absindex(lua, -1);
        _options(lua, index,
                 {"mode", "view", "target", "eye", "up", "height", "projection", "fov", "near", "far"},
                 false);
        static constexpr LuaName<CameraMode> MODES[] = {
            {"fixed", CameraMode::Fixed}, {"interactive", CameraMode::Interactive}};
        config.cameraMode = _fieldEnum(lua, index, "mode", CameraMode::Fixed, MODES, "camera mode");
        config.cameraView = _fieldEnum(lua, index, "view", CameraView::TwoD, CAMERA_VIEWS, "camera view");
        if (config.cameraView == CameraView::TwoD) {
            config.camera.target = _fieldWorld(lua, index, "target", {});
            config.camera.eye = config.camera.target + Vec3{0.0f, 0.0f, 10.0f};
            config.camera.up = {0.0f, 1.0f, 0.0f};
            config.camera.projection = Projection::Orthographic;
            config.camera.orthoHeight = _fieldNumber(lua, index, "height", config.camera.orthoHeight);
        } else {
            config.camera.eye = _fieldWorld(lua, index, "eye", {6.0f, 4.0f, 6.0f});
            config.camera.target = _fieldWorld(lua, index, "target", config.camera.target);
            config.camera.up = _fieldWorld(lua, index, "up", config.camera.up);
            config.camera.fov = _fieldNumber(lua, index, "fov", config.camera.fov);
            config.camera.orthoHeight = _fieldNumber(lua, index, "height", config.camera.orthoHeight);
            config.camera.near = _fieldNumber(lua, index, "near", config.camera.near);
            config.camera.far = _fieldNumber(lua, index, "far", config.camera.far);
            config.camera.projection = _fieldEnum(lua, index, "projection", Projection::Perspective, PROJECTIONS);
        }
        lua_pop(lua, 1);
    }

    if (config.camera.near <= 0.0f || config.camera.far <= config.camera.near) {
        return luaL_error(lua, "camera requires 0 < near < far");
    }
    if (config.camera.fov <= 0.0f || config.camera.fov >= PI) {
        return luaL_error(lua, "camera fov must be in radians between 0 and pi");
    }
    if (config.camera.orthoHeight <= 0.0f) return luaL_error(lua, "camera height must be positive");

    void* memoryData = nullptr;
    lua_getallocf(lua, &memoryData);
    auto memory = static_cast<LuaMemory*>(memoryData);
    if (memory->scenes >= LUA_SCENE_LIMIT) return luaL_error(lua, "scene limit exceeded");
    if (memory->nativeUsed > LUA_NATIVE_MEMORY_LIMIT - LUA_SCENE_CHARGE) {
        return luaL_error(lua, "scene native memory limit exceeded");
    }
    auto box = new (lua_newuserdatauv(lua, sizeof(LuaScene), 0)) LuaScene;
    luaL_getmetatable(lua, SCENE_METATABLE);
    lua_setmetatable(lua, -2);
    box->scene = Scene::gen(config);
    if (!box->scene) return luaL_error(lua, "could not create scene: invalid configuration or out of memory");
    if (hasTheme) {
        _check(lua, box->scene, box->scene->theme(theme), "could not apply scene theme");
        if (backgroundAuthored) box->scene->config().background = config.background;
    }
    box->nativeUsed = LUA_SCENE_CHARGE;
    box->memory = memory;
    box->resolver = memory->resolver;
    box->resolverData = memory->resolverData;
    memory->nativeUsed += LUA_SCENE_CHARGE;
    memory->scenes++;
    return 1;
}

static const luaL_Reg FACTORY_METHODS[] = {
    {"group", _groupGen},
    {"space", _spaceGen},
    {"point", _point},
    {"line", _line},
    {"arrow", _arrow},
    {"vector", _vector},
    {"circle", _circle},
    {"arc", _arc},
    {"angle", _angle},
    {"rectangle", _rectangle},
    {"brace", _brace},
    {"polygon", _polygon},
    {"plot", _plot},
    {"contour", _contour},
    {"area", _area},
    {"riemann", _riemann},
    {"vector_field", _vectorField},
    {"stream_lines", _streamLines},
    {"route", _route},
    {"path", _path},
    {"union", _union},
    {"intersection", _intersection},
    {"difference", _difference},
    {"exclusion", _exclusion},
    {"curve", _curveGen},
    {"surface", _surface},
    {"text", _textWorld},
    {"spans", _spans},
    {"code", _code},
    {"number", _numberText},
    {"matrix", _matrixGen},
    {"table", _tableGen},
    {nullptr, nullptr}};

static const luaL_Reg SCENE_METHODS[] = {
    {"picture", _picture},
    {"cell", _cell},
    {"connector", _connector},
    {"viewport", _viewport},
    {"view", _sceneView},
    {"scene_transition", _sceneTransition},
    {"play", _playTargets},
    {"set", _setTargets},
    {"save", _save},
    {"restore", _restore},
    {"create", _create},
    {"uncreate", _uncreate},
    {"fill_reveal", _fillReveal},
    {"draw_border_then_fill", _drawBorderThenFill},
    {"write", _write},
    {"shift", _shift},
    {"transform", _transform},
    {"rotate", _rotate},
    {"scale", _scale},
    {"move_along", _moveAlong},
    {"advect", _advect},
    {"warp", _warp},
    {"fade", _fade},
    {"fade_in", _fadeIn},
    {"fade_out", _fadeOut},
    {"grow_from_center", _growFromCenter},
    {"grow_from_edge", _growFromEdge},
    {"shrink_to_center", _shrinkToCenter},
    {"indicate", _indicate},
    {"wiggle", _wiggle},
    {"circumscribe", _circumscribe},
    {"passing_flash", _passingFlash},
    {"flash", _flash},
    {"ripple", _ripple},
    {"morph", _morph},
    {"fade_transform", _fadeTransform},
    {"match_transform", _matchTransform},
    {"edit", _edit},
    {"highlight", _highlight},
    {"arrange", _arrangeOver},
    {"transition", _transition},
    {"stroke", _stroke},
    {"fill", _fill},
    {"look", _look},
    {"orbit", _orbit},
    {"focus", _focus},
    {"wait", _wait},
    {"parallel", _parallel},
    {"at", _at},
    {"mark", _mark},
    {"time", _sceneTime},
    {"seek", _sceneSeek},
    {"remove", _remove},
    {"clone", _clone},
    {"find", _find},
    {"bounds", _boundsOf},
    {"duration", _sceneDuration},
    {nullptr, nullptr}};

static const luaL_Reg OBJECT_METHODS[] = {
    {"label", _label},
    {"picture", _picture},
    {"cell", _cell},
    {"connector", _connector},
    {"move_to", _moveTo},
    {"next_to", _nextTo},
    {"align_to", _alignTo},
    {"mask", _maskObject},
    {"children", _children},
    {"measure", _measure},
    {"arrange", _arrange},
    {"arrange_grid", _arrangeGrid},
    {nullptr, nullptr}};

// A group also reaches its rows, the groups directly below it, and what they hold.
static const luaL_Reg GROUP_METHODS[] = {
    {"row", _rowOf},
    {"entry", _entryOf},
    {"column", _columnOf},
    {"search", _search},
    {nullptr, nullptr}};

// A space also samples cells and voxels over its ranges and places labels beside its ticks.
static const luaL_Reg SPACE_METHODS[] = {
    {"cell", _cellMethod},
    {"voxel", _voxel},
    {"labels", _labelsMethod},
    {nullptr, nullptr}};

static const luaL_Reg VIEW_METHODS[] = {
    {"look", _viewLook},
    {"focus", _viewFocus},
    {"orbit", _viewOrbit},
    {"fade", _viewFade},
    {nullptr, nullptr}};

static int _open(lua_State* lua)
{
    static const detail::LuaApi api = {
        FACTORY_METHODS, SCENE_METHODS, OBJECT_METHODS, GROUP_METHODS, SPACE_METHODS, VIEW_METHODS,
        _sceneGc, _objectGc, _sceneGen, _springResponse,
        _trackerEase, _trackerSettle, _trackerFps, _trackerUnknown, _trackerWhere, _trackerPending, _trackerSample,
    };
    return detail::luaOpenApi(lua, api);
}

bool detail::luaAuthoring(lua_State* lua) noexcept
{
    if (!lua) return false;
    void* data = nullptr;
    lua_getallocf(lua, &data);
    auto memory = static_cast<LuaMemory*>(data);
    return memory && memory->authoring;
}

void detail::luaClose(lua_State* lua) noexcept
{
    _close(lua, true);
}

Result detail::luaLoad(const char* source, uint32_t size, const char* name, Scene** output,
                       char* error, uint32_t errorSize, Lua::AssetResolver resolver,
                       void* resolverData, const char* sourcePath, size_t sourceRoot,
                       const Theme* adaptiveTheme, bool* adaptiveThemeUsed,
                       const LuaHookSpan& hooks, lua_State** retained) noexcept
{
    if (output) *output = nullptr;
    if (retained) *retained = nullptr;
    if (adaptiveThemeUsed) *adaptiveThemeUsed = false;
    if (error && errorSize) error[0] = '\0';
    if (!source || !output || (hooks.count && !hooks.data)) {
        _message(error, errorSize, "invalid Lua load arguments");
        return Result::InvalidArguments;
    }
    if (size > LUA_MEMORY_LIMIT) {
        _message(error, errorSize, "Lua source exceeds the 64 MiB limit");
        return Result::ScriptError;
    }

    NumericLocale locale;
    if (!locale.available()) {
        _message(error, errorSize, "could not activate the deterministic numeric locale");
        return Result::NonSupport;
    }

    LuaMemory localMemory;
    auto memory = retained ? new (std::nothrow) LuaMemory : &localMemory;
    if (!memory) {
        _message(error, errorSize, "could not allocate retained Lua memory state");
        return Result::OutOfMemory;
    }
    memory->resolver = resolver;
    memory->resolverData = resolverData;
    memory->sourcePath = sourcePath;
    memory->sourceRoot = sourceRoot;
    if (adaptiveTheme) {
        memory->adaptiveTheme = *adaptiveTheme;
        memory->hasAdaptiveTheme = true;
    }
    auto lua = lua_newstate(_allocate, memory);
    if (!lua) {
        if (retained) delete memory;
        _message(error, errorSize, "could not create Lua state");
        return Result::OutOfMemory;
    }

    lua_pushcfunction(lua, _open);
    if (lua_pcall(lua, 0, 0, 0) != LUA_OK) {
        _message(error, errorSize, lua_tostring(lua, -1));
        _close(lua, retained != nullptr);
        return Result::ScriptError;
    }
    for (auto i = 0u; i < hooks.count; i++) {
        auto hook = hooks.data + i;
        if (!hook->open) continue;
        lua_pushlightuserdata(lua, const_cast<LuaHooks*>(hook));
        lua_pushcclosure(lua, [](lua_State* state) -> int {
            auto active = static_cast<LuaHooks*>(lua_touserdata(state, lua_upvalueindex(1)));
            return active->open(state, active->data);
        }, 1);
        if (lua_pcall(lua, 0, 0, 0) != LUA_OK) {
            _message(error, errorSize, lua_tostring(lua, -1));
            _close(lua, retained != nullptr);
            return Result::ScriptError;
        }
    }

    auto chunk = name && *name ? name : "viscript";
    char* ownedName = nullptr;
    if (*chunk != '@' && *chunk != '=') {
        auto length = std::strlen(chunk);
        ownedName = new (std::nothrow) char[length + 2];
        if (!ownedName) {
            _message(error, errorSize, "out of memory while copying script name");
            _close(lua, retained != nullptr);
            return Result::OutOfMemory;
        }
        ownedName[0] = '@';
        std::memcpy(ownedName + 1, chunk, length + 1);
        chunk = ownedName;
    }

    auto status = luaL_loadbufferx(lua, source, size, chunk, "t");
    delete[] ownedName;
    if (status != LUA_OK) {
        _message(error, errorSize, lua_tostring(lua, -1));
        _close(lua, retained != nullptr);
        return Result::ScriptError;
    }

    lua_sethook(lua, _limit, LUA_MASKCOUNT, LUA_INSTRUCTION_LIMIT);
    status = lua_pcall(lua, 0, 1, 0);
    // The tracks that still wait play before the scene leaves the script.
    if (status == LUA_OK && memory->tracksPending) {
        memory->tracksPending = false;
        memory->tracksFlushing = true;
        lua_getfield(lua, LUA_REGISTRYINDEX, TRACKER_FLUSH);
        status = lua_pcall(lua, 0, 0, 0);
        memory->tracksFlushing = false;
    }
    if (status != LUA_OK) {
        _message(error, errorSize, lua_tostring(lua, -1));
        _close(lua, retained != nullptr);
        return Result::ScriptError;
    }

    auto box = static_cast<LuaScene*>(luaL_testudata(lua, -1, SCENE_METATABLE));
    if (!box || !box->scene || !box->owner) {
        char message[512];
        std::snprintf(message, sizeof(message), "%s: script must return an owning root viscript scene",
                      name && name[0] ? name : "viscript");
        _message(error, errorSize, message);
        _close(lua, retained != nullptr);
        return Result::ScriptError;
    }
    for (auto i = 0u; i < hooks.count; i++) {
        auto hook = hooks.data + i;
        if (!hook->loaded) continue;
        auto result = hook->loaded(lua, box->scene, hook->data);
        if (result != Result::Success) {
            _message(error, errorSize, _explain(result));
            _close(lua, retained != nullptr);
            return result;
        }
    }
    *output = box->scene;
    if (adaptiveThemeUsed) *adaptiveThemeUsed = memory->adaptiveThemeUsed;
    _releaseSceneBudget(box);
    box->owner = false;
    if (retained) {
        memory->resolver = nullptr;
        memory->resolverData = nullptr;
        memory->sourcePath = nullptr;
        memory->sourceRoot = 0u;
        memory->authoring = false;
        box->resolver = nullptr;
        box->resolverData = nullptr;
        *retained = lua;
        return Result::Success;
    }
    box->scene = nullptr;
    _close(lua, false);
    return Result::Success;
}

struct LuaFileAssets
{
    const char* path = nullptr;
    size_t root = 0;
    Asset* asset = nullptr;
};

static const Asset* _fileAsset(const char* name, void* data)
{
    auto files = static_cast<LuaFileAssets*>(data);
    delete files->asset;
    files->asset = nullptr;
    auto absolute = name[0] == '/' || name[0] == '\\' || (std::strlen(name) > 2u && name[1] == ':');
    if (absolute || !files->root) {
        AssetLoader::load(name, files->asset);
        return files->asset;
    }
    auto nameSize = std::strlen(name);
    if (files->root > SIZE_MAX - nameSize - 1u) return nullptr;
    auto joined = new (std::nothrow) char[files->root + nameSize + 1u];
    if (!joined) return nullptr;
    std::memcpy(joined, files->path, files->root);
    std::memcpy(joined + files->root, name, nameSize + 1u);
    AssetLoader::load(joined, files->asset);
    delete[] joined;
    return files->asset;
}

Result detail::luaLoadFile(const char* path, Scene** scene, char* error,
                           uint32_t errorSize, const LuaHookSpan& hooks,
                           lua_State** retained) noexcept
{
    if (scene) *scene = nullptr;
    if (error && errorSize) error[0] = '\0';
    if (!path || !scene) {
        _message(error, errorSize, "invalid Lua file arguments");
        return Result::InvalidArguments;
    }

    auto file = std::fopen(path, "rb");
    if (!file) {
        _message(error, errorSize, "could not open Lua file");
        return Result::IoError;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        _message(error, errorSize, "could not seek Lua file");
        return Result::IoError;
    }
    auto length = std::ftell(file);
    if (length < 0 || static_cast<unsigned long>(length) > LUA_MEMORY_LIMIT || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        _message(error, errorSize, "Lua file is too large or cannot be read");
        return Result::IoError;
    }

    auto data = new (std::nothrow) char[length ? static_cast<size_t>(length) : 1u];
    if (!data) {
        std::fclose(file);
        _message(error, errorSize, "could not allocate Lua file buffer");
        return Result::OutOfMemory;
    }
    auto read = std::fread(data, 1, static_cast<size_t>(length), file);
    auto close = std::fclose(file);
    if (read != static_cast<size_t>(length) || close != 0) {
        delete[] data;
        _message(error, errorSize, "could not read Lua file");
        return Result::IoError;
    }
    LuaFileAssets assets;
    assets.path = path;
    auto slash = std::strrchr(path, '/');
    auto backslash = std::strrchr(path, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
    if (slash) assets.root = static_cast<size_t>(slash - path) + 1u;
    auto result = detail::luaLoad(data, static_cast<uint32_t>(length), path, scene,
                                  error, errorSize, _fileAsset, &assets, path,
                                  assets.root, nullptr, nullptr, hooks, retained);
    delete assets.asset;
    delete[] data;
    return result;
}

Result Lua::load(const char* source, uint32_t size, const char* name, Scene** scene,
                 char* error, uint32_t errorSize, AssetResolver resolver,
                 void* resolverData, const Theme* adaptiveTheme,
                 bool* adaptiveThemeUsed) noexcept
{
    return detail::luaLoad(source, size, name, scene, error, errorSize, resolver,
                           resolverData, nullptr, 0, adaptiveTheme,
                           adaptiveThemeUsed);
}

}  // namespace vis
