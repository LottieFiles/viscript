// Native fixed-step 2D game template.
// Link viscript and vis-input; vis-ui and a window toolkit are not required.
// The host calls begin(), forwards normalized events, calls update(), then renders time 0.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <vis.h>
#include <vis_input.h>

namespace
{

using namespace vis;

template<typename Parent, typename T>
bool attach(Parent* parent, std::unique_ptr<T>& object)
{
    if (!parent || !object || parent->add(object.get()) != Result::Success) return false;
    object.release();
    return true;
}

class CollectorGame final
{
public:
    static std::unique_ptr<CollectorGame> gen()
    {
        auto game = std::unique_ptr<CollectorGame>(new CollectorGame);
        if (!game->build()) return nullptr;
        return game;
    }

    Scene* scene() const noexcept
    {
        return ownedScene.get();
    }

    input::State* inputState() const noexcept
    {
        return state.get();
    }

    input::ActionMap* actionMap() const noexcept
    {
        return actions.get();
    }

    Vec2 position() const noexcept
    {
        return playerPosition;
    }

    Vec2 target() const noexcept
    {
        return targetPosition;
    }

    uint32_t points() const noexcept
    {
        return score;
    }

    Result begin(float time) noexcept
    {
        return state ? state->begin(time) : Result::InvalidArguments;
    }

    Result input(const input::InputEvent& event) noexcept
    {
        return state ? state->input(event) : Result::InvalidArguments;
    }

    Result release(float time) noexcept
    {
        return state ? state->release(time) : Result::InvalidArguments;
    }

    Result update(float elapsed) noexcept
    {
        if (!ownedScene || !state || !actions || !std::isfinite(elapsed)
            || elapsed < 0.0f) {
            return Result::InvalidArguments;
        }
        input::ActionState move;
        input::ActionState boost;
        input::ActionState reset;
        input::ActionState place;
        auto result = actions->state(state.get(), "Move", move);
        if (result != Result::Success) return result;
        if ((result = actions->state(state.get(), "Boost", boost)) != Result::Success) {
            return result;
        }
        if ((result = actions->state(state.get(), "Reset", reset)) != Result::Success) {
            return result;
        }
        if ((result = actions->state(state.get(), "Place", place)) != Result::Success) {
            return result;
        }

        if (reset.pressed) {
            resetGame();
            elapsed = 0.0f;
        } else if (place.pressed) {
            input::PointerState pointer;
            if (state->pointer(0u, pointer) == Result::Success) {
                targetPosition = pixelToWorld(pointer.position);
                targetIndex = 0u;
                targetDirty = true;
            }
        }

        accumulator = std::fmin(accumulator + elapsed, MaxAccumulation);
        while (accumulator >= FixedStep) {
            step(move.value, boost.down, FixedStep);
            accumulator -= FixedStep;
        }
        if (playerDirty) {
            playerGroup->model = Mat4::translate({playerPosition.x, playerPosition.y, 0.0f});
            if ((result = ownedScene->update(playerGroup)) != Result::Success) return result;
            playerDirty = false;
        }
        if (targetDirty) {
            targetGroup->model = Mat4::translate({targetPosition.x, targetPosition.y, 0.0f});
            if ((result = ownedScene->update(targetGroup)) != Result::Success) return result;
            targetDirty = false;
        }
        return updateStatus(boost.down);
    }

private:
    static constexpr float SceneWidth = 960.0f;
    static constexpr float SceneHeight = 540.0f;
    static constexpr float CameraWidth = 16.0f;
    static constexpr float CameraHeight = 9.0f;
    static constexpr float FixedStep = 1.0f / 120.0f;
    static constexpr float MaxAccumulation = 0.25f;
    static constexpr float PlayerSpeed = 3.2f;
    static constexpr float BoostScale = 1.65f;
    static constexpr float PlayerRadius = 0.34f;
    static constexpr float TargetRadius = 0.28f;
    static constexpr float ArenaLeft = -6.55f;
    static constexpr float ArenaRight = 6.55f;
    static constexpr float ArenaBottom = -3.25f;
    static constexpr float ArenaTop = 2.25f;

    std::unique_ptr<Scene> ownedScene;
    std::unique_ptr<input::State> state;
    std::unique_ptr<input::ActionMap> actions;
    Group* playerGroup = nullptr;
    Group* targetGroup = nullptr;
    Text* scoreValue = nullptr;
    Text* stateValue = nullptr;
    Vec2 playerPosition = {-4.8f, -0.25f};
    Vec2 targetPosition = {3.8f, 1.15f};
    float accumulator = 0.0f;
    uint32_t targetIndex = 0u;
    uint32_t score = 0u;
    uint32_t displayedScore = UINT32_MAX;
    int8_t displayedBoost = -1;
    bool playerDirty = true;
    bool targetDirty = true;

    CollectorGame() = default;

    static float clamp(float value, float minimum, float maximum) noexcept
    {
        return std::fmax(minimum, std::fmin(maximum, value));
    }

    Vec2 pixelToWorld(const Vec2& point) const noexcept
    {
        auto x = (point.x / SceneWidth - 0.5f) * CameraWidth;
        auto y = (0.5f - point.y / SceneHeight) * CameraHeight;
        return {clamp(x, ArenaLeft, ArenaRight), clamp(y, ArenaBottom, ArenaTop)};
    }

    void nextTarget() noexcept
    {
        static constexpr Vec2 positions[] = {
            {3.8f, 1.15f}, {4.9f, -1.8f}, {1.4f, 1.75f}, {-1.3f, -2.15f},
            {-3.7f, 1.55f}, {5.4f, 0.1f}, {-5.2f, -1.55f}, {0.4f, 0.45f},
        };
        targetIndex = (targetIndex + 1u)
                    % static_cast<uint32_t>(sizeof(positions) / sizeof(positions[0]));
        targetPosition = positions[targetIndex];
        targetDirty = true;
    }

    void resetGame() noexcept
    {
        playerPosition = {-4.8f, -0.25f};
        targetPosition = {3.8f, 1.15f};
        accumulator = 0.0f;
        targetIndex = 0u;
        score = 0u;
        playerDirty = true;
        targetDirty = true;
    }

    void step(Vec2 direction, bool boost, float elapsed) noexcept
    {
        auto length = std::hypot(direction.x, direction.y);
        if (length > 1.0f) direction = direction * (1.0f / length);
        auto speed = PlayerSpeed * (boost ? BoostScale : 1.0f);
        auto previous = playerPosition;
        playerPosition = playerPosition + direction * (speed * elapsed);
        playerPosition.x = clamp(playerPosition.x, ArenaLeft, ArenaRight);
        playerPosition.y = clamp(playerPosition.y, ArenaBottom, ArenaTop);
        playerDirty = playerDirty || playerPosition.x != previous.x
                   || playerPosition.y != previous.y;
        auto separation = playerPosition - targetPosition;
        if (separation.length() > PlayerRadius + TargetRadius) return;
        score++;
        nextTarget();
    }

    Result updateStatus(bool boost) noexcept
    {
        if (displayedScore != score) {
            char value[48];
            std::snprintf(value, sizeof(value), "score  %u", score);
            auto result = scoreValue->text(value);
            if (result != Result::Success) return result;
            displayedScore = score;
        }
        auto active = boost ? 1 : 0;
        if (displayedBoost == active) return Result::Success;
        auto result = stateValue->text(boost ? "BOOST  1.65×" : "speed  3.2 u/s");
        if (result == Result::Success) displayedBoost = active;
        return result;
    }

    Text* addText(Scene* parent, const char* value, const Vec3& point, TextRole role,
                  Color color, const char* id, const Vec2& align = {0.0f, 0.5f})
    {
        auto text = std::unique_ptr<Text>(Text::gen(value, point));
        if (!text) return nullptr;
        text->role = role;
        text->align = align;
        text->layer = 40;
        text->fill(color).tag(id);
        auto handle = text.get();
        if (!attach(parent, text)) return nullptr;
        return handle;
    }

    Rectangle* addRectangle(Scene* parent, const Vec3& center, const Vec2& size,
                            Color fill, Color stroke, float width, const char* id,
                            int32_t layer, float corner = 0.0f)
    {
        auto rectangle = std::unique_ptr<Rectangle>(Rectangle::gen(center, size, corner));
        if (!rectangle) return nullptr;
        rectangle->layer = layer;
        rectangle->fill(fill).stroke(stroke, width).tag(id);
        auto handle = rectangle.get();
        if (!attach(parent, rectangle)) return nullptr;
        return handle;
    }

    bool bindActions()
    {
        if (!actions) return false;
        auto result = actions->key("Move", input::Key::A, {-1.0f, 0.0f});
        if (result != Result::Success) return false;
        if (actions->key("Move", input::Key::D, {1.0f, 0.0f}) != Result::Success
            || actions->key("Move", input::Key::W, {0.0f, 1.0f}) != Result::Success
            || actions->key("Move", input::Key::S, {0.0f, -1.0f}) != Result::Success
            || actions->key("Move", input::Key::ArrowLeft, {-1.0f, 0.0f})
                   != Result::Success
            || actions->key("Move", input::Key::ArrowRight, {1.0f, 0.0f})
                   != Result::Success
            || actions->key("Move", input::Key::ArrowUp, {0.0f, 1.0f})
                   != Result::Success
            || actions->key("Move", input::Key::ArrowDown, {0.0f, -1.0f})
                   != Result::Success
            || actions->key("Boost", input::Key::Space) != Result::Success
            || actions->key("Reset", input::Key::Escape) != Result::Success
            || actions->pointer("Place", 0u) != Result::Success) {
            return false;
        }
        return true;
    }

    bool buildPlayer(const Theme& theme)
    {
        auto& colors = theme.colors;
        auto player = std::unique_ptr<Group>(Group::gen());
        if (!player) return false;
        player->tag("collector:player");
        auto body = std::unique_ptr<Circle>(Circle::gen({}, PlayerRadius));
        if (!body) return false;
        body->layer = 20;
        body->fill(colors.accent).stroke(colors.foreground, 1.5f)
            .tag("collector:player:body");
        if (!attach(player.get(), body)) return false;
        auto core = std::unique_ptr<Circle>(Circle::gen({}, 0.09f));
        if (!core) return false;
        core->layer = 21;
        core->fill(theme.background).tag("collector:player:core");
        if (!attach(player.get(), core)) return false;
        playerGroup = player.get();
        return attach(ownedScene.get(), player);
    }

    bool buildTarget(const SemanticTheme& colors)
    {
        auto target = std::unique_ptr<Group>(Group::gen());
        if (!target) return false;
        target->tag("collector:target");
        auto ring = std::unique_ptr<Circle>(Circle::gen({}, 0.48f));
        if (!ring) return false;
        auto halo = colors.result;
        halo.a = 42;
        ring->layer = 10;
        ring->fill(halo).stroke(colors.result, 1.25f).tag("collector:target:ring");
        if (!attach(target.get(), ring)) return false;
        auto core = std::unique_ptr<Circle>(Circle::gen({}, TargetRadius));
        if (!core) return false;
        core->layer = 11;
        core->fill(colors.result).tag("collector:target:core");
        if (!attach(target.get(), core)) return false;
        targetGroup = target.get();
        return attach(ownedScene.get(), target);
    }

    bool build()
    {
        Config config;
        config.width = static_cast<uint32_t>(SceneWidth);
        config.height = static_cast<uint32_t>(SceneHeight);
        config.fps = 60u;
        config.loop = false;
        config.cameraMode = CameraMode::Fixed;
        config.cameraView = CameraView::TwoD;
        config.camera.orthoHeight = CameraHeight;
        ownedScene.reset(Scene::gen(config));
        state.reset(input::State::gen());
        actions.reset(input::ActionMap::gen());
        if (!ownedScene || !state || !actions
            || ownedScene->theme(Theme::preset(ThemePreset::AdaptiveVscode))
                   != Result::Success
            || !bindActions()) {
            return false;
        }
        auto& colors = ownedScene->theme().colors;
        if (!addText(ownedScene.get(), "Action map arena", {-7.05f, 3.86f, 0.0f},
                     TextRole::H2, colors.foreground, "collector:title")
            || !addText(ownedScene.get(),
                        "Collect the beacon with a fixed-step input loop.",
                        {-7.05f, 3.27f, 0.0f}, TextRole::Text, colors.muted,
                        "collector:subtitle")
            || !addText(ownedScene.get(),
                        "MOVE  WASD / arrows    BOOST  Space    PLACE TARGET  click    RESET  Esc",
                        {0.0f, -4.08f, 0.0f}, TextRole::Code, colors.muted,
                        "collector:controls", {0.5f, 0.5f})) {
            return false;
        }
        scoreValue = addText(ownedScene.get(), "score  0", {7.05f, 3.82f, 0.0f},
                             TextRole::Code, colors.result, "collector:score",
                             {1.0f, 0.5f});
        stateValue = addText(ownedScene.get(), "speed  3.2 u/s", {7.05f, 3.28f, 0.0f},
                             TextRole::Code, colors.muted, "collector:state",
                             {1.0f, 0.5f});
        if (!scoreValue || !stateValue) return false;

        auto surface = colors.surface;
        surface.a = 72;
        if (!addRectangle(ownedScene.get(), {0.0f, -0.5f, 0.0f}, {14.1f, 6.2f},
                          surface, colors.border, 1.25f, "collector:arena", 0, 0.08f)) {
            return false;
        }
        for (auto x = -4.0f; x <= 4.0f; x += 2.0f) {
            auto line = std::unique_ptr<Line>(Line::gen({x, ArenaBottom, 0.0f},
                                                       {x, ArenaTop, 0.0f}));
            if (!line) return false;
            auto grid = colors.border;
            grid.a = 60;
            line->layer = -2;
            line->stroke(grid, 1.0f);
            if (!attach(ownedScene.get(), line)) return false;
        }
        for (auto y = -2.0f; y <= 1.0f; y += 1.5f) {
            auto line = std::unique_ptr<Line>(Line::gen({ArenaLeft, y, 0.0f},
                                                       {ArenaRight, y, 0.0f}));
            if (!line) return false;
            auto grid = colors.border;
            grid.a = 60;
            line->layer = -2;
            line->stroke(grid, 1.0f);
            if (!attach(ownedScene.get(), line)) return false;
        }
        if (!buildPlayer(ownedScene->theme()) || !buildTarget(colors)) return false;
        resetGame();
        return update(0.0f) == Result::Success;
    }
};

}  // namespace

#if defined(VIS_INTERACTIVE_TEMPLATE_TEST)
int main(int argc, char** argv)
{
    auto game = CollectorGame::gen();
    if (!game || !game->scene() || !game->inputState() || !game->actionMap()) {
        return EXIT_FAILURE;
    }
    if (game->begin(1.0f) != vis::Result::Success) return EXIT_FAILURE;

    vis::input::InputEvent pointer;
    pointer.kind = vis::input::InputKind::PointerDown;
    pointer.pointer = 0u;
    pointer.position = {192.0f, 285.0f};
    pointer.button = 0;
    pointer.buttons = 1u;
    pointer.time = 1.001f;
    if (game->input(pointer) != vis::Result::Success
        || game->update(1.0f / 60.0f) != vis::Result::Success
        || game->points() != 1u) {
        return EXIT_FAILURE;
    }

    if (game->begin(1.02f) != vis::Result::Success) return EXIT_FAILURE;
    vis::input::InputEvent move;
    move.kind = vis::input::InputKind::KeyDown;
    move.key = vis::input::Key::D;
    move.time = 1.021f;
    if (game->input(move) != vis::Result::Success) return EXIT_FAILURE;
    auto before = game->position();
    if (game->update(0.1f) != vis::Result::Success
        || game->position().x <= before.x) {
        return EXIT_FAILURE;
    }

    if (game->begin(1.13f) != vis::Result::Success) return EXIT_FAILURE;
    move.kind = vis::input::InputKind::KeyDown;
    move.key = vis::input::Key::Escape;
    move.time = 1.131f;
    if (game->input(move) != vis::Result::Success
        || game->update(1.0f / 60.0f) != vis::Result::Success
        || game->points() != 0u || std::fabs(game->position().x + 4.8f) > 1.0e-4f) {
        return EXIT_FAILURE;
    }

    if (game->release(1.2f) != vis::Result::Success) return EXIT_FAILURE;
    if (argc == 3) {
        auto renderer = std::unique_ptr<vis::SwRenderer>(vis::SwRenderer::gen());
        vis::Surface surface;
        if (!renderer || renderer->font(argv[1]) != vis::Result::Success
            || renderer->render(game->scene(), 0.0f, surface) != vis::Result::Success
            || vis::Saver::png(surface, argv[2]) != vis::Result::Success) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
#endif
