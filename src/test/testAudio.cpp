#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "vis.h"
#include "vis_audio.h"
#if defined(VIS_LUA_RUNTIME)
#include "vis_lua_runtime.h"
#endif
#include "testLuaLoad.h"

using namespace vis;
using namespace vis::audio;

static constexpr uint32_t SampleRate = 8000u;
static constexpr uint32_t SampleCount = 800u;
static constexpr uint32_t WavSize = 44u + SampleCount;

static void _word(uint8_t* output, uint16_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8u);
}

static void _dword(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8u);
    output[2] = static_cast<uint8_t>(value >> 16u);
    output[3] = static_cast<uint8_t>(value >> 24u);
}

static void _wav(uint8_t (&output)[WavSize])
{
    std::memcpy(output, "RIFF", 4u);
    _dword(output + 4u, WavSize - 8u);
    std::memcpy(output + 8u, "WAVEfmt ", 8u);
    _dword(output + 16u, 16u);
    _word(output + 20u, 1u);
    _word(output + 22u, 1u);
    _dword(output + 24u, SampleRate);
    _dword(output + 28u, SampleRate);
    _word(output + 32u, 1u);
    _word(output + 34u, 8u);
    std::memcpy(output + 36u, "data", 4u);
    _dword(output + 40u, SampleCount);
    for (auto i = 0u; i < SampleCount; i++) output[44u + i] = (i / 20u) % 2u ? 192u : 64u;
}

static void soundscape()
{
    CHECK(Soundscape::gen(nullptr) == nullptr);
    auto scene = Scene::gen();
    auto soundscape = Soundscape::gen(scene);
    CHECK(scene && soundscape);
    if (!scene || !soundscape) {
        delete scene;
        return;
    }
    CHECK(soundscape->type() == Type::Group);
    CHECK(soundscape->scene() == scene);
    CHECK(scene->object(soundscape->id()) == soundscape);
    CHECK(soundscape->count() == 0u && near(soundscape->duration(), 0.0f));

    Cue cue;
    CueConfig invalid;
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);
    invalid.asset = "";
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);
    invalid.asset = "tone.wav";
    invalid.bus = static_cast<Bus>(255u);
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);
    invalid.bus = Bus::Effect;
    invalid.begin = -1.0f;
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);
    invalid.begin = 2.0f;
    invalid.end = 1.0f;
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);
    invalid.end = 0.0f;
    invalid.gain = 4.01f;
    CHECK(soundscape->cue(invalid, cue) == Result::InvalidArguments && !cue);

    char musicName[] = "music.wav";
    CueConfig music;
    music.asset = musicName;
    music.bus = Bus::Music;
    music.begin = 0.0f;
    music.end = 4.0f;
    music.gain = 0.8f;
    music.loop = true;
    Cue musicCue;
    CHECK(soundscape->cue(music, musicCue) == Result::Success && musicCue);
    musicName[0] = 'x';

    CueConfig stored;
    CHECK(soundscape->cueAt(0u, stored));
    CHECK(std::strcmp(stored.asset, "music.wav") == 0);
    CHECK(stored.bus == Bus::Music && stored.loop);
    CHECK(!soundscape->cueAt(1u, stored));

    CueConfig effect;
    effect.asset = "effect.wav";
    effect.begin = 1.5f;
    effect.gain = 0.5f;
    Cue effectCue;
    CHECK(soundscape->cue(effect, effectCue) == Result::Success && effectCue);
    CHECK(soundscape->count() == 2u && near(soundscape->duration(), 4.0f));

    float value = 0.0f;
    CHECK(soundscape->sample(musicCue, 0.0f, value) == Result::Success);
    CHECK(near(value, 0.8f));
    CHECK(soundscape->gain(musicCue, 0.4f, 1.0f, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(soundscape->sample(musicCue, 1.0f, value) == Result::Success);
    CHECK(near(value, 0.8f));
    CHECK(soundscape->sample(musicCue, 2.0f, value) == Result::Success);
    CHECK(near(value, 0.6f));
    CHECK(soundscape->sample(musicCue, 3.0f, value) == Result::Success);
    CHECK(near(value, 0.4f));

    CHECK(soundscape->gain(musicCue, 0.7f, 2.0f, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);
    CHECK(soundscape->gain({}, 0.7f, 3.0f, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);
    CHECK(soundscape->gain(musicCue, 4.1f, 3.0f, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);
    CHECK(soundscape->gain(musicCue, 0.7f, 3.0f, 0.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);
    CHECK(soundscape->gain(musicCue, 0.7f,
                           std::numeric_limits<float>::quiet_NaN(), 1.0f,
                           AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);

    auto curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    CHECK(soundscape->gain(musicCue, 1.0f, 5.0f, 1.0f, curve) == Result::Success);
    CHECK(near(soundscape->duration(), 6.0f));
    CHECK(soundscape->sample(musicCue, 6.0f, value) == Result::Success);
    CHECK(near(value, 1.0f));
    CHECK(scene->duration() == 0.0f);
    delete scene;
}

static void player()
{
    PlayerConfig invalid;
    invalid.voiceLimit = 0u;
    invalid.device = false;
    CHECK(Player::gen(invalid) == nullptr);
    invalid.voiceLimit = Player::VoiceLimit + 1u;
    CHECK(Player::gen(invalid) == nullptr);

    PlayerConfig config;
    config.voiceLimit = 4u;
    config.device = false;
    auto player = Player::gen(config);
    CHECK(player != nullptr);
    if (!player) return;

    uint8_t wav[WavSize] = {};
    _wav(wav);
    CHECK(player->load(nullptr, wav, WavSize) == Result::InvalidArguments);
    CHECK(player->load("", wav, WavSize) == Result::InvalidArguments);
    CHECK(player->load("tone.wav", nullptr, WavSize) == Result::InvalidArguments);
    CHECK(player->load("tone.wav", wav, 0u) == Result::InvalidArguments);
    CHECK(player->load("tone.wav", wav, WavSize) == Result::Success);
    CHECK(player->load("tone.wav", wav, WavSize) == Result::InsufficientCondition);
    std::memset(wav, 0, sizeof(wav));

    CHECK(player->gain(0.75f) == Result::Success);
    CHECK(player->gain(Bus::Music, 0.6f) == Result::Success);
    CHECK(player->gain(Bus::Effect, 0.8f) == Result::Success);
    CHECK(player->gain(Bus::Ui, 0.9f) == Result::Success);
    CHECK(player->gain(-0.1f) == Result::InvalidArguments);
    CHECK(player->gain(static_cast<Bus>(255u), 1.0f) == Result::InvalidArguments);
    CHECK(player->start() == Result::Success);
    CHECK(player->start() == Result::Success);

    Playback effect;
    effect.asset = "tone.wav";
    effect.bus = Bus::Effect;
    effect.gain = 0.8f;
    Voice first;
    Voice second;
    CHECK(player->play(effect, &first) == Result::Success && first);
    CHECK(player->play(effect, &second) == Result::Success && second);
    CHECK(first.index != second.index);
    CHECK(player->active(first) && player->active(second));

    Playback music;
    music.asset = "tone.wav";
    music.bus = Bus::Music;
    music.gain = 0.6f;
    music.loop = true;
    Voice background;
    CHECK(player->play(music, &background) == Result::Success && background);
    CHECK(player->active(background));

    Playback ui = effect;
    ui.bus = Bus::Ui;
    Voice click;
    CHECK(player->play(ui, &click) == Result::Success && click);
    CHECK(player->play(effect) == Result::InsufficientCondition);

    CHECK(player->stop(second) == Result::Success);
    CHECK(!player->active(second));
    CHECK(player->stop(second) == Result::InsufficientCondition);
    Voice replacement;
    CHECK(player->play(effect, &replacement) == Result::Success && replacement);
    CHECK(replacement.index == second.index);
    CHECK(replacement.generation != second.generation);
    CHECK(player->stop(second) == Result::InsufficientCondition);
    CHECK(player->active(replacement));

    Playback bad = effect;
    bad.asset = "missing.wav";
    CHECK(player->play(bad) == Result::InvalidArguments);
    bad = effect;
    bad.rate = 0.124f;
    CHECK(player->play(bad) == Result::InvalidArguments);
    bad.rate = 8.01f;
    CHECK(player->play(bad) == Result::InvalidArguments);
    bad = effect;
    bad.gain = 4.01f;
    CHECK(player->play(bad) == Result::InvalidArguments);

    CHECK(player->stop(first) == Result::Success);
    CHECK(player->stop(background) == Result::Success);
    CHECK(player->stop(click) == Result::Success);
    CHECK(player->stop(replacement) == Result::Success);

    auto scene = Scene::gen();
    auto soundscape = Soundscape::gen(scene);
    CHECK(scene && soundscape);
    if (!scene || !soundscape) {
        delete scene;
        delete player;
        return;
    }
    CueConfig backgroundCue;
    backgroundCue.asset = "tone.wav";
    backgroundCue.bus = Bus::Music;
    backgroundCue.end = 10.0f;
    backgroundCue.loop = true;
    Cue cue;
    CHECK(soundscape->cue(backgroundCue, cue) == Result::Success);
    CueConfig effectCue;
    effectCue.asset = "tone.wav";
    effectCue.begin = 2.0f;
    effectCue.end = 3.0f;
    CHECK(soundscape->cue(effectCue, cue) == Result::Success);
    CHECK(player->bind(soundscape) == Result::Success);

    Transport transport;
    transport.seek = true;
    CHECK(player->sync(transport) == Result::Success);
    transport.time = 2.5f;
    transport.rate = 0.5f;
    transport.playing = true;
    CHECK(player->sync(transport) == Result::Success);
    transport.time = 4.0f;
    transport.rate = 2.0f;
    CHECK(player->sync(transport) == Result::Success);
    transport.time = 1.25f;
    transport.playing = false;
    CHECK(player->sync(transport) == Result::Success);
    transport.playing = true;
    CHECK(player->sync(transport) == Result::Success);

    transport.time = -1.0f;
    CHECK(player->sync(transport) == Result::InvalidArguments);
    transport.time = 1.0f;
    transport.rate = 0.0f;
    CHECK(player->sync(transport) == Result::InvalidArguments);
    transport.rate = 8.01f;
    CHECK(player->sync(transport) == Result::InvalidArguments);

    CueConfig late = effectCue;
    late.begin = 5.0f;
    late.end = 6.0f;
    CHECK(soundscape->cue(late, cue) == Result::Success);
    transport.rate = 1.0f;
    CHECK(player->sync(transport) == Result::InsufficientCondition);
    CHECK(player->bind(soundscape) == Result::Success);
    CHECK(player->sync(transport) == Result::Success);
    CHECK(player->bind(nullptr) == Result::Success);
    delete scene;
    delete player;
}

// The decoded level of frame index of the test tone, as miniaudio converts 8-bit samples.
static float _level(uint32_t index)
{
    auto value = (index % SampleCount / 20u) % 2u ? 192.0f : 64.0f;
    return value * 0.00784313725490196078f - 1.0f;
}

static bool _stereo(const float* output, uint32_t frame, float expected, float epsilon = 1.0e-5f)
{
    return near(output[frame * 2u], expected, epsilon)
        && near(output[frame * 2u + 1u], expected, epsilon);
}

static void mixing()
{
    PlayerConfig config;
    config.voiceLimit = 1u;
    config.device = false;
    auto player = Player::gen(config);
    auto scene = Scene::gen();
    auto soundscape = scene ? Soundscape::gen(scene) : nullptr;
    CHECK(player && scene && soundscape);
    if (!player || !scene || !soundscape) {
        delete scene;
        delete player;
        return;
    }
    uint8_t wav[WavSize] = {};
    _wav(wav);
    CHECK(player->load("tone.wav", wav, WavSize) == Result::Success);

    static constexpr uint32_t Frames = 2400u;
    static float whole[Frames * 2u];
    static float parts[Frames * 2u];
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::InsufficientCondition);

    // The 0.1 s tone plays once from 0.05 s at half gain, and loops on the music bus from
    // 0.1 s to 0.275 s, so the two overlap from 0.1 s until the first one ends at 0.15 s.
    CueConfig once;
    once.asset = "tone.wav";
    once.begin = 0.05f;
    once.gain = 0.5f;
    Cue onceCue;
    CHECK(soundscape->cue(once, onceCue) == Result::Success);
    CueConfig loop;
    loop.asset = "tone.wav";
    loop.bus = Bus::Music;
    loop.begin = 0.1f;
    loop.end = 0.275f;
    loop.loop = true;
    Cue loopCue;
    CHECK(soundscape->cue(loop, loopCue) == Result::Success);
    CHECK(player->gain(Bus::Music, 0.5f) == Result::Success);
    CHECK(player->bind(soundscape) == Result::Success);

    CHECK(player->mix(SampleRate - 1u, 0u, whole, Frames) == Result::InvalidArguments);
    CHECK(player->mix(Player::MixRateMax + 1u, 0u, whole, Frames) == Result::InvalidArguments);
    CHECK(player->mix(SampleRate, 0u, nullptr, Frames) == Result::InvalidArguments);
    CHECK(player->mix(SampleRate, 0u, nullptr, 0u) == Result::Success);

    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::Success);
    auto exact = true;
    for (auto frame = 0u; frame < Frames; frame++) {
        auto expected = 0.0f;
        if (frame >= 400u && frame < 1200u) expected += 0.5f * _level(frame - 400u);
        if (frame >= 800u && frame < 2200u) expected += 0.5f * _level(frame - 800u);
        if (!_stereo(whole, frame, expected)) exact = false;
    }
    CHECK(exact);
    CHECK(_stereo(whole, 399u, 0.0f) && _stereo(whole, 2200u, 0.0f));

    // Mixing in uneven blocks, or out of order, gives the same samples.
    for (auto first = 0u; first < Frames; first += 333u) {
        auto count = Frames - first < 333u ? Frames - first : 333u;
        CHECK(player->mix(SampleRate, first, parts + first * 2u, count) == Result::Success);
    }
    CHECK(std::memcmp(whole, parts, sizeof(whole)) == 0);
    CHECK(player->mix(SampleRate, 1000u, parts, 10u) == Result::Success);
    CHECK(std::memcmp(whole + 2000u, parts, 20u * sizeof(float)) == 0);
    CHECK(player->mix(SampleRate, 5u, parts, 10u) == Result::Success);
    CHECK(std::memcmp(whole + 10u, parts, 20u * sizeof(float)) == 0);

    // A gain clip fades the single cue out between 0.075 s and 0.1 s. Gains are joined
    // linearly every 32 frames, so they are exact outside the fade and close inside it.
    CHECK(soundscape->gain(onceCue, 0.0f, 0.075f, 0.025f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(player->gain(2.0f) == Result::Success);
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::Success);
    CHECK(_stereo(whole, 576u, 1.0f * _level(176u)));
    CHECK(_stereo(whole, 700u, 0.5f * _level(300u), 0.01f));
    CHECK(_stereo(whole, 832u, 1.0f * _level(32u)));
    CHECK(_stereo(whole, 1600u, 1.0f * _level(800u)));

    // The mix is clamped, and every cue's asset must be loaded.
    CHECK(player->gain(4.0f) == Result::Success);
    CHECK(player->gain(Bus::Music, 1.0f) == Result::Success);
    CHECK(player->mix(SampleRate, 1600u, parts, 40u) == Result::Success);
    CHECK(_stereo(parts, 0u, -1.0f) && _stereo(parts, 20u, 1.0f));
    CHECK(player->gain(1.0f) == Result::Success);
    CHECK(player->gain(Bus::Music, 0.5f) == Result::Success);
    CueConfig missing;
    missing.asset = "missing.wav";
    missing.begin = 0.25f;
    CHECK(soundscape->cue(missing, onceCue) == Result::Success);
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::InsufficientCondition);
    CHECK(player->bind(soundscape) == Result::Success);
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::InsufficientCondition);
    CHECK(player->load("missing.wav", wav, WavSize) == Result::Success);
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::Success);
    CHECK(_stereo(whole, 2100u, 0.5f * _level(1300u) + _level(100u)));
    delete scene;

    // At 48 kHz the tone is resampled and still lasts 0.1 s.
    scene = Scene::gen();
    soundscape = scene ? Soundscape::gen(scene) : nullptr;
    CHECK(scene && soundscape);
    if (scene && soundscape) {
        CHECK(soundscape->cue(once, onceCue) == Result::Success);
        CHECK(player->bind(soundscape) == Result::Success);
        static float resampled[9600u * 2u];
        CHECK(player->mix(48000u, 0u, resampled, 9600u) == Result::Success);
        auto peak = 0.0f;
        auto silent = true;
        for (auto frame = 0u; frame < 9600u; frame++) {
            auto value = std::fabs(resampled[frame * 2u]);
            if (frame >= 2600u && frame < 7000u && value > peak) peak = value;
            if ((frame < 2400u || frame >= 7200u) && value != 0.0f) silent = false;
            if (resampled[frame * 2u] != resampled[frame * 2u + 1u]) silent = false;
        }
        CHECK(peak > 0.2f && peak < 0.3f);
        CHECK(silent);
    }
    CHECK(player->bind(nullptr) == Result::Success);
    CHECK(player->mix(SampleRate, 0u, whole, Frames) == Result::InsufficientCondition);
    delete scene;
    delete player;
}

static void lua()
{
    static constexpr const char* source = R"(
local scene = vis.scene {width = 32, height = 32}
local music = vis.audio.cue(scene, {
    asset = "music.wav", bus = "music", begin = 0, ["end"] = 4,
    gain = 0.8, loop = true,
})
music:gain(0.2, 1, {duration = 1, curve = "linear"})
return scene
)";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "audio.lua", context);
    auto soundscape = scene ? context.audio.soundscape : nullptr;
    CHECK(soundscape && soundscape->scene() == scene);
    if (soundscape) {
        CHECK(soundscape->count() == 1u);
        CueConfig cue;
        CHECK(soundscape->cueAt(0u, cue));
        CHECK(cue.bus == Bus::Music && cue.loop && near(cue.end, 4.0f));
        auto value = 0.0f;
        CHECK(soundscape->sample({0u}, 1.5f, value) == Result::Success);
        CHECK(near(value, 0.5f));
    }
    delete scene;

    static constexpr const char* invalid = R"(
local scene = vis.scene {}
vis.audio.cue(scene, {asset = "bad.wav", bus = "voice"})
return scene
)";
    _scriptError(invalid, "invalid-audio.lua", "audio bus");
}

// An asset name out of range stops the script with the limit, at the line of the cue.
static void luaAsset()
{
    static constexpr const char* source = R"(
local scene = vis.scene {}
vis.audio.cue(scene, {asset = ""})
return scene
)";
    _scriptError(source, "audio-asset.lua",
                 "audio-asset.lua:3: audio asset must be 1 to 255 bytes without zero bytes");
}

#if defined(VIS_LUA_RUNTIME)
static void retainedLua()
{
    static constexpr const char* source = R"(
local scene = vis.scene {width = 32, height = 32}
local cue = vis.audio.cue(scene, {asset = "tone.wav"})
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        cue:gain(0.5, time, 0.1, "linear")
    end,
})
return scene
)";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "audio-runtime.lua", context);
    CHECK(context.audio.soundscape && context.runtime.runtime);
    if (scene && context.runtime.runtime) {
        CHECK(context.runtime.runtime->advance(0.11f) == Result::ScriptError);
        CHECK(context.runtime.runtime->failed());
        CHECK(std::strstr(context.runtime.runtime->error(),
                          "audio authoring is unavailable after loading") != nullptr);
    }
    delete scene;
}
#endif

int main()
{
    soundscape();
    player();
    mixing();
    lua();
    luaAsset();
#if defined(VIS_LUA_RUNTIME)
    retainedLua();
#endif
    return report();
}
