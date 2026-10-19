// Native Key lifecycle monitor template.
// Link viscript and vis-input; vis-ui is not required.
// The host supplies monotonic seconds, forwards key events, calls tick(), then renders
// this zero-duration Scene at time 0. Register the Theme fonts before rendering Text.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>

#include <vis.h>
#include <vis_input.h>

namespace
{

using namespace vis;

template<typename T>
bool attach(Scene* scene, std::unique_ptr<T>& object)
{
    if (!scene || !object || scene->add(object.get()) != Result::Success) return false;
    object.release();
    return true;
}

class KeyLifecycleMonitor final
{
public:
    static std::unique_ptr<KeyLifecycleMonitor> gen()
    {
        auto monitor = std::unique_ptr<KeyLifecycleMonitor>(new KeyLifecycleMonitor);
        if (!monitor->build()) return nullptr;
        return monitor;
    }

    Scene* scene() const noexcept
    {
        return ownedScene.get();
    }

    input::Controller* controller() const noexcept
    {
        return inputController;
    }

    input::InputResult input(const input::InputEvent& event) noexcept
    {
        auto result = inputController ? inputController->input(event)
                                      : input::InputResult{false, false,
                                                           Result::InvalidArguments};
        if (result.status != Result::Success) return result;
        auto update = tick(event.time);
        if (update != Result::Success) result.status = update;
        else if (result.handled) result.redraw = true;
        return result;
    }

    Result tick(float time) noexcept
    {
        if (!ownedScene || !inputController || !std::isfinite(time) || time < 0.0f) {
            return Result::InvalidArguments;
        }
        input::KeyState state;
        auto result = inputController->keyState(input::Key::Space, state);
        if (result != Result::Success) return result;

        auto elapsed = state.down ? time - state.begin : state.end - state.begin;
        elapsed = std::max(0.0f, elapsed);
        auto amount = std::min(elapsed / HoldScaleSeconds, 1.0f);
        auto width = HoldBarWidth * amount;
        holdBar->center.x = HoldBarLeft + width * 0.5f;
        holdBar->size.x = std::max(width, 0.001f);

        auto& colors = ownedScene->theme().colors;
        auto focusFill = colors.focus;
        auto secondaryFill = colors.secondary;
        auto resultFill = colors.result;
        focusFill.a = secondaryFill.a = resultFill.a = 52;
        statePill->style.fill = state.down ? focusFill : colors.surface;
        statePill->style.stroke = state.down ? colors.focus : colors.border;
        downNode->style.fill = state.down ? focusFill : colors.surface;
        downNode->style.stroke = state.down ? colors.focus : colors.border;
        upNode->style.fill = !state.down && sawUp ? secondaryFill : colors.surface;
        upNode->style.stroke = !state.down && sawUp ? colors.secondary : colors.border;
        tapNode->style.fill = sawTap ? resultFill : colors.surface;
        tapNode->style.stroke = sawTap ? colors.result : colors.border;

        char status[64];
        std::snprintf(status, sizeof(status), "state  %s", state.down ? "DOWN" : "UP");
        if ((result = setText(stateValue, status)) != Result::Success) return result;

        char timing[128];
        if (state.down) {
            std::snprintf(timing, sizeof(timing),
                          "begin %.3f s    end —    held %.3f s", state.begin, elapsed);
        } else if (sawTap) {
            std::snprintf(timing, sizeof(timing),
                          "begin %.3f s    end %.3f s    duration %.3f s",
                          state.begin, state.end, elapsed);
        } else {
            std::snprintf(timing, sizeof(timing), "begin —    end —    duration —");
        }
        if ((result = setText(timingValue, timing)) != Result::Success) return result;

        char event[96];
        if (lastTrigger == input::KeyTrigger::Down) {
            std::snprintf(event, sizeof(event), "latest signal  Down @ %.3f s", lastEnd);
        } else if (lastTrigger == input::KeyTrigger::Up) {
            std::snprintf(event, sizeof(event), "latest signal  Up @ %.3f s", lastEnd);
        } else if (sawTap) {
            std::snprintf(event, sizeof(event),
                          "latest signal  Tap [%.3f s, %.3f s]", lastBegin, lastEnd);
        } else {
            std::snprintf(event, sizeof(event), "latest signal  —");
        }
        if ((result = setText(eventValue, event)) != Result::Success) return result;

        Object* changed[] = {holdBar, statePill, downNode, upNode, tapNode};
        for (auto object : changed) {
            result = ownedScene->update(object);
            if (result != Result::Success) return result;
        }
        return Result::Success;
    }

private:
    static constexpr float HoldBarLeft = -5.7f;
    static constexpr float HoldBarWidth = 11.4f;
    static constexpr float HoldScaleSeconds = 2.0f;

    std::unique_ptr<Scene> ownedScene;
    input::Controller* inputController = nullptr;
    Rectangle* statePill = nullptr;
    Rectangle* downNode = nullptr;
    Rectangle* upNode = nullptr;
    Rectangle* tapNode = nullptr;
    Rectangle* holdBar = nullptr;
    Text* stateValue = nullptr;
    Text* timingValue = nullptr;
    Text* eventValue = nullptr;
    input::KeyTrigger lastTrigger = input::KeyTrigger::Tap;
    float lastBegin = 0.0f;
    float lastEnd = 0.0f;
    bool sawUp = false;
    bool sawTap = false;

    KeyLifecycleMonitor() = default;

    static void signal(const input::KeySignal& value, void* data)
    {
        auto monitor = static_cast<KeyLifecycleMonitor*>(data);
        if (!monitor) return;
        monitor->lastTrigger = value.trigger;
        monitor->lastBegin = value.begin;
        monitor->lastEnd = value.end;
        if (value.trigger == input::KeyTrigger::Down) {
            monitor->sawUp = false;
            monitor->sawTap = false;
        } else if (value.trigger == input::KeyTrigger::Up) {
            monitor->sawUp = true;
        } else {
            monitor->sawTap = true;
        }
    }

    Result setText(Text* text, const char* value) noexcept
    {
        return text->text(value);
    }

    Text* addText(const char* value, const Vec3& point, TextRole role,
                  Color color, const char* id, const Vec2& align = {0.0f, 0.5f})
    {
        auto text = std::unique_ptr<Text>(Text::gen(value, point));
        if (!text) return nullptr;
        text->role = role;
        text->align = align;
        text->layer = 40;
        text->fill(color).tag(id);
        auto handle = text.get();
        if (!attach(ownedScene.get(), text)) return nullptr;
        return handle;
    }

    Rectangle* addRectangle(const Vec3& center, const Vec2& size, Color fill,
                            Color stroke, float width, const char* id, int32_t layer,
                            float corner = 0.08f)
    {
        auto rectangle = std::unique_ptr<Rectangle>(Rectangle::gen(center, size, corner));
        if (!rectangle) return nullptr;
        rectangle->layer = layer;
        rectangle->fill(fill).stroke(stroke, width).tag(id);
        auto handle = rectangle.get();
        if (!attach(ownedScene.get(), rectangle)) return nullptr;
        return handle;
    }

    bool build()
    {
        Config config;
        config.width = 960;
        config.height = 540;
        config.fps = 30;
        config.loop = false;
        config.cameraMode = CameraMode::Fixed;
        config.cameraView = CameraView::TwoD;
        config.camera.orthoHeight = 9.0f;
        ownedScene.reset(Scene::gen(config));
        if (!ownedScene
            || ownedScene->theme(Theme::preset(ThemePreset::AdaptiveVscode))
                   != Result::Success) {
            return false;
        }
        auto& colors = ownedScene->theme().colors;

        if (!addText("Keyboard lifecycle", {-7.05f, 3.72f, 0.0f}, TextRole::H2,
                     colors.foreground, "key-lifecycle:title")
            || !addText("Hold Space. Down is retained state; Up and Tap close the cycle.",
                        {-7.05f, 3.12f, 0.0f}, TextRole::Text, colors.muted,
                        "key-lifecycle:subtitle")) {
            return false;
        }

        statePill = addRectangle({5.52f, 3.49f, 0.0f}, {2.55f, 0.7f}, colors.surface,
                                 colors.border, 1.5f, "key-lifecycle:state-pill", 20);
        stateValue = addText("state  UP", {4.75f, 3.49f, 0.0f}, TextRole::Code,
                             colors.foreground, "key-lifecycle:state-value");
        if (!statePill || !stateValue) return false;

        auto rail = std::unique_ptr<Line>(Line::gen({-4.5f, 1.18f, 0.0f},
                                                   {4.5f, 1.18f, 0.0f}));
        if (!rail) return false;
        rail->layer = 10;
        rail->stroke(colors.border, 2.0f).tag("key-lifecycle:rail");
        if (!attach(ownedScene.get(), rail)) return false;

        downNode = addRectangle({-4.5f, 1.18f, 0.0f}, {2.35f, 1.35f}, colors.surface,
                                colors.border, 1.5f, "key-lifecycle:down:body", 20);
        upNode = addRectangle({0.0f, 1.18f, 0.0f}, {2.35f, 1.35f}, colors.surface,
                              colors.border, 1.5f, "key-lifecycle:up:body", 20);
        tapNode = addRectangle({4.5f, 1.18f, 0.0f}, {2.35f, 1.35f}, colors.surface,
                               colors.border, 1.5f, "key-lifecycle:tap:body", 20);
        if (!downNode || !upNode || !tapNode
            || !addText("Down", {-4.5f, 1.37f, 0.0f}, TextRole::H3, colors.foreground,
                        "key-lifecycle:down:title", {0.5f, 0.5f})
            || !addText("begin", {-4.5f, 0.92f, 0.0f}, TextRole::Code, colors.muted,
                        "key-lifecycle:down:label", {0.5f, 0.5f})
            || !addText("Up", {0.0f, 1.37f, 0.0f}, TextRole::H3, colors.foreground,
                        "key-lifecycle:up:title", {0.5f, 0.5f})
            || !addText("end", {0.0f, 0.92f, 0.0f}, TextRole::Code, colors.muted,
                        "key-lifecycle:up:label", {0.5f, 0.5f})
            || !addText("Tap", {4.5f, 1.37f, 0.0f}, TextRole::H3, colors.foreground,
                        "key-lifecycle:tap:title", {0.5f, 0.5f})
            || !addText("begin + end", {4.5f, 0.92f, 0.0f}, TextRole::Code,
                        colors.muted, "key-lifecycle:tap:label", {0.5f, 0.5f})) {
            return false;
        }

        if (!addText("hold duration / 2.0 s", {HoldBarLeft, -0.18f, 0.0f},
                     TextRole::Code, colors.muted, "key-lifecycle:hold-label")) {
            return false;
        }
        auto holdTrack = addRectangle({0.0f, -0.78f, 0.0f}, {HoldBarWidth, 0.24f},
                                      colors.surface, colors.border, 1.0f,
                                      "key-lifecycle:hold-track", 20);
        holdBar = addRectangle({HoldBarLeft, -0.78f, 0.0f}, {0.001f, 0.24f},
                               colors.focus, colors.focus, 0.0f,
                               "key-lifecycle:hold-value", 21, 0.0f);
        timingValue = addText("begin —    end —    duration —",
                              {HoldBarLeft, -1.45f, 0.0f}, TextRole::Code,
                              colors.foreground, "key-lifecycle:timing");
        eventValue = addText("latest signal  —", {HoldBarLeft, -2.08f, 0.0f},
                             TextRole::Code, colors.muted, "key-lifecycle:event");
        if (!holdTrack || !holdBar || !timingValue || !eventValue) return false;

        inputController = input::Controller::gen(ownedScene.get());
        if (!inputController) return false;
        input::KeyTriggerBinding binding;
        binding.key = input::Key::Space;
        binding.callback = signal;
        binding.data = this;
        binding.trigger = input::KeyTrigger::Down;
        if (inputController->keyTrigger(binding) != Result::Success) return false;
        binding.trigger = input::KeyTrigger::Up;
        if (inputController->keyTrigger(binding) != Result::Success) return false;
        binding.trigger = input::KeyTrigger::Tap;
        if (inputController->keyTrigger(binding) != Result::Success) return false;
        return tick(0.0f) == Result::Success;
    }
};

}  // namespace

#if defined(VIS_INTERACTIVE_TEMPLATE_TEST)
int main(int argc, char** argv)
{
    auto monitor = KeyLifecycleMonitor::gen();
    if (!monitor || !monitor->scene() || !monitor->controller()) return EXIT_FAILURE;

    vis::input::InputEvent event;
    event.kind = vis::input::InputKind::KeyDown;
    event.key = vis::input::Key::Space;
    event.time = 1.0f;
    auto result = monitor->input(event);
    if (!result.handled || !result.redraw || result.status != vis::Result::Success) {
        return EXIT_FAILURE;
    }

    vis::input::KeyState state;
    if (monitor->controller()->keyState(vis::input::Key::Space, state)
            != vis::Result::Success
        || !state.down || state.begin != 1.0f
        || monitor->tick(1.4f) != vis::Result::Success) {
        return EXIT_FAILURE;
    }

    event.kind = vis::input::InputKind::KeyUp;
    event.time = 1.65f;
    result = monitor->input(event);
    if (!result.handled || !result.redraw || result.status != vis::Result::Success
        || monitor->controller()->keyState(vis::input::Key::Space, state)
            != vis::Result::Success
        || state.down || state.end != 1.65f) {
        return EXIT_FAILURE;
    }

    auto object = monitor->scene()->object("key-lifecycle:timing");
    if (!object || object->type() != vis::Type::Text
        || !std::strstr(static_cast<vis::Text*>(object)->text(), "duration 0.650 s")) {
        return EXIT_FAILURE;
    }
    if (argc == 3) {
        auto renderer = std::unique_ptr<vis::Renderer>(vis::Renderer::gen());
        vis::Surface surface;
        if (!renderer || renderer->font(argv[1]) != vis::Result::Success
            || renderer->render(monitor->scene(), 0.0f, surface) != vis::Result::Success
            || vis::Saver::png(surface, argv[2]) != vis::Result::Success) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
#endif
