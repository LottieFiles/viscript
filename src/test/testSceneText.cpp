#include "testSceneChecks.h"

void countingText(Renderer* renderer)
{
    // A counting text writes its value with the format between its prefix and suffix.
    auto text = Text::gen("plain");
    CHECK(text && !text->counting() && text->value() == 0.0f);
    if (text) {
        NumberFormat grouped;
        grouped.separator = ',';
        CHECK(text->count(1234567.0f, grouped) == Result::Success);
        CHECK(text->counting() && text->value() == 1234567.0f && std::strcmp(text->text(), "1,234,567") == 0);
        NumberFormat signedTwo;
        signedTwo.decimals = 2;
        signedTwo.sign = true;
        CHECK(text->count(3.14159f, signedTwo, "x = ", " m") == Result::Success);
        CHECK(std::strcmp(text->text(), "x = +3.14 m") == 0);
        // A value that rounds to zero has no sign.
        CHECK(text->count(-0.004f, signedTwo) == Result::Success && std::strcmp(text->text(), "0.00") == 0);
        NumberFormat spaced;
        spaced.decimals = 1;
        spaced.separator = ' ';
        CHECK(text->count(-1234.56f, spaced) == Result::Success && std::strcmp(text->text(), "-1 234.6") == 0);
        CHECK(text->count(999.0f, grouped) == Result::Success && std::strcmp(text->text(), "999") == 0);
        CHECK(text->format().separator == ',' && text->format().decimals == 0u);
        char small[4];
        char room[32];
        CHECK(text->write(12345.0f, small, sizeof(small)) == 0u);
        CHECK(text->write(12345.0f, room, sizeof(room)) == 6u && std::strcmp(room, "12,345") == 0);
        CHECK(text->write(std::numeric_limits<float>::quiet_NaN(), room, sizeof(room)) == 0u);
        CHECK(text->write(-std::numeric_limits<float>::max(), room, sizeof(room)) == 0u);
        char huge[96];
        CHECK(text->write(-std::numeric_limits<float>::max(), huge, sizeof(huge)) > 50u && huge[0] == '-');
        // A refused format keeps the previous count.
        NumberFormat tooFine;
        tooFine.decimals = 7;
        NumberFormat control;
        control.separator = '\n';
        CHECK(text->count(1.0f, tooFine) == Result::InvalidArguments);
        CHECK(text->count(1.0f, control) == Result::InvalidArguments);
        CHECK(text->count(std::numeric_limits<float>::infinity()) == Result::InvalidArguments);
        CHECK(text->counting() && std::strcmp(text->text(), "999") == 0);
        // A plain string ends counting.
        CHECK(text->text("plain") == Result::Success && !text->counting() && text->value() == 0.0f);
        CHECK(text->write(1.0f, room, sizeof(room)) == 0u);
    }
    delete text;

    // Tabular text uses the widest digit advance, including spaces used to pad a counter.
    {
        Config cfg;
        cfg.width = 320;
        cfg.height = 180;
        cfg.camera.orthoHeight = 180.0f;
        auto owner = Scene::gen(cfg);
        auto label = Text::gen("1");
        label->size = 24.0f;
        label->align.x = 0.0f;
        TextExtent proportional[10];
        auto widest = 0.0f;
        for (auto digit = 0u; digit < 10u; digit++) {
            char value[] = {static_cast<char>('0' + digit), 0};
            CHECK(owner->measure(label, value, proportional[digit]) == Result::Success);
            widest = std::fmax(widest, proportional[digit].width);
        }
        CHECK(widest > proportional[1].width + 0.1f);
        label->tabular = true;
        for (auto digit = 0u; digit < 10u; digit++) {
            char value[] = {static_cast<char>('0' + digit), 0};
            TextExtent cell;
            CHECK(owner->measure(label, value, cell) == Result::Success);
            CHECK(near(cell.width, widest));
            CHECK(near(cell.left, proportional[digit].left + (widest - proportional[digit].width) * 0.5f));
            CHECK(near(cell.right, proportional[digit].right + (widest - proportional[digit].width) * 0.5f));
        }
        TextExtent sample, letter;
        CHECK(owner->measure(label, "a", letter) == Result::Success);
        CHECK(owner->measure(label, "a1", sample) == Result::Success && near(sample.width, letter.width + widest));
        CHECK(owner->measure(label, "- ", sample) == Result::Success && near(sample.width, 2.0f * widest));
        for (auto frame = 0u; frame < 30u; frame++) {
            char value[8];
            std::snprintf(value, sizeof(value), "%4u", (1000u * frame + 14u) / 29u);
            CHECK(owner->measure(label, value, sample) == Result::Success);
            CHECK(std::fabs(sample.width - 4.0f * widest) < 0.01f);
        }
        CHECK(owner->reserve(label, "0000") == Result::Success);
        CHECK(near(label->box.x, 4.0f * widest));
        auto leaned = Text::gen("1\n9", {70.0f, 0.0f});
        leaned->tabular = true;
        leaned->slant = -0.25f;
        leaned->size = label->size;
        leaned->align = {0.0f, 0.0f};
        leaned->orientation = TextOrientation::Plane;
        CHECK(owner->measure(leaned, sample) == Result::Success && sample.lines == 2u);
        CHECK(owner->add(leaned) == Result::Success);
        BBox ink;
        CHECK(renderer->bounds(owner, leaned, 0.0f, ink) == Result::Success);
        CHECK(std::fabs(ink.width - (sample.right - sample.left)) < 0.05f);
        CHECK(label->count(0.0f) == Result::Success && owner->add(label) == Result::Success);
        auto target = AnimationTarget::from(label);
        target.value(1000.0f);
        CHECK(owner->play(target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
        Surface kept, fresh;
        auto other = Renderer::gen();
        CHECK(renderer->render(owner, 1.0f, kept) == Result::Success);
        CHECK(renderer->render(owner, 0.25f, kept) == Result::Success);
        CHECK(other->render(owner, 0.25f, fresh) == Result::Success);
        CHECK(checksum(kept) == checksum(fresh));
        Object* copy = nullptr;
        CHECK(owner->clone(label, copy) == Result::Success);
        CHECK(copy && static_cast<Text*>(copy)->tabular);
        delete other;
        delete owner;
    }
    // Reservation measures a sample without changing the value and never follows later values.
    {
        auto reservedScene = Scene::gen();
        auto label = Text::gen("1");
        CHECK(reservedScene && label);
        TextExtent sample;
        TextExtent initial;
        CHECK(reservedScene->measure(label, "-000.00", sample) == Result::Success);
        CHECK(reservedScene->measure(label, initial) == Result::Success);
        CHECK(sample.width > initial.width);
        CHECK(reservedScene->reserve(label, "-000.00") == Result::Success);
        CHECK(near(label->box.x, sample.width) && std::strcmp(label->text(), "1") == 0);
        auto box = label->box;
        CHECK(reservedScene->reserve(label, nullptr) == Result::InvalidArguments);
        CHECK(near(label->box.x, box.x));
        CHECK(label->text("123456789012345") == Result::Success);
        CHECK(reservedScene->measure(label, initial) == Result::Success && initial.width > box.x);
        CHECK(near(label->box.x, box.x));
        CHECK(reservedScene->reserve(label, "1") == Result::Success && near(label->box.x, initial.width));
        CHECK(reservedScene->add(label) == Result::Success);
        CHECK(reservedScene->reserve(label, "000") == Result::InvalidArguments);
        delete reservedScene;
    }
    // The value animates, and every frame writes the number it has reached.
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto counter = Text::gen("", {0.0f, 1.5f, 0.0f});
    auto half = Text::gen("500", {0.0f, 0.0f, 0.0f});
    auto whole = Text::gen("1,000", {0.0f, -1.5f, 0.0f});
    CHECK(scene && counter && half && whole);
    if (scene && counter && half && whole) {
        NumberFormat grouped;
        grouped.separator = ',';
        CHECK(counter->count(0.0f, grouped) == Result::Success);
        Text* texts[] = {counter, half, whole};
        for (auto item : texts) {
            CHECK(item->font("Pretendard") == Result::Success);
            item->size = 28.0f;
        }
        CHECK(scene->add(counter) == Result::Success && scene->add(half) == Result::Success
              && scene->add(whole) == Result::Success);
        auto count = AnimationTarget::from(counter);
        count.value(1000.0f);
        CHECK(scene->play(count, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
        BBox counted;
        BBox expected;
        CHECK(renderer->bounds(scene, counter, 1.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, half, 1.0f, expected) == Result::Success);
        CHECK(near(counted.width, expected.width, 0.5f) && near(counted.height, expected.height, 0.5f));
        CHECK(renderer->bounds(scene, counter, 2.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, whole, 2.0f, expected) == Result::Success);
        CHECK(near(counted.width, expected.width, 0.5f));
        // The text keeps its latest authored count, like every other property.
        CHECK(counter->value() == 1000.0f && std::strcmp(counter->text(), "1,000") == 0);
        // Only a counting text takes a value.
        auto wrong = AnimationTarget::from(half);
        wrong.value(3.0f);
        CHECK(scene->play(wrong) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "counting text"));
        auto endless = AnimationTarget::from(counter);
        endless.value(std::numeric_limits<float>::infinity());
        CHECK(scene->play(endless) == Result::InvalidArguments);
        // A count and a move run side by side on their own channels.
        auto down = AnimationTarget::from(counter);
        down.value(0.0f);
        auto move = AnimationTarget::from(counter);
        move.shift({1.0f, 0.0f, 0.0f});
        CHECK(scene->play(down, 1.0f) == Result::Success);
        CHECK(scene->seek(2.0f) == Result::Success);
        CHECK(scene->play(move, 1.0f) == Result::Success);
        CHECK(counter->value() == 0.0f && std::strcmp(counter->text(), "0") == 0);
    }
    delete scene;
}

// Plays a linear text change of one second on a new text and checks what it shows a quarter,
// a half and three quarters of the way.
static void _textChange(const char* from, const char* to, TextChange change, const char* quarter,
                        const char* half, const char* threeQuarters)
{
    auto scene = Scene::gen({});
    auto text = Text::gen(from);
    auto added = scene && text && scene->add(text) == Result::Success;
    CHECK(added);
    if (!added) {
        delete text;
        delete scene;
        return;
    }
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    CHECK(scene->play(AnimationTarget::from(text).text(to, change), 1.0f, linear) == Result::Success);
    CHECK(std::strcmp(_shown(scene, text, 0.0f), from) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.25f), quarter) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.5f), half) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.75f), threeQuarters) == 0);
    CHECK(std::strcmp(_shown(scene, text, 1.0f), to) == 0);
    CHECK(std::strcmp(text->text(), to) == 0);
    delete scene;
}

static void _fadingText(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    const char* from[] = {"old sentence", "", "old sentence", "same sentence"};
    const char* to[] = {"new sentence", "new sentence", "", "same sentence"};
    for (auto example = 0u; example < 4u; example++) {
        auto scene = Scene::gen(config);
        auto text = Text::gen(from[example]);
        CHECK(scene && text && scene->add(text) == Result::Success);
        if (!scene || !text) { delete scene; delete text; continue; }
        auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
        CHECK(scene->play(AnimationTarget::from(text).text(to[example]), 1.0f, linear) == Result::Success);
        // Opacity and motion remain independent of the content transition.
        CHECK(scene->seek(0.0f) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(text).opacity(0.5f).shift({1.0f, 0.0f, 0.0f}), 1.0f, linear)
              == Result::Success);
        // Sample out of order as a player does after scrubbing.
        const float times[] = {1.0f, 0.25f, 0.5f, 0.0f, 0.75f};
        for (auto time : times) {
            auto value = example == 0u ? (time < 0.5f ? from[example] : to[example])
                       : example == 1u ? (time == 0.0f ? "" : to[example])
                       : example == 2u ? (time == 1.0f ? "" : from[example]) : from[example];
            auto alpha = example == 0u ? std::fabs(1.0f - 2.0f * time)
                       : example == 1u ? time : example == 2u ? 1.0f - time : 1.0f;
            CHECK(std::strcmp(_shown(scene, text, time), value) == 0);
            auto expected = Scene::gen(config);
            auto still = Text::gen(value);
            CHECK(expected && still);
            if (!expected || !still) { delete expected; delete still; continue; }
            still->opacity = (1.0f - time * 0.5f) * alpha;
            still->model = Mat4::translate({time, 0.0f, 0.0f});
            CHECK(expected->add(still) == Result::Success);
            Surface actual, reference;
            CHECK(renderer->render(scene, time, actual) == Result::Success);
            CHECK(renderer->render(expected, 0.0f, reference) == Result::Success);
            CHECK(checksum(actual) == checksum(reference));
            BBox bounds;
            CHECK(renderer->bounds(scene, text, time, bounds)
                  == (alpha == 0.0f || !*value ? Result::InsufficientCondition : Result::Success));
            delete expected;
        }
        if (example == 0u) {
            for (auto time : {0.25f, 0.5f, 0.75f}) {
                CHECK(scene->seek(time) == Result::Success);
                Object* copy = nullptr;
                CHECK(scene->clone(text, copy) == Result::Success);
                auto value = time < 0.5f ? from[example] : to[example];
                CHECK(copy && std::strcmp(static_cast<Text*>(copy)->text(), value) == 0);
                CHECK(copy && near(copy->opacity, (1.0f - time * 0.5f) * std::fabs(1.0f - 2.0f * time))
                      && near(copy->model.e[3], time));
            }
        }
        delete scene;
    }
    // Text effects must disappear with their source at the replacement midpoint.
    auto scene = Scene::gen(config);
    auto text = Text::gen("old sentence");
    text->blur = 1.0f;
    text->shadow = {Color{0, 0, 0, 255}, {8.0f, 8.0f}, 2.0f};
    CHECK(scene->add(text) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(text).text("new sentence"), 1.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    Surface midpoint;
    CHECK(renderer->render(scene, 0.25f, midpoint) == Result::Success);
    CHECK(extent(midpoint, config.background).valid);
    CHECK(renderer->render(scene, 0.5f, midpoint) == Result::Success);
    CHECK(!extent(midpoint, config.background).valid);
    // The same fade applies when the text itself supplies an alpha mask.
    auto card = Rectangle::gen({}, {8.0f, 4.0f});
    card->fill(Color{255, 0, 0, 255});
    CHECK(scene->seek(0.0f) == Result::Success && scene->add(card) == Result::Success);
    CHECK(card->mask(text) == Result::Success);
    CHECK(renderer->render(scene, 0.25f, midpoint) == Result::Success);
    CHECK(extent(midpoint, config.background).valid);
    CHECK(renderer->render(scene, 0.5f, midpoint) == Result::Success);
    CHECK(!extent(midpoint, config.background).valid);
    delete scene;
}

void textChanges(Renderer* renderer)
{
    _fadingText(renderer);
    _textChange("old sentence", "new sentence", TextChange::Fade, "old sentence", "new sentence", "new sentence");
    // Sweep replaces characters from the left; shortening removes them from the right.
    _textChange("cat", "horse", TextChange::Sweep, "hat", "hots", "horse");
    _textChange("horse", "cat", TextChange::Sweep, "horse", "hors", "hat");
    _textChange("abcd", "wxyz", TextChange::Sweep, "wbcd", "wxcd", "wxyd");
    _textChange("", "abc", TextChange::Sweep, "a", "ab", "abc");
    _textChange("abc", "", TextChange::Sweep, "abc", "ab", "a");
    // Characters, not bytes: syllables of three bytes, an emoji with its skin tone, a family
    // of joined emoji, a flag and a letter with its combining accent each change whole.
    _textChange("\xea\xb0\x80", "\xeb\x82\x98\xeb\x8b\xa4\xeb\x9d\xbc", TextChange::Sweep, "\xea\xb0\x80",
                "\xeb\x82\x98\xeb\x8b\xa4", "\xeb\x82\x98\xeb\x8b\xa4\xeb\x9d\xbc");
    _textChange("\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd!", "ok", TextChange::Sweep, "\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd!",
                "o!", "o!");
    _textChange("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7",
                "xyz", TextChange::Sweep,
                "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7",
                "x\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7z", "xyz");
    _textChange("e\xcc\x81" "a", "xyz", TextChange::Sweep, "e\xcc\x81" "a", "xaz", "xyz");
    // Retyping erases back to what both strings begin with, then types the rest.
    _textChange("Hello world", "Hello there", TextChange::Retype, "Hello wor", "Hello ", "Hello th");
    _textChange("", "abc", TextChange::Retype, "", "a", "ab");
    _textChange("\xea\xb0\x80\xeb\x82\x98", "\xea\xb0\x80\xeb\x8b\xa4\xeb\x9d\xbc", TextChange::Retype,
                "\xea\xb0\x80\xeb\x82\x98", "\xea\xb0\x80", "\xea\xb0\x80\xeb\x8b\xa4");

    auto scene = Scene::gen({});
    auto label = Text::gen("draft", {0.0f, 1.0f, 0.0f});
    auto left = Text::gen("cat", {-2.0f, -1.0f, 0.0f});
    auto counter = Text::gen("");
    auto circle = Circle::gen({0.0f, -1.0f, 0.0f}, 0.5f);
    CHECK(scene && label && left && counter && circle);
    if (!scene || !label || !left || !counter || !circle) {
        delete scene;
        delete label;
        delete left;
        delete counter;
        delete circle;
        return;
    }
    left->align = {0.0f, 0.5f};
    CHECK(counter->count(0.0f) == Result::Success);
    CHECK(scene->add(label) == Result::Success && scene->add(left) == Result::Success
          && scene->add(counter) == Result::Success && scene->add(circle) == Result::Success);
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto says = [scene](const char* words) { return scene->diagnostic() && std::strstr(scene->diagnostic(), words); };
    // Before the timeline a new string becomes the text's own, as Text::text() does.
    CHECK(scene->update(AnimationTarget::from(label).text("cat")) == Result::Success);
    CHECK(std::strcmp(label->text(), "cat") == 0 && std::strcmp(_shown(scene, label, 0.0f), "cat") == 0);
    CHECK(label->texts(nullptr, 0u) == 1u);
    // Text content is a channel of its own: a move, a fade and a recolor run beside it.
    AnimationTarget beside[] = {AnimationTarget::from(label).text("horse", TextChange::Sweep),
                                AnimationTarget::from(left).text("horse", TextChange::Sweep),
                                AnimationTarget::from(circle).fill({255, 0, 0, 255})};
    CHECK(scene->play(beside, 3u, 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).shift({1.0f, 0.0f, 0.0f}).opacity(0.5f), 1.0f, linear)
          == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).fill({0, 0, 255, 255}).text("cat", TextChange::Sweep), 1.0f, linear)
          == Result::Success);
    CHECK(std::strcmp(_shown(scene, label, 0.5f), "hots") == 0);
    CHECK(std::strcmp(_shown(scene, label, 1.5f), "hors") == 0);
    // Sampling is a pure function of time, in any order.
    CHECK(std::strcmp(_shown(scene, label, 0.25f), "hat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 1.75f), "hat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 0.5f), "hots") == 0);
    // The text keeps the string its latest change ends at, and the strings come back.
    CHECK(std::strcmp(label->text(), "cat") == 0);
    const char* strings[3] = {};
    CHECK(label->texts(strings, 3u) == 2u && std::strcmp(strings[0], "cat") == 0
          && std::strcmp(strings[1], "horse") == 0);
    // A change may not begin while another runs, and a plain string waits for none.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).text("dog")) == Result::InsufficientCondition);
    CHECK(says("already animates its text until 2.00 s"));
    CHECK(label->text("dog") == Result::InsufficientCondition && std::strcmp(label->text(), "cat") == 0);
    // After the timeline starts, a set switches the string at the cursor.
    CHECK(scene->seek(2.5f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(label).text("dog")) == Result::Success);
    CHECK(std::strcmp(_shown(scene, label, 2.4f), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 2.5f), "dog") == 0);
    // Only a text that is not a number changes its string, and only to a string.
    CHECK(scene->play(AnimationTarget::from(circle).text("x")) == Result::InvalidArguments);
    CHECK(says("text needs a text object"));
    CHECK(scene->play(AnimationTarget::from(counter).text("x")) == Result::InvalidArguments);
    CHECK(says("counting text changes through its value"));
    CHECK(scene->play(AnimationTarget::from(label).text(nullptr)) == Result::InvalidArguments);
    CHECK(scene->play(AnimationTarget::from(label).text("x", static_cast<TextChange>(7))) == Result::InvalidArguments);
    CHECK(says("the text change is unknown"));
    char room[4];
    CHECK(scene->text(label, 1.0f, room, sizeof(room)) == 5u && room[0] == '\0');
    CHECK(scene->text(counter, 0.0f, room, sizeof(room)) == 1u && std::strcmp(room, "0") == 0);
    CHECK(scene->text(label, -1.0f, room, sizeof(room)) == 0u);
    CHECK(scene->text(nullptr, 0.0f, room, sizeof(room)) == 0u);
    // The renderer draws the string the time shows, pinned to the text's point by its align as
    // the width changes: about the center by default, from the left edge here.
    BBox shorter;
    BBox longer;
    CHECK(renderer->bounds(scene, label, 1.75f, shorter) == Result::Success);
    CHECK(renderer->bounds(scene, label, 1.0f, longer) == Result::Success);
    CHECK(longer.width > shorter.width * 1.3f);
    CHECK(near(shorter.x + shorter.width * 0.5f, longer.x + longer.width * 0.5f, 3.0f));
    CHECK(renderer->bounds(scene, left, 0.25f, shorter) == Result::Success);
    CHECK(renderer->bounds(scene, left, 1.0f, longer) == Result::Success);
    CHECK(longer.width > shorter.width * 1.3f && near(shorter.x, longer.x, 1.0f));
    TextExtent hat;
    TextExtent horse;
    CHECK(label->measure("hat", hat) == Result::Success && label->measure("horse", horse) == Result::Success);
    CHECK(hat.width < horse.width && label->measure(nullptr, hat) == Result::InvalidArguments);
    delete scene;
}

void matchedPieces(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;

    // Mistakes leave the timeline as it was.
    auto checks = Scene::gen(config);
    auto kept = Group::gen();
    auto inside = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto busy = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto early = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(checks && kept && inside && busy && early);
    if (checks && kept && inside && busy && early) {
        CHECK(kept->add(inside) == Result::Success);
        CHECK(checks->add(kept) == Result::Success && checks->add(busy) == Result::Success
              && checks->add(early) == Result::Success);
        CHECK(checks->matchTransform(kept, kept) == Result::InvalidArguments);
        CHECK(checks->matchTransform(kept, inside) == Result::InvalidArguments);
        CHECK(checks->play(Animation::shift(busy, {0.0f, 1.0f, 0.0f}), 2.0f) == Result::Success);
        CHECK(checks->seek(1.0f) == Result::Success);
        auto late = Circle::gen({0.0f, 2.0f, 0.0f}, 0.5f);
        CHECK(late && checks->add(late) == Result::Success);
        auto clips = checks->clipCount();
        CHECK(checks->matchTransform(busy, late) == Result::InsufficientCondition);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "at rest"));
        CHECK(checks->matchTransform(kept, early) == Result::InsufficientCondition);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "right before the match"));
        CHECK(checks->matchTransform(kept, late, 0.0f) == Result::InvalidArguments);
        MatchOptions turn;
        turn.arc = 7.0f;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, turn) == Result::InvalidArguments);
        MatchOptions faded;
        faded.fade = 0.0f;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, faded) == Result::InvalidArguments);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "fade share"));
        faded.fade = std::numeric_limits<float>::quiet_NaN();
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, faded) == Result::InvalidArguments);
        Object* foreign[] = {busy, late};
        MatchOptions stray;
        stray.pairs = foreign;
        stray.pairCount = 1;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, stray) == Result::InvalidArguments);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "each pair"));
        CHECK(checks->clipCount() == clips && near(checks->cursor(), 1.0f) && near(checks->duration(), 2.0f));
        // A plain shape pairs with a plain shape only when asked; unpaired, they cross over.
        CHECK(checks->matchTransform(kept, late, 1.0f) == Result::Success);
        CHECK(near(checks->cursor(), 2.0f) && checks->clipCount() == clips + 2u);
        CHECK(checks->play(Animation::fadeOut(kept)) == Result::InvalidArguments);
    } else {
        delete kept;
        delete inside;
        delete busy;
        delete early;
    }
    delete checks;

    // Texts that read the same pair by themselves unless the caller pairs everything: one
    // clip carries a pair, and two fade an unpaired text out and its twin in.
    const bool modes[] = {true, false};
    for (auto strings : modes) {
        auto twins = Scene::gen(config);
        auto from = Group::gen();
        auto to = Group::gen();
        auto first = Text::gen("x", {-1.0f, 0.0f, 0.0f});
        auto second = Text::gen("x", {1.0f, 0.0f, 0.0f});
        CHECK(twins && from && to && first && second);
        if (!twins || !from || !to || !first || !second) {
            delete twins;
            delete from;
            delete to;
            delete first;
            delete second;
            continue;
        }
        CHECK(from->add(first) == Result::Success && to->add(second) == Result::Success);
        CHECK(twins->add(from) == Result::Success && twins->wait(1.0f) == Result::Success);
        CHECK(twins->add(to) == Result::Success);
        MatchOptions options;
        options.strings = strings;
        CHECK(twins->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
        CHECK(twins->clipCount() == (strings ? 1u : 2u));
        delete twins;
    }

    // Texts that read the same travel; a given pair cross-fades; the rest fades.
    auto scene = Scene::gen(config);
    auto source = Group::gen();
    auto target = Group::gen();
    auto nested = Group::gen();
    auto a = Text::gen("a", {-2.0f, 0.0f, 0.0f});
    auto plus = Text::gen("+", {-1.0f, 0.0f, 0.0f});
    auto square = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.6f, 0.6f});
    auto gone = Text::gen("z", {2.0f, 0.0f, 0.0f});
    auto plusTo = Text::gen("+", {-1.0f, 1.5f, 0.0f});
    auto aTo = Text::gen("a", {1.5f, 1.5f, 0.0f});
    auto round = Circle::gen({1.0f, -1.5f, 0.0f}, 0.4f);
    auto fresh = Text::gen("c", {-2.0f, -1.5f, 0.0f});
    CHECK(scene && source && target && nested && a && plus && square && gone && plusTo && aTo && round && fresh);
    if (scene && source && target && nested && a && plus && square && gone && plusTo && aTo && round && fresh) {
        Text* texts[] = {a, plus, gone, plusTo, aTo, fresh};
        for (auto item : texts) {
            CHECK(item->font("Pretendard") == Result::Success);
            item->size = 28.0f;
        }
        aTo->size = 56.0f;
        a->fill(Color::hex("#ef5350"));
        aTo->fill(Color::hex("#42a5f5"));
        CHECK(source->add(a) == Result::Success && source->add(plus) == Result::Success
              && source->add(square) == Result::Success && source->add(gone) == Result::Success);
        CHECK(nested->add(aTo) == Result::Success);
        CHECK(target->add(plusTo) == Result::Success && target->add(nested) == Result::Success
              && target->add(round) == Result::Success && target->add(fresh) == Result::Success);
        CHECK(scene->add(source) == Result::Success);
        CHECK(scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        Object* pairs[] = {square, round};
        MatchOptions options;
        options.arc = 1.5f;
        options.pairs = pairs;
        options.pairCount = 1;
        CHECK(scene->matchTransform(source, target, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        CHECK(near(scene->cursor(), 3.0f) && near(scene->duration(), 3.0f));

        // The source text leaves as its pair takes its place, size and color.
        BBox before;
        BBox start;
        BBox middle;
        BBox end;
        CHECK(renderer->bounds(scene, a, 0.9f, before) == Result::Success);
        CHECK(renderer->bounds(scene, a, 1.0f, start) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, aTo, 1.0f, start) == Result::Success);
        CHECK(near(start.x, before.x, 0.5f) && near(start.y, before.y, 0.5f));
        CHECK(near(start.width, before.width, 0.5f) && near(start.height, before.height, 0.5f));
        Surface surface;
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto inked = [&](const BBox& box, Color color) {
            auto wanted = pixel(color);
            for (auto y = static_cast<uint32_t>(box.y); y < static_cast<uint32_t>(box.y + box.height); y++) {
                for (auto x = static_cast<uint32_t>(box.x); x < static_cast<uint32_t>(box.x + box.width); x++) {
                    if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == wanted) return true;
                }
            }
            return false;
        };
        CHECK(inked(start, Color::hex("#ef5350")));
        // It grows and turns blue on the way, along an arc, and lands in its own place.
        CHECK(renderer->bounds(scene, aTo, 2.0f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, aTo, 3.0f, end) == Result::Success);
        CHECK(end.width > start.width * 1.6f && end.center().x > start.center().x + 80.0f);
        CHECK(std::fabs(middle.center().y - (start.center().y + end.center().y) * 0.5f) > 10.0f);
        CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
        CHECK(inked(end, Color::hex("#42a5f5")) && !inked(end, Color::hex("#ef5350")));
        // Unpaired pieces fade: one in, one out with its object.
        BBox unseen;
        CHECK(renderer->bounds(scene, fresh, 1.0f, unseen) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, fresh, 3.0f, unseen) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 2.0f, unseen) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 3.0f, unseen) == Result::InsufficientCondition);
        // The given pair cross-fades while each travels to the other's place.
        BBox squareStart;
        BBox roundEnd;
        BBox roundMiddle;
        BBox squareMiddle;
        CHECK(renderer->bounds(scene, square, 0.9f, squareStart) == Result::Success);
        CHECK(renderer->bounds(scene, round, 1.0f, unseen) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, round, 2.0f, roundMiddle) == Result::Success);
        CHECK(renderer->bounds(scene, square, 2.0f, squareMiddle) == Result::Success);
        CHECK(near(roundMiddle.center().x, squareMiddle.center().x, 1.0f)
              && near(roundMiddle.center().y, squareMiddle.center().y, 1.0f));
        CHECK(renderer->bounds(scene, round, 3.0f, roundEnd) == Result::Success);
        CHECK(renderer->bounds(scene, square, 3.0f, unseen) == Result::InsufficientCondition);
        CHECK(roundEnd.center().y > squareStart.center().y + 30.0f);
        // The source is gone; the target plays on.
        CHECK(scene->play(Animation::fadeOut(source)) == Result::InvalidArguments);
        CHECK(scene->play(Animation::indicate(aTo)) == Result::Success);

        // Lag starts each later piece later, counting the pieces of each object in order.
        auto next = Group::gen();
        auto plusNext = Text::gen("+", {-1.0f, -1.0f, 0.0f});
        auto aNext = Text::gen("a", {1.0f, -1.0f, 0.0f});
        CHECK(next && plusNext && aNext);
        if (next && plusNext && aNext) {
            Text* more[] = {plusNext, aNext};
            for (auto item : more) {
                CHECK(item->font("Pretendard") == Result::Success);
                item->size = 28.0f;
                CHECK(next->add(item) == Result::Success);
            }
            CHECK(scene->add(next) == Result::Success);
            auto begin = scene->cursor();
            MatchOptions staggered;
            staggered.lag = 0.5f;
            CHECK(scene->matchTransform(target, next, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), staggered)
                  == Result::Success);
            // The target had four pieces, so the last one starts three lags in.
            CHECK(near(scene->cursor(), begin + 2.5f));
            BBox waiting;
            BBox moving;
            BBox first;
            CHECK(renderer->bounds(scene, aNext, begin, first) == Result::Success);
            CHECK(renderer->bounds(scene, aNext, begin + 0.4f, waiting) == Result::Success);
            CHECK(near(waiting.x, first.x, 0.5f) && near(waiting.y, first.y, 0.5f));
            CHECK(renderer->bounds(scene, plusNext, begin + 0.4f, moving) == Result::Success);
            BBox plusFirst;
            CHECK(renderer->bounds(scene, plusNext, begin, plusFirst) == Result::Success);
            CHECK(std::fabs(moving.y - plusFirst.y) > 5.0f);
        } else {
            delete next;
            delete plusNext;
            delete aNext;
        }
    }
    delete scene;
}

// A group of lines, each a group of the texts of one string split at spaces, set at one unit
// per text and per line from the top left.
static Group* _lines(const char* const* lines, uint32_t count, Text** texts = nullptr)
{
    auto block = Group::gen();
    if (!block) return nullptr;
    auto made = 0u;
    for (auto row = 0u; row < count; row++) {
        auto line = Group::gen();
        if (!line || block->add(line) != Result::Success) {
            delete line;
            delete block;
            return nullptr;
        }
        char word[16];
        auto column = 0u;
        for (auto at = lines[row]; *at;) {
            auto length = 0u;
            while (at[length] && at[length] != ' ' && length < 15u) length++;
            std::memcpy(word, at, length);
            word[length] = '\0';
            auto text = Text::gen(word, {-3.0f + static_cast<float>(column++), 2.0f - static_cast<float>(row), 0.0f});
            if (!text || line->add(text) != Result::Success) {
                delete text;
                delete block;
                return nullptr;
            }
            text->font("Pretendard");
            text->size = 20.0f;
            if (texts) texts[made] = text;
            made++;
            at += length;
            while (*at == ' ') at++;
        }
    }
    return block;
}

void diffedLines(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;

    // A diff keeps order: words that cross a line kept in place fade, where strings would
    // carry them; one clip carries a pair, and two fade an unpaired text out and its twin in.
    const char* const crossed[] = {"a b", "c"};
    const char* const across[] = {"c", "b a"};
    const bool modes[] = {false, true};
    for (auto diff : modes) {
        auto scene = Scene::gen(config);
        auto from = _lines(crossed, 2u);
        auto to = _lines(across, 2u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
            CHECK(scene->add(to) == Result::Success);
            MatchOptions options;
            options.diff = diff;
            CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
            CHECK(scene->clipCount() == (diff ? 5u : 3u));
        } else {
            delete from;
            delete to;
        }
        delete scene;
    }

    // A line that reads as one left over elsewhere moves there whole.
    const char* const pair[] = {"a b", "c d"};
    const char* const swapped[] = {"c d", "a b"};
    auto moved = Scene::gen(config);
    auto from = _lines(pair, 2u);
    auto to = _lines(swapped, 2u);
    CHECK(moved && from && to);
    if (moved && from && to) {
        CHECK(moved->add(from) == Result::Success && moved->wait(1.0f) == Result::Success);
        CHECK(moved->add(to) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(moved->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
        CHECK(moved->clipCount() == 4u);
    } else {
        delete from;
        delete to;
    }
    delete moved;

    // More lines than a diff takes leave the timeline as it was.
    auto crowded = Scene::gen(config);
    auto many = Group::gen();
    auto one = Text::gen("x", {0.0f, 0.0f, 0.0f});
    CHECK(crowded && many && one);
    if (crowded && many && one) {
        for (auto i = 0u; i < 4097u; i++) {
            auto line = Group::gen();
            if (!line || many->add(line) != Result::Success) delete line;
        }
        CHECK(many->childCount() == 4097u);
        CHECK(crowded->add(many) == Result::Success && crowded->wait(1.0f) == Result::Success);
        CHECK(crowded->add(one) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(crowded->matchTransform(many, one, 1.0f, {}, options) == Result::InvalidArguments);
        CHECK(crowded->diagnostic() && std::strstr(crowded->diagnostic(), "4096 lines"));
        CHECK(crowded->clipCount() == 0u && near(crowded->cursor(), 1.0f));
    } else {
        delete many;
        delete one;
    }
    delete crowded;

    // An edit: a line comes in above, the kept lines move down, and a changed line keeps the
    // words it shares while the rest fades.
    const char* const before[] = {"x = 1", "y = 2", "end"};
    const char* const after[] = {"z = 0", "x = 1", "y = 3", "end"};
    Text* old[7] = {};
    Text* edited[10] = {};
    auto scene = Scene::gen(config);
    auto source = _lines(before, 3u, old);
    auto target = _lines(after, 4u, edited);
    CHECK(scene && source && target);
    if (scene && source && target) {
        CHECK(scene->add(source) == Result::Success && scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        auto startsAt = [&](Text* piece, Text* place) {
            BBox start;
            BBox there;
            return renderer->bounds(scene, piece, 1.0f, start) == Result::Success
                   && renderer->bounds(scene, place, 0.9f, there) == Result::Success && near(start.x, there.x, 0.5f)
                   && near(start.y, there.y, 0.5f);
        };
        auto unseen = [&](Text* piece) {
            BBox box;
            return renderer->bounds(scene, piece, 1.0f, box) == Result::InsufficientCondition;
        };
        // x = 1 and end kept their lines; y and = kept their places in the changed line.
        CHECK(startsAt(edited[3], old[0]) && startsAt(edited[4], old[1]) && startsAt(edited[5], old[2]));
        CHECK(startsAt(edited[6], old[3]) && startsAt(edited[7], old[4]) && startsAt(edited[9], old[6]));
        // The new line comes in whole, = too, and 3 replaces 2.
        CHECK(unseen(edited[0]) && unseen(edited[1]) && unseen(edited[2]) && unseen(edited[8]));
        BBox leaving;
        CHECK(renderer->bounds(scene, old[5], 1.5f, leaving) == Result::Success);
        CHECK(renderer->bounds(scene, old[5], 2.0f, leaving) == Result::InsufficientCondition);
    } else {
        delete source;
        delete target;
    }
    delete scene;

    // With a share of 0.4, what leaves is gone before what arrives shows, while a pair
    // travels the whole time.
    const char* const kept[] = {"a b"};
    const char* const changed[] = {"c a"};
    Text* first[2] = {};
    Text* second[2] = {};
    scene = Scene::gen(config);
    source = _lines(kept, 1u, first);
    target = _lines(changed, 1u, second);
    CHECK(scene && source && target);
    if (scene && source && target) {
        CHECK(scene->add(source) == Result::Success && scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        MatchOptions options;
        options.fade = 0.4f;
        CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        CHECK(near(scene->cursor(), 2.0f));
        BBox box;
        CHECK(renderer->bounds(scene, first[1], 1.2f, box) == Result::Success);
        CHECK(renderer->bounds(scene, first[1], 1.45f, box) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, second[0], 1.5f, box) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, second[0], 1.8f, box) == Result::Success);
        BBox start;
        BBox middle;
        BBox end;
        CHECK(renderer->bounds(scene, second[1], 1.0f, start) == Result::Success);
        CHECK(renderer->bounds(scene, second[1], 1.5f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, second[1], 2.0f, end) == Result::Success);
        CHECK(near(middle.x, (start.x + end.x) * 0.5f, 1.0f) && end.x > start.x + 20.0f);
    } else {
        delete source;
        delete target;
    }
    delete scene;
}

// A match and a cross-fade record that their objects hand over while they play.
// The pixel center of object at time along x, or -1 when it is not drawn.
static float _shownX(Renderer* renderer, Scene* scene, const Object* object, float time)
{
    BBox box;
    if (renderer->bounds(scene, object, time, box) != Result::Success) return -1.0f;
    return box.center().x;
}

void arrangedRows(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto box = [](float x) {
        auto shape = Rectangle::gen({x, 0.0f, 0.0f}, {1.0f, 1.0f});
        if (shape) shape->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        return shape;
    };

    // A box joins a row of five at the third place: the others slide apart and it grows in.
    auto scene = Scene::gen(config);
    Object* items[6] = {box(-2.5f), box(-1.25f), nullptr, box(0.0f), box(1.25f), box(2.5f)};
    CHECK(scene && items[0] && items[1] && items[3] && items[4] && items[5]);
    if (scene && items[0] && items[1] && items[3] && items[4] && items[5]) {
        for (auto i = 0u; i < 6u; i++) CHECK(i == 2u || scene->add(items[i]) == Result::Success);
        CHECK(scene->wait(0.5f) == Result::Success);
        items[2] = box(4.0f);
        CHECK(items[2] && scene->add(items[2]) == Result::Success);
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, 0.25f, {}, 1.0f, linear) == Result::Success);
        CHECK(near(scene->duration(), 1.5f));
        // Six boxes a quarter apart span 7.25 units, 30 pixels each, around the middle.
        float slots[] = {-3.125f, -1.875f, -0.625f, 0.625f, 1.875f, 3.125f};
        float starts[] = {-2.5f, -1.25f, -0.625f, 0.0f, 1.25f, 2.5f};
        for (auto i = 0u; i < 6u; i++) {
            CHECK(near(_shownX(renderer, scene, items[i], 1.5f), 160.0f + 30.0f * slots[i], 0.6f));
            if (i != 2u) CHECK(near(_shownX(renderer, scene, items[i], 0.5f), 160.0f + 30.0f * starts[i], 0.6f));
        }
        CHECK(near(_shownX(renderer, scene, items[0], 1.0f), 160.0f + 30.0f * -2.8125f, 0.6f));
        // The new box is unseen before and half grown halfway, in its own place.
        BBox grown;
        CHECK(renderer->bounds(scene, items[2], 0.25f, grown) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, items[2], 1.0f, grown) == Result::Success);
        CHECK(near(grown.width, 15.0f, 0.6f) && near(grown.center().x, 160.0f + 30.0f * -0.625f, 0.6f));
        // Arranged again in place, nothing moves and the call waits.
        auto clips = scene->clipCount();
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, 0.25f, {}, 0.5f, linear) == Result::Success);
        CHECK(scene->clipCount() == clips && near(scene->duration(), 2.0f));
        // A column runs down from its top item, centered where it is told.
        CHECK(scene->arrange(items, 3u, {0.0f, -1.0f, 0.0f}, 0.5f, {-4.0f, 0.0f, 0.0f}, 1.0f, linear)
              == Result::Success);
        BBox top;
        CHECK(renderer->bounds(scene, items[0], 3.0f, top) == Result::Success);
        CHECK(near(top.center().x, 160.0f - 120.0f, 0.6f) && near(top.center().y, 90.0f - 45.0f, 0.6f));
        // Refusals say what is wrong.
        auto group = Group::gen();
        auto stranger = box(0.0f);
        CHECK(group && stranger && group->add(stranger) == Result::Success && scene->add(group) == Result::Success);
        Object* mixed[] = {items[0], stranger};
        CHECK(scene->arrange(mixed, 2u) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "share one parent"));
        Object* twice[] = {items[0], items[0]};
        CHECK(scene->arrange(twice, 2u) == Result::InvalidArguments);
        CHECK(scene->arrange(items, 6u, {0.0f, 0.0f, 0.0f}) == Result::InvalidArguments);
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, -1.0f) == Result::InvalidArguments);
        auto empty = Group::gen();
        CHECK(empty && scene->add(empty) == Result::Success);
        Object* hollow[] = {items[0], empty};
        CHECK(scene->arrange(hollow, 2u) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "with bounds"));
    } else {
        for (auto item : items) delete item;
    }
    delete scene;
}

void handedOver(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto a = Text::gen("a", {0.0f, 0.0f, 0.0f});
    auto b = Text::gen("b", {0.0f, 0.0f, 0.0f});
    auto other = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(scene && from && to && a && b && other);
    if (!scene || !from || !to || !a || !b || !other) {
        delete scene;
        delete from;
        delete to;
        delete a;
        delete b;
        delete other;
        return;
    }
    CHECK(from->add(a) == Result::Success && to->add(b) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->add(other) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success && scene->add(to) == Result::Success);
    CHECK(!scene->handoff(a, b, 1.5f));
    auto unrelated = Text::gen("z", {1.0f, 0.0f, 0.0f});
    CHECK(unrelated && from->add(unrelated) == Result::Success);
    CHECK(scene->matchTransform(from, to, 1.0f) == Result::Success);
    CHECK(!scene->handoff(unrelated, b, 1.5f));
    // An unmatched fade at the same authored slot meets either way round.
    CHECK(scene->handoff(a, b, 1.5f) && scene->handoff(b, a, 1.5f) && scene->handoff(from, to, 1.0f));
    CHECK(!scene->handoff(a, b, 0.5f) && !scene->handoff(a, b, 2.5f));
    CHECK(!scene->handoff(a, other, 1.5f) && !scene->handoff(a, a, 1.5f) && !scene->handoff(nullptr, b, 1.5f));
    // A cross-fade hands over too.
    auto next = Circle::gen({2.0f, 1.0f, 0.0f}, 0.5f);
    CHECK(next && scene->add(next) == Result::Success);
    CHECK(scene->fadeTransform(other, next, 1.0f) == Result::Success);
    CHECK(scene->handoff(next, other, 2.5f) && !scene->handoff(next, other, 1.5f) && !scene->handoff(b, next, 2.5f));
    // The layout report gives a scene the time it shows.
    LayoutReport report;
    CHECK(renderer->layout(scene, 1.5f, report) == Result::Success);
    CHECK(report.sceneCount() == 1u && report.sceneAt(0) && near(report.sceneAt(0)->time, 1.5f));
    delete scene;
}

// A departing unpaired token yields its final slot to a moved token only while it fades.
// With copy, the original stays and is not a replacement slot.
void replacementContacts()
{
    for (auto copying : {false, true}) {
        auto scene = Scene::gen({});
        auto from = Group::gen();
        auto to = Group::gen();
        auto kept = Text::gen("a", {-1.0f, 0.0f, 0.0f});
        auto replaced = Text::gen("b", {1.0f, 0.0f, 0.0f});
        auto arriving = Text::gen("a", {1.0f, 0.0f, 0.0f});
        CHECK(scene && from && to && kept && replaced && arriving);
        if (!scene || !from || !to || !kept || !replaced || !arriving) {
            delete scene; delete from; delete to;
            delete kept; delete replaced; delete arriving;
            continue;
        }
        from->tag("slots");
        CHECK(std::strcmp(from->tag(), "slots") == 0);
        CHECK(from->add(kept) == Result::Success && from->add(replaced) == Result::Success);
        CHECK(to->add(arriving) == Result::Success);
        CHECK(scene->add(from) == Result::Success && scene->wait(0.13f) == Result::Success
              && scene->add(to) == Result::Success);
        MatchOptions options;
        options.copy = copying;
        options.fade = 0.4f;
        options.lag = 0.3f;
        CHECK(scene->matchTransform(from, to, 0.7f, {}, options) == Result::Success);
        CHECK(scene->handoff(kept, arriving, 0.4f));
        CHECK(scene->handoff(replaced, arriving, 0.4f) == !copying);
        CHECK(!scene->handoff(replaced, arriving, 0.7f));
        if (copying) {
            auto copy = scene->object("slots:copy");
            CHECK(copy && scene->handoff(copy->childAt(1), arriving, 0.4f));
            CHECK(copy && !scene->handoff(copy->childAt(1), arriving, 0.7f));
        }
        auto begin = static_cast<float>(static_cast<double>(0.13f) + static_cast<double>(0.7f) * 0.3f);
        auto end = begin + static_cast<float>(static_cast<double>(0.7f) * 0.4f);
        auto fading = copying ? scene->object("slots:copy")->childAt(1) : replaced;
        CHECK(!scene->handoff(fading, arriving, std::nextafter(begin, -1.0f)));
        CHECK(scene->handoff(fading, arriving, begin) && scene->handoff(fading, arriving, end));
        CHECK(!scene->handoff(fading, arriving, std::nextafter(end, 2.0f)));
        delete scene;
    }
}
