// The audio module's player: decodes cue assets with miniaudio, plays the buses in step with
// scene time, and mixes them into buffers on request.
#include <cmath>
#include <cstring>
#include <new>

// Browser playback uses miniaudio's main-thread Web Audio ScriptProcessor backend;
// long main-thread work can underrun audio.
#include <miniaudio.h>

#include "vis_audio.h"
#include "../common/visString.h"

namespace vis::audio
{

namespace
{

static bool _bus(Bus bus) noexcept
{
    return static_cast<uint8_t>(bus) <= static_cast<uint8_t>(Bus::Ui);
}

static bool _gain(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f && value <= 4.0f;
}

static bool _rate(float value) noexcept
{
    return std::isfinite(value) && value >= 0.125f && value <= 8.0f;
}

// The frame nearest to Scene time time, at rate frames per second.
static uint64_t _frame(float time, uint32_t rate) noexcept
{
    auto value = static_cast<double>(time) * rate;
    if (value >= 9.0e18) return UINT64_MAX;
    return static_cast<uint64_t>(std::llround(value));
}

static Result _result(ma_result value) noexcept
{
    switch (value) {
        case MA_SUCCESS: return Result::Success;
        case MA_INVALID_ARGS:
        case MA_OUT_OF_RANGE:
        case MA_TOO_BIG:
        case MA_NAME_TOO_LONG: return Result::InvalidArguments;
        case MA_INVALID_OPERATION:
        case MA_BUSY:
        case MA_UNAVAILABLE:
        case MA_ALREADY_IN_USE: return Result::InsufficientCondition;
        case MA_OUT_OF_MEMORY: return Result::OutOfMemory;
        case MA_DOES_NOT_EXIST:
        case MA_INVALID_FILE:
        case MA_BAD_SEEK:
        case MA_IO_ERROR: return Result::IoError;
        case MA_FORMAT_NOT_SUPPORTED:
        case MA_DEVICE_TYPE_NOT_SUPPORTED:
        case MA_NO_BACKEND:
        case MA_NO_DEVICE:
        case MA_API_NOT_FOUND:
        case MA_BACKEND_NOT_ENABLED:
        case MA_NOT_IMPLEMENTED: return Result::NonSupport;
        default: return Result::Unknown;
    }
}

}  // namespace

struct Player::Impl
{
    struct Asset
    {
        char* name = nullptr;
        uint8_t* data = nullptr;
        uint32_t size = 0u;
        bool registered = false;
    };

    struct VoiceSlot
    {
        ma_sound sound = {};
        uint32_t generation = 0u;
        bool initialized = false;
        bool loop = false;
    };

    struct TrackSlot
    {
        ma_sound sound = {};
        float length = 0.0f;
        bool initialized = false;
    };

    // Decodes one cue's asset at the mix rate for mix().
    struct MixSlot
    {
        ma_decoder decoder = {};
        // Asset frames at the mix rate.
        ma_uint64 length = 0u;
        // The asset frame the decoder reads next.
        ma_uint64 cursor = 0u;
        bool initialized = false;
    };

    static constexpr ma_uint32 MixBlock = 256u;
    static constexpr uint64_t GainStep = 32u;

    ~Impl()
    {
        clearMixes();
        for (auto i = 0u; i < assetCount; i++) {
            delete[] assets[i].name;
            delete[] assets[i].data;
        }
        delete[] voices;
        delete[] tracks;
    }

    Asset* asset(const char* name) noexcept
    {
        for (auto i = 0u; i < assetCount; i++) {
            if (std::strcmp(assets[i].name, name) == 0) return assets + i;
        }
        return nullptr;
    }

    ma_sound_group* group(Bus bus) noexcept
    {
        return groups + static_cast<uint8_t>(bus);
    }

    void clearTracks() noexcept
    {
        for (auto i = 0u; i < trackCount; i++) {
            if (!tracks[i].initialized) continue;
            ma_sound_stop(&tracks[i].sound);
            ma_sound_uninit(&tracks[i].sound);
            tracks[i] = {};
        }
    }

    void clearMixes() noexcept
    {
        for (auto i = 0u; i < mixCount; i++) {
            if (mixes[i].initialized) ma_decoder_uninit(&mixes[i].decoder);
        }
        delete[] mixes;
        mixes = nullptr;
        mixCount = 0u;
        mixRate = 0u;
    }

    void clearVoices() noexcept
    {
        for (auto i = 0u; i < config.voiceLimit; i++) {
            if (!voices[i].initialized) continue;
            ma_sound_stop(&voices[i].sound);
            ma_sound_uninit(&voices[i].sound);
            voices[i].initialized = false;
        }
    }

    void collect() noexcept
    {
        for (auto i = 0u; i < config.voiceLimit; i++) {
            auto& slot = voices[i];
            if (!slot.initialized || slot.loop || !ma_sound_at_end(&slot.sound)) continue;
            ma_sound_uninit(&slot.sound);
            slot.initialized = false;
        }
    }

    void uninit() noexcept
    {
        if (!initialized) return;
        clearTracks();
        clearVoices();
        auto manager = ma_engine_get_resource_manager(&engine);
        for (auto i = 0u; i < assetCount; i++) {
            if (!assets[i].registered) continue;
            ma_resource_manager_unregister_data(manager, assets[i].name);
            assets[i].registered = false;
        }
        for (auto i = groupCount; i > 0u; i--) ma_sound_group_uninit(groups + i - 1u);
        ma_engine_uninit(&engine);
        groupCount = 0u;
        initialized = false;
        started = false;
        transportValid = false;
    }

    Result init() noexcept
    {
        if (initialized) return Result::Success;
        auto engineConfig = ma_engine_config_init();
        engineConfig.sampleRate = 48000u;
        engineConfig.noAutoStart = MA_TRUE;
        engineConfig.noDevice = config.device ? MA_FALSE : MA_TRUE;
        if (!config.device) engineConfig.channels = 2u;
        engineConfig.defaultVolumeSmoothTimeInPCMFrames = 128u;
        auto status = ma_engine_init(&engineConfig, &engine);
        if (status != MA_SUCCESS) return _result(status);

        // From here on uninit() undoes as much of the setup as has run.
        initialized = true;
        auto fail = [this](ma_result failure) {
            uninit();
            return _result(failure);
        };
        for (auto i = 0u; i < 3u; i++) {
            status = ma_sound_group_init(&engine, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                         nullptr, groups + i);
            if (status != MA_SUCCESS) return fail(status);
            groupCount++;
            ma_sound_group_set_volume(groups + i, busGain[i]);
        }
        status = ma_engine_set_volume(&engine, masterGain);
        if (status != MA_SUCCESS) return fail(status);

        auto manager = ma_engine_get_resource_manager(&engine);
        for (auto i = 0u; i < assetCount; i++) {
            status = ma_resource_manager_register_encoded_data(
                manager, assets[i].name, assets[i].data, assets[i].size);
            if (status != MA_SUCCESS) return fail(status);
            assets[i].registered = true;
        }
        return Result::Success;
    }

    // Readies sound to play playback; the caller starts it.
    Result init(ma_sound& sound, const Playback& playback) noexcept
    {
        auto flags = static_cast<ma_uint32>(MA_SOUND_FLAG_NO_SPATIALIZATION);
        if (playback.bus != Bus::Music) flags |= MA_SOUND_FLAG_DECODE;
        if (playback.loop) flags |= MA_SOUND_FLAG_LOOPING;
        auto status = ma_sound_init_from_file(&engine, playback.asset, flags,
                                              group(playback.bus), nullptr, &sound);
        if (status != MA_SUCCESS) return _result(status);
        ma_sound_set_volume(&sound, playback.gain);
        ma_sound_set_pitch(&sound, playback.rate);
        return Result::Success;
    }

    Result init(TrackSlot& slot, const CueConfig& cue) noexcept
    {
        if (slot.initialized) return Result::Success;
        if (!asset(cue.asset)) return Result::InsufficientCondition;
        auto result = init(slot.sound, {cue.asset, cue.bus, cue.gain, 1.0f, cue.loop});
        if (result != Result::Success) return result;
        slot.initialized = true;
        if (ma_sound_get_length_in_seconds(&slot.sound, &slot.length) != MA_SUCCESS) {
            slot.length = 0.0f;
        }
        return Result::Success;
    }

    Result seek(MixSlot& slot, ma_uint64 frame) noexcept
    {
        if (slot.cursor == frame) return Result::Success;
        auto status = ma_decoder_seek_to_pcm_frame(&slot.decoder, frame);
        if (status != MA_SUCCESS) return _result(status);
        slot.cursor = frame;
        return Result::Success;
    }

    Result init(MixSlot& slot, const Asset& source) noexcept
    {
        if (slot.initialized) return Result::Success;
        auto decoderConfig = ma_decoder_config_init(ma_format_f32, 2u, mixRate);
        auto status = ma_decoder_init_memory(source.data, source.size, &decoderConfig, &slot.decoder);
        if (status != MA_SUCCESS) return _result(status);
        slot.initialized = true;
        slot.cursor = 0u;
        if (ma_decoder_get_length_in_pcm_frames(&slot.decoder, &slot.length) == MA_SUCCESS
            && slot.length) {
            return Result::Success;
        }
        // Formats that cannot report a length are counted by decoding them once.
        float scratch[2u * MixBlock];
        slot.length = 0u;
        while (true) {
            ma_uint64 read = 0u;
            status = ma_decoder_read_pcm_frames(&slot.decoder, scratch, MixBlock, &read);
            slot.length += read;
            if (!read || status != MA_SUCCESS) break;
        }
        slot.cursor = slot.length;
        return seek(slot, 0u);
    }

    // The gain of cue at frame of the mix.
    float gain(Cue cue, uint64_t frame) const noexcept
    {
        auto value = 0.0f;
        soundscape->sample(cue, static_cast<float>(static_cast<double>(frame) / mixRate), value);
        return value;
    }

    // Adds what cue plays over frames frames from frame first to output.
    Result mix(MixSlot& slot, const CueConfig& cue, Cue handle, uint64_t first,
               float* output, uint32_t frames) noexcept
    {
        auto start = _frame(cue.begin, mixRate);
        auto stop = cue.end > 0.0f ? _frame(cue.end, mixRate) : UINT64_MAX;
        auto last = first + frames;
        if (last <= start || first >= stop) return Result::Success;
        auto result = init(slot, *asset(cue.asset));
        if (result != Result::Success) return result;
        if (!slot.length) return Result::Success;
        if (!cue.loop && slot.length < stop - start) stop = start + slot.length;
        auto frame = first > start ? first : start;
        if (last > stop) last = stop;
        if (frame >= last) return Result::Success;
        result = seek(slot, cue.loop ? (frame - start) % slot.length : frame - start);
        if (result != Result::Success) return result;

        auto scale = busGain[static_cast<uint8_t>(cue.bus)] * masterGain;
        float block[2u * MixBlock];
        while (frame < last) {
            if (slot.cursor >= slot.length) {
                if (!cue.loop) break;
                result = seek(slot, 0u);
                if (result != Result::Success) return result;
            }
            auto want = last - frame;
            if (want > MixBlock) want = MixBlock;
            if (want > slot.length - slot.cursor) want = slot.length - slot.cursor;
            ma_uint64 read = 0u;
            auto status = ma_decoder_read_pcm_frames(&slot.decoder, block, want, &read);
            if (status != MA_SUCCESS && status != MA_AT_END) return _result(status);
            if (!read) {
                // The asset ended sooner than its reported length.
                if (!cue.loop || !slot.cursor) break;
                slot.length = slot.cursor;
                continue;
            }
            slot.cursor += read;
            // Gain clips are sampled every GainStep frames of the timeline and joined
            // linearly, so the result does not depend on how the mix is split into calls.
            auto end = frame + read;
            auto samples = block;
            auto target = output + (frame - first) * 2u;
            while (frame < end) {
                auto segment = frame - frame % GainStep;
                auto head = gain(handle, segment);
                auto tail = gain(handle, segment + GainStep);
                auto segmentEnd = segment + GainStep < end ? segment + GainStep : end;
                for (; frame < segmentEnd; frame++, samples += 2u, target += 2u) {
                    auto progress = static_cast<float>(frame - segment) / static_cast<float>(GainStep);
                    auto value = (head + (tail - head) * progress) * scale;
                    target[0] += samples[0] * value;
                    target[1] += samples[1] * value;
                }
            }
        }
        return Result::Success;
    }

    PlayerConfig config;
    ma_engine engine = {};
    ma_sound_group groups[3] = {};
    Asset assets[AssetLimit];
    VoiceSlot* voices = nullptr;
    TrackSlot* tracks = nullptr;
    MixSlot* mixes = nullptr;
    const Soundscape* soundscape = nullptr;
    uint32_t assetCount = 0u;
    uint32_t assetBytes = 0u;
    uint32_t trackCount = 0u;
    uint32_t mixCount = 0u;
    uint32_t mixRate = 0u;
    uint32_t groupCount = 0u;
    float masterGain = 1.0f;
    float busGain[3] = {1.0f, 1.0f, 1.0f};
    // The transport sync() last applied, when transportValid.
    Transport transport;
    bool initialized = false;
    bool started = false;
    bool transportValid = false;
};

Player::Player(const PlayerConfig& config) noexcept : pImpl(new (std::nothrow) Impl)
{
    if (!pImpl) return;
    pImpl->config = config;
    pImpl->voices = new (std::nothrow) Impl::VoiceSlot[config.voiceLimit];
}

Player::~Player()
{
    if (pImpl) pImpl->uninit();
    delete pImpl;
}

Player* Player::gen(const PlayerConfig& config) noexcept
{
    if (!config.voiceLimit || config.voiceLimit > VoiceLimit) return nullptr;
    auto player = new (std::nothrow) Player(config);
    if (!player || !player->pImpl || !player->pImpl->voices) {
        delete player;
        return nullptr;
    }
    return player;
}

Result Player::load(const char* name, const uint8_t* data, uint32_t size) noexcept
{
    if (!name || !data || !size || size > AssetByteLimit) {
        return Result::InvalidArguments;
    }
    auto nameSize = std::strlen(name);
    if (!nameSize || nameSize > Soundscape::AssetNameLimit) return Result::InvalidArguments;
    if (pImpl->asset(name) || pImpl->assetCount >= AssetLimit
        || size > AssetTotalByteLimit - pImpl->assetBytes) {
        return Result::InsufficientCondition;
    }

    auto copiedName = _duplicate(name);
    auto copiedData = new (std::nothrow) uint8_t[size];
    if (!copiedName || !copiedData) {
        delete[] copiedName;
        delete[] copiedData;
        return Result::OutOfMemory;
    }
    std::memcpy(copiedData, data, size);

    auto& asset = pImpl->assets[pImpl->assetCount];
    asset.name = copiedName;
    asset.data = copiedData;
    asset.size = size;
    if (pImpl->initialized) {
        auto status = ma_resource_manager_register_encoded_data(
            ma_engine_get_resource_manager(&pImpl->engine), asset.name, asset.data, asset.size);
        if (status != MA_SUCCESS) {
            delete[] asset.name;
            delete[] asset.data;
            asset = {};
            return _result(status);
        }
        asset.registered = true;
    }
    pImpl->assetCount++;
    pImpl->assetBytes += size;
    return Result::Success;
}

Result Player::start() noexcept
{
    auto result = pImpl->init();
    if (result != Result::Success || pImpl->started) return result;
    if (pImpl->config.device) {
        auto status = ma_engine_start(&pImpl->engine);
        if (status != MA_SUCCESS) return _result(status);
    }
    pImpl->started = true;
    return Result::Success;
}

Result Player::play(const Playback& playback, Voice* output) noexcept
{
    if (output) *output = {};
    if (!playback.asset || !_bus(playback.bus)
        || !_gain(playback.gain) || !_rate(playback.rate)) {
        return Result::InvalidArguments;
    }
    auto result = pImpl->init();
    if (result != Result::Success) return result;
    if (!pImpl->asset(playback.asset)) return Result::InvalidArguments;
    pImpl->collect();

    auto index = 0u;
    while (index < pImpl->config.voiceLimit && pImpl->voices[index].initialized) index++;
    if (index == pImpl->config.voiceLimit) return Result::InsufficientCondition;

    auto& slot = pImpl->voices[index];
    result = pImpl->init(slot.sound, playback);
    if (result != Result::Success) return result;
    auto status = ma_sound_start(&slot.sound);
    if (status != MA_SUCCESS) {
        ma_sound_uninit(&slot.sound);
        return _result(status);
    }
    slot.initialized = true;
    slot.loop = playback.loop;
    slot.generation++;
    if (!slot.generation) slot.generation = 1u;
    if (output) *output = {index, slot.generation};
    return Result::Success;
}

Result Player::stop(Voice voice) noexcept
{
    if (voice.index >= pImpl->config.voiceLimit) return Result::InvalidArguments;
    auto& slot = pImpl->voices[voice.index];
    if (!slot.initialized || slot.generation != voice.generation) {
        return Result::InsufficientCondition;
    }
    ma_sound_stop(&slot.sound);
    ma_sound_uninit(&slot.sound);
    slot.initialized = false;
    return Result::Success;
}

bool Player::active(Voice voice) const noexcept
{
    if (voice.index >= pImpl->config.voiceLimit) return false;
    auto& slot = pImpl->voices[voice.index];
    return slot.initialized && slot.generation == voice.generation
        && ma_sound_is_playing(&slot.sound);
}

Result Player::gain(float value) noexcept
{
    if (!_gain(value)) return Result::InvalidArguments;
    pImpl->masterGain = value;
    if (!pImpl->initialized) return Result::Success;
    return _result(ma_engine_set_volume(&pImpl->engine, value));
}

Result Player::gain(Bus bus, float value) noexcept
{
    if (!_bus(bus) || !_gain(value)) return Result::InvalidArguments;
    pImpl->busGain[static_cast<uint8_t>(bus)] = value;
    if (pImpl->initialized) ma_sound_group_set_volume(pImpl->group(bus), value);
    return Result::Success;
}

Result Player::bind(const Soundscape* soundscape) noexcept
{
    auto count = soundscape ? soundscape->count() : 0u;
    auto tracks = count ? new (std::nothrow) Impl::TrackSlot[count] : nullptr;
    if (count && !tracks) return Result::OutOfMemory;
    pImpl->clearTracks();
    pImpl->clearMixes();
    delete[] pImpl->tracks;
    pImpl->tracks = tracks;
    pImpl->trackCount = count;
    pImpl->soundscape = soundscape;
    pImpl->transportValid = false;
    return Result::Success;
}

Result Player::sync(const Transport& transport) noexcept
{
    if (!std::isfinite(transport.time) || transport.time < 0.0f
        || !_rate(transport.rate)) {
        return Result::InvalidArguments;
    }
    // bind() forgets the transport, so an unbound player need not keep it.
    if (!pImpl->soundscape) return Result::Success;
    auto result = pImpl->init();
    if (result != Result::Success) return result;
    if (pImpl->trackCount != pImpl->soundscape->count()) {
        return Result::InsufficientCondition;
    }

    auto& last = pImpl->transport;
    auto hard = transport.seek || !pImpl->transportValid || transport.time < last.time
             || transport.rate != last.rate || transport.playing != last.playing;
    for (auto i = 0u; i < pImpl->trackCount; i++) {
        CueConfig cue;
        pImpl->soundscape->cueAt(i, cue);
        auto& slot = pImpl->tracks[i];
        result = pImpl->init(slot, cue);
        if (result != Result::Success) return result;

        auto local = transport.time - cue.begin;
        auto active = local >= 0.0f && (cue.end == 0.0f || transport.time < cue.end);
        if (active && !cue.loop && slot.length > 0.0f && local >= slot.length) active = false;
        auto offset = std::fmax(0.0f, local);
        if (cue.loop && slot.length > 0.0f) offset = std::fmod(offset, slot.length);

        float value;
        pImpl->soundscape->sample({i}, transport.time, value);
        ma_sound_set_volume(&slot.sound, value);
        ma_sound_set_pitch(&slot.sound, transport.rate);

        if (!active) {
            if (ma_sound_is_playing(&slot.sound)) ma_sound_stop(&slot.sound);
            if (hard && local < 0.0f) ma_sound_seek_to_second(&slot.sound, 0.0f);
            continue;
        }

        float cursor = 0.0f;
        auto cursorResult = ma_sound_get_cursor_in_seconds(&slot.sound, &cursor);
        auto seek = hard || cursorResult != MA_SUCCESS || std::fabs(cursor - offset) > 0.08f;
        if (seek) {
            auto status = ma_sound_seek_to_second(&slot.sound, offset);
            if (status != MA_SUCCESS) return _result(status);
        }
        if (!transport.playing) {
            if (ma_sound_is_playing(&slot.sound)) ma_sound_stop(&slot.sound);
            continue;
        }
        if (!ma_sound_is_playing(&slot.sound)) {
            auto status = ma_sound_start(&slot.sound);
            if (status != MA_SUCCESS) return _result(status);
        }
    }
    last = transport;
    pImpl->transportValid = true;
    return Result::Success;
}

Result Player::mix(uint32_t rate, uint64_t first, float* output, uint32_t frames) noexcept
{
    if (rate < MixRateMin || rate > MixRateMax || (frames && !output)
        || frames > UINT32_MAX / 2u || first > UINT64_MAX - frames) {
        return Result::InvalidArguments;
    }
    auto soundscape = pImpl->soundscape;
    if (!soundscape || pImpl->trackCount != soundscape->count()) {
        return Result::InsufficientCondition;
    }
    auto count = soundscape->count();
    for (auto i = 0u; i < count; i++) {
        CueConfig cue;
        soundscape->cueAt(i, cue);
        if (!pImpl->asset(cue.asset)) return Result::InsufficientCondition;
    }
    if (pImpl->mixRate != rate || pImpl->mixCount != count) {
        pImpl->clearMixes();
        if (count) {
            pImpl->mixes = new (std::nothrow) Impl::MixSlot[count];
            if (!pImpl->mixes) return Result::OutOfMemory;
        }
        pImpl->mixCount = count;
        pImpl->mixRate = rate;
    }
    for (auto i = 0u; i < frames * 2u; i++) output[i] = 0.0f;
    for (auto i = 0u; i < count; i++) {
        CueConfig cue;
        soundscape->cueAt(i, cue);
        auto result = pImpl->mix(pImpl->mixes[i], cue, {i}, first, output, frames);
        if (result != Result::Success) return result;
    }
    for (auto i = 0u; i < frames * 2u; i++) output[i] = std::fmax(-1.0f, std::fmin(1.0f, output[i]));
    return Result::Success;
}

}  // namespace vis::audio
