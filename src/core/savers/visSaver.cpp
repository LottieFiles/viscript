// Saver: PNG written by hand, and frames piped to ffmpeg for GIF and MP4.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <signal.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <fcntl.h>
#else
#include <cerrno>
#include <pthread.h>
#include <time.h>
#endif
#endif

#include "vis.h"
#include "visSaver.h"

namespace vis
{

static uint32_t _crc(uint32_t crc, const uint8_t* data, size_t size)
{
    crc = ~crc;
    for (auto i = 0u; i < size; i++) {
        crc ^= data[i];
        for (auto bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static uint32_t _adler(const uint8_t* data, size_t size)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (auto i = 0u; i < size; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static void _be32(uint8_t* data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

static bool _write(FILE* file, const void* data, size_t size)
{
    return std::fwrite(data, 1, size, file) == size;
}

static bool _chunk(FILE* file, const char* type, const uint8_t* data, uint32_t size)
{
    uint8_t length[4];
    _be32(length, size);
    if (!_write(file, length, sizeof(length)) || !_write(file, type, 4)) return false;
    if (size && !_write(file, data, size)) return false;
    auto crc = _crc(0, reinterpret_cast<const uint8_t*>(type), 4);
    crc = _crc(crc, data, size);
    uint8_t checksum[4];
    _be32(checksum, crc);
    return _write(file, checksum, sizeof(checksum));
}

static uint8_t* _rgba(const Surface& surface, bool filters, size_t& size)
{
    auto limit = std::numeric_limits<size_t>::max();
    if (surface.width() > limit / 4u) return nullptr;
    auto row = static_cast<size_t>(surface.width()) * 4;
    auto stride = row + (filters ? 1u : 0u);
    if (stride < row || surface.height() > limit / stride) return nullptr;
    size = stride * surface.height();
    auto output = new (std::nothrow) uint8_t[size];
    if (!output) return nullptr;
    size_t offset = 0;
    for (auto y = 0u; y < surface.height(); y++) {
        if (filters) output[offset++] = 0;
        for (auto x = 0u; x < surface.width(); x++) {
            auto pixel = surface.data()[static_cast<size_t>(y) * surface.stride() + x];
            output[offset++] = static_cast<uint8_t>(pixel);
            output[offset++] = static_cast<uint8_t>(pixel >> 8);
            output[offset++] = static_cast<uint8_t>(pixel >> 16);
            output[offset++] = static_cast<uint8_t>(pixel >> 24);
        }
    }
    return output;
}

static uint8_t _paeth(int left, int up, int corner)
{
    auto a = std::abs(up - corner);
    auto b = std::abs(left - corner);
    auto c = std::abs(left + up - 2 * corner);
    return static_cast<uint8_t>(a <= b && a <= c ? left : b <= c ? up : corner);
}

static void _filter(uint8_t* raw, uint32_t width, uint32_t height)
{
    auto row = static_cast<size_t>(width) * 4;
    auto stride = row + 1;
    // Work backwards so the left pixel and the previous row still contain RGBA.
    for (auto y = height; y-- > 0;) {
        auto data = raw + static_cast<size_t>(y) * stride + 1;
        auto previous = y ? data - stride : nullptr;
        uint64_t scores[5] = {};
        for (size_t x = 0; x < row; x++) {
            auto left = x >= 4 ? data[x - 4] : 0;
            auto up = previous ? previous[x] : 0;
            auto corner = previous && x >= 4 ? previous[x - 4] : 0;
            uint8_t values[] = {data[x], static_cast<uint8_t>(data[x] - left),
                                static_cast<uint8_t>(data[x] - up),
                                static_cast<uint8_t>(data[x] - (left + up) / 2),
                                static_cast<uint8_t>(data[x] - _paeth(left, up, corner))};
            for (auto filter = 0u; filter < 5; filter++)
                scores[filter] += values[filter] < 128 ? values[filter] : 256 - values[filter];
        }
        auto filter = 0u;
        for (auto i = 1u; i < 5; i++) {
            if (scores[i] < scores[filter]) filter = i;
        }
        data[-1] = static_cast<uint8_t>(filter);
        for (auto x = row; x-- > 0;) {
            auto left = x >= 4 ? data[x - 4] : 0;
            auto up = previous ? previous[x] : 0;
            auto corner = previous && x >= 4 ? previous[x - 4] : 0;
            auto predictor = filter == 1 ? left : filter == 2 ? up : filter == 3 ? (left + up) / 2
                              : filter == 4 ? _paeth(left, up, corner) : 0;
            data[x] = static_cast<uint8_t>(data[x] - predictor);
        }
    }
}

static constexpr uint16_t LENGTH_BASE[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                                          31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static constexpr uint8_t LENGTH_BITS[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                         2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static constexpr uint16_t DISTANCE_BASE[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                            6145, 8193, 12289, 16385, 24577};
static constexpr uint8_t DISTANCE_BITS[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                           6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static uint32_t _reverse(uint32_t value, uint32_t bits)
{
    auto output = 0u;
    while (bits--) {
        output = (output << 1) | (value & 1u);
        value >>= 1;
    }
    return output;
}

struct PngBits
{
    uint8_t* data;
    size_t size = 2;
    uint32_t pending = 0;
    uint32_t count = 0;

    void write(uint32_t value, uint32_t bits)
    {
        pending |= value << count;
        count += bits;
        while (count >= 8) {
            data[size++] = static_cast<uint8_t>(pending);
            pending >>= 8;
            count -= 8;
        }
    }
};

static bool _huffman(const uint32_t* frequencies, uint32_t count, uint8_t* bits)
{
    uint64_t weights[572] = {};
    uint16_t parents[572] = {};
    auto nodes = count;
    auto active = 0u;
    for (auto i = 0u; i < count; i++) {
        weights[i] = frequencies[i];
        if (weights[i]) active++;
    }
    if (!active) {
        bits[0] = 1;
        return true;
    }
    while (active > 1) {
        auto first = nodes;
        auto second = nodes;
        for (auto i = 0u; i < nodes; i++) {
            if (!weights[i] || parents[i]) continue;
            if (first == nodes || weights[i] < weights[first]) {
                second = first;
                first = i;
            } else if (second == nodes || weights[i] < weights[second]) second = i;
        }
        weights[nodes] = weights[first] + weights[second];
        parents[first] = parents[second] = static_cast<uint16_t>(nodes++);
        active--;
    }
    for (auto i = 0u; i < count; i++) {
        if (!frequencies[i]) continue;
        auto depth = 0u;
        for (auto node = i; parents[node]; node = parents[node]) depth++;
        if (!depth) depth = 1;
        if (depth > 15) return false;
        bits[i] = static_cast<uint8_t>(depth);
    }
    return true;
}

static void _codes(const uint8_t* bits, uint32_t count, uint16_t* codes)
{
    uint32_t lengths[16] = {};
    uint32_t next[16] = {};
    for (auto i = 0u; i < count; i++) {
        if (bits[i]) lengths[bits[i]]++;
    }
    for (auto i = 1u; i < 16; i++) next[i] = (next[i - 1] + lengths[i - 1]) << 1;
    for (auto i = 0u; i < count; i++) {
        if (bits[i]) codes[i] = static_cast<uint16_t>(_reverse(next[bits[i]]++, bits[i]));
    }
}

static uint32_t _hash(const uint8_t* data)
{
    return ((data[0] * 251u + data[1]) * 251u + data[2]) & 32767u;
}

static uint8_t* _deflate(const uint8_t* raw, size_t size, size_t& outputSize)
{
    // Nine bits per input byte bounds fixed coding, including short matches.
    auto limit = std::numeric_limits<size_t>::max();
    if (size > (limit - 7u) / 9u || size > limit / sizeof(uint16_t)) return nullptr;
    auto capacity = (size * 9u + 7u) / 8u + 8u;
    if (capacity > UINT32_MAX) return nullptr;
    std::unique_ptr<uint32_t[]> chain(new (std::nothrow) uint32_t[65536]);
    std::unique_ptr<uint16_t[]> tokens(new (std::nothrow) uint16_t[size]);
    std::unique_ptr<uint8_t[]> output(new (std::nothrow) uint8_t[capacity]);
    if (!chain || !tokens || !output) return nullptr;
    auto heads = chain.get();
    auto previous = heads + 32768;
    for (auto i = 0u; i < 65536; i++) chain[i] = UINT32_MAX;
    uint32_t frequencies[286] = {};
    uint32_t distances[30] = {};
    size_t tokenCount = 0;
    uint64_t extraBits = 0;
    for (auto input = 0u; input < size;) {
        auto length = 2u;
        auto distance = 0u;
        auto maximum = static_cast<uint32_t>(size - input > 258 ? 258 : size - input);
        if (maximum >= 3) {
            auto candidate = heads[_hash(raw + input)];
            for (auto tries = 0u; tries < 64 && candidate != UINT32_MAX && input - candidate <= 32768; tries++) {
                if (raw[candidate] == raw[input] && raw[candidate + length] == raw[input + length]) {
                    auto matched = 1u;
                    while (matched < maximum && raw[candidate + matched] == raw[input + matched]) matched++;
                    if (matched > length) {
                        length = matched;
                        distance = input - candidate;
                        if (length == maximum) break;
                    }
                }
                candidate = previous[candidate & 32767u];
            }
        }
        auto consumed = 1u;
        if (distance) {
            auto code = 0u;
            while (code < 28 && length >= LENGTH_BASE[code + 1]) code++;
            frequencies[257 + code]++;
            extraBits += LENGTH_BITS[code];
            code = 0;
            while (code < 29 && distance >= DISTANCE_BASE[code + 1]) code++;
            distances[code]++;
            extraBits += DISTANCE_BITS[code];
            tokens[tokenCount++] = static_cast<uint16_t>(256 + length - 3);
            tokens[tokenCount++] = static_cast<uint16_t>(distance);
            consumed = length;
        } else {
            frequencies[raw[input]]++;
            tokens[tokenCount++] = raw[input];
        }
        for (auto end = input + consumed; input < end; input++) {
            if (size - input < 3) continue;
            auto hash = _hash(raw + input);
            previous[input & 32767u] = heads[hash];
            heads[hash] = input;
        }
    }
    frequencies[256] = 1;
    uint8_t literalBits[288] = {};
    uint8_t distanceBits[32] = {};
    auto dynamic = _huffman(frequencies, 286, literalBits) && _huffman(distances, 30, distanceBits);
    // A complete four-bit code-length alphabet avoids a second tree builder and RLE.
    uint64_t dynamicSize = 3 + 5 + 5 + 4 + 19 * 3 + (286 + 30) * 4 + extraBits;
    uint64_t fixedSize = 3 + extraBits;
    for (auto i = 0u; i < 286; i++) {
        dynamicSize += static_cast<uint64_t>(frequencies[i]) * literalBits[i];
        fixedSize += static_cast<uint64_t>(frequencies[i]) * (i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
    }
    for (auto i = 0u; i < 30; i++) {
        dynamicSize += static_cast<uint64_t>(distances[i]) * distanceBits[i];
        fixedSize += static_cast<uint64_t>(distances[i]) * 5;
    }
    dynamic = dynamic && dynamicSize < fixedSize;
    if (!dynamic) {
        for (auto i = 0u; i < 288; i++) literalBits[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        for (auto i = 0u; i < 32; i++) distanceBits[i] = 5;
    }
    uint16_t literalCodes[288] = {};
    uint16_t distanceCodes[32] = {};
    _codes(literalBits, 288, literalCodes);
    _codes(distanceBits, 32, distanceCodes);
    output[0] = 0x78;
    output[1] = 0x01;
    PngBits bits = {output.get()};
    bits.write(dynamic ? 5 : 3, 3);
    if (dynamic) {
        bits.write(29, 5);
        bits.write(29, 5);
        bits.write(15, 4);
        static constexpr uint8_t order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        for (auto value : order) bits.write(value < 16 ? 4 : 0, 3);
        for (auto i = 0u; i < 286; i++) bits.write(_reverse(literalBits[i], 4), 4);
        for (auto i = 0u; i < 30; i++) bits.write(_reverse(distanceBits[i], 4), 4);
    }
    for (size_t i = 0; i < tokenCount; i++) {
        auto token = tokens[i];
        if (token < 256) bits.write(literalCodes[token], literalBits[token]);
        else {
            auto length = token - 256u + 3;
            auto code = 0u;
            while (code < 28 && length >= LENGTH_BASE[code + 1]) code++;
            bits.write(literalCodes[257 + code], literalBits[257 + code]);
            bits.write(length - LENGTH_BASE[code], LENGTH_BITS[code]);
            auto distance = tokens[++i];
            code = 0;
            while (code < 29 && distance >= DISTANCE_BASE[code + 1]) code++;
            bits.write(distanceCodes[code], distanceBits[code]);
            bits.write(distance - DISTANCE_BASE[code], DISTANCE_BITS[code]);
        }
    }
    bits.write(literalCodes[256], literalBits[256]);
    if (bits.count) bits.write(0, 8 - bits.count);
    _be32(output.get() + bits.size, _adler(raw, size));
    outputSize = bits.size + 4;
    return output.release();
}

Result Saver::png(const Surface& surface, const char* path) noexcept
{
    if (!surface.data() || !surface.width() || !surface.height() || surface.stride() < surface.width() || !path) {
        return Result::InvalidArguments;
    }
    size_t rawSize;
    auto raw = _rgba(surface, true, rawSize);
    if (!raw) return Result::OutOfMemory;
    _filter(raw, surface.width(), surface.height());
    size_t zlibSize;
    auto zlib = _deflate(raw, rawSize, zlibSize);
    delete[] raw;
    if (!zlib) {
        return Result::OutOfMemory;
    }

    auto file = std::fopen(path, "wb");
    if (!file) {
        delete[] zlib;
        return Result::IoError;
    }
    static constexpr uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t header[13] = {};
    _be32(header, surface.width());
    _be32(header + 4, surface.height());
    header[8] = 8;
    header[9] = 6;
    auto ok = _write(file, signature, sizeof(signature));
    if (ok) ok = _chunk(file, "IHDR", header, sizeof(header));
    if (ok) ok = _chunk(file, "IDAT", zlib, static_cast<uint32_t>(zlibSize));
    if (ok) ok = _chunk(file, "IEND", nullptr, 0);
    if (std::fclose(file) != 0) ok = false;
    delete[] zlib;
    return ok ? Result::Success : Result::IoError;
}

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
static char* _quote(const char* path)
{
    auto size = std::strlen(path);
    if (size > (std::numeric_limits<size_t>::max() - 3u) / 4u) return nullptr;
    auto output = new (std::nothrow) char[size * 4 + 3];
    if (!output) return nullptr;
    auto offset = 0u;
    output[offset++] = '\'';
    for (auto i = 0u; i < size; i++) {
        if (path[i] == '\'') {
            output[offset++] = '\'';
            output[offset++] = '\\';
            output[offset++] = '\'';
            output[offset++] = '\'';
        } else {
            output[offset++] = path[i];
        }
    }
    output[offset++] = '\'';
    output[offset] = '\0';
    return output;
}

static char* _temporary(const char* path)
{
    auto size = std::strlen(path);
    static constexpr char suffix[] = ".vis-XXXXXX";
    if (size > std::numeric_limits<size_t>::max() - sizeof(suffix)) return nullptr;
    auto output = new (std::nothrow) char[size + sizeof(suffix)];
    if (!output) return nullptr;
    std::memcpy(output, path, size);
    std::memcpy(output + size, suffix, sizeof(suffix));
    auto descriptor = mkstemp(output);
    if (descriptor < 0) {
        delete[] output;
        return nullptr;
    }
    if (close(descriptor) == 0) return output;
    unlink(output);
    delete[] output;
    return nullptr;
}

#if !defined(__APPLE__)
struct SigpipeGuard
{
    sigset_t mask = {};
    sigset_t previous = {};
    bool active = false;
    bool pending = false;

    bool block()
    {
        sigemptyset(&mask);
        sigaddset(&mask, SIGPIPE);
        sigset_t current = {};
        if (sigpending(&current) != 0) return false;
        pending = sigismember(&current, SIGPIPE) == 1;
        if (pthread_sigmask(SIG_BLOCK, &mask, &previous) != 0) return false;
        active = true;
        return true;
    }

    void restore()
    {
        if (!active) return;
        if (!pending) {
            sigset_t current = {};
            if (sigpending(&current) == 0 && sigismember(&current, SIGPIPE) == 1) {
                timespec timeout = {};
                while (sigtimedwait(&mask, nullptr, &timeout) < 0 && errno == EINTR) {
                }
            }
        }
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        active = false;
    }
};
#endif
#endif

bool saver::ends(const char* value, const char* suffix) noexcept
{
    if (!value || !suffix) return false;
    auto valueSize = std::strlen(value);
    auto suffixSize = std::strlen(suffix);
    return valueSize >= suffixSize && std::strcmp(value + valueSize - suffixSize, suffix) == 0;
}

Result saver::timeline(const Scene* scene, uint32_t& fps, uint32_t& frames) noexcept
{
    if (!scene) return Result::InvalidArguments;
    auto& config = scene->config();
    if (!fps) fps = config.fps;
    if (!fps || !config.width || !config.height) return Result::InvalidArguments;
    auto total = scene->duration();
    if (!std::isfinite(total) || total < 0.0f) return Result::InvalidArguments;
    auto samples = static_cast<double>(total) * fps;
    if (!std::isfinite(samples)) return Result::InvalidArguments;
    auto adjacent = std::nextafter(total, std::numeric_limits<float>::infinity());
    if (std::isfinite(adjacent)) {
        auto tolerance = (static_cast<double>(adjacent) - total) * fps;
        auto integer = std::round(samples);
        if (std::fabs(samples - integer) <= tolerance) samples = integer;
    }
    auto frameValue = std::ceil(samples);
    if (!std::isfinite(frameValue) || frameValue > UINT32_MAX) return Result::InvalidArguments;
    frames = frameValue < 1.0 ? 1u : static_cast<uint32_t>(frameValue);
    return Result::Success;
}

Result saver::frames(const Scene* scene, SwRenderer* renderer, const char* path, uint32_t fps,
                     const char* options, bool evenDimensions, const char* soundtrack) noexcept
{
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
    static_cast<void>(scene);
    static_cast<void>(renderer);
    static_cast<void>(path);
    static_cast<void>(fps);
    static_cast<void>(options);
    static_cast<void>(evenDimensions);
    static_cast<void>(soundtrack);
    return Result::NonSupport;
#else
    if (!scene || !renderer || !path || !path[0] || !options || !options[0]) return Result::InvalidArguments;
    if (soundtrack && !soundtrack[0]) return Result::InvalidArguments;
    auto& config = scene->config();
    uint32_t frameCount;
    auto result = timeline(scene, fps, frameCount);
    if (result != Result::Success) return result;
    if (evenDimensions && ((config.width & 1u) || (config.height & 1u))) return Result::InvalidArguments;
    // The soundtrack is the second input, read for as long as the video lasts.
    char* audio = nullptr;
    if (soundtrack) {
        auto track = std::fopen(soundtrack, "rb");
        if (!track) return Result::IoError;
        std::fclose(track);
        auto quotedTrack = _quote(soundtrack);
        if (!quotedTrack) return Result::OutOfMemory;
        auto audioSize = std::strlen(quotedTrack) + 96u;
        audio = new (std::nothrow) char[audioSize];
        auto audioLength = audio ? std::snprintf(audio, audioSize, "-t %.6f -i %s -map 0:v:0 -map 1:a:0",
                                                 static_cast<double>(frameCount) / fps, quotedTrack)
                                 : -1;
        delete[] quotedTrack;
        if (audioLength < 0 || static_cast<size_t>(audioLength) >= audioSize) {
            delete[] audio;
            return Result::OutOfMemory;
        }
    }
    auto temporary = _temporary(path);
    if (!temporary) {
        delete[] audio;
        return Result::IoError;
    }
    auto quoted = _quote(temporary);
    if (!quoted) {
        delete[] audio;
        unlink(temporary);
        delete[] temporary;
        return Result::OutOfMemory;
    }
    auto optionSize = std::strlen(options) + (audio ? std::strlen(audio) : 0u);
    if (std::strlen(quoted) > std::numeric_limits<size_t>::max() - optionSize - 384u) {
        delete[] audio;
        delete[] quoted;
        unlink(temporary);
        delete[] temporary;
        return Result::OutOfMemory;
    }
    auto commandSize = std::strlen(quoted) + optionSize + 384u;
    auto command = new (std::nothrow) char[commandSize];
    if (!command) {
        delete[] audio;
        delete[] quoted;
        unlink(temporary);
        delete[] temporary;
        return Result::OutOfMemory;
    }
    auto commandLength = std::snprintf(command, commandSize,
                                       "ffmpeg -loglevel error -y -f rawvideo -pixel_format rgba -video_size %ux%u -framerate %u -i pipe:0 %s%s%s %s",
                                       config.width, config.height, fps, audio ? audio : "", audio ? " " : "",
                                       options, quoted);
    delete[] audio;
    delete[] quoted;
    if (commandLength < 0 || static_cast<size_t>(commandLength) >= commandSize) {
        delete[] command;
        unlink(temporary);
        delete[] temporary;
        return Result::OutOfMemory;
    }
    auto pipe = popen(command, "w");
    delete[] command;
    if (!pipe) {
        unlink(temporary);
        delete[] temporary;
        return Result::NonSupport;
    }

#if defined(__APPLE__)
    if (fcntl(fileno(pipe), F_SETNOSIGPIPE, 1) != 0) {
        pclose(pipe);
        unlink(temporary);
        delete[] temporary;
        return Result::IoError;
    }
#else
    SigpipeGuard sigpipe;
    if (!sigpipe.block()) {
        pclose(pipe);
        unlink(temporary);
        delete[] temporary;
        return Result::IoError;
    }
#endif

    Surface surface;
    uint8_t* bytes = nullptr;
    size_t byteSize = 0;
    result = Result::Success;
    for (auto frame = 0u; frame < frameCount; frame++) {
        auto time = static_cast<float>(frame) / static_cast<float>(fps);
        result = renderer->render(scene, time, surface);
        if (result != Result::Success) break;
        if (!bytes) {
            bytes = _rgba(surface, false, byteSize);
            if (!bytes) {
                result = Result::OutOfMemory;
                break;
            }
        } else {
            auto offset = 0u;
            for (auto y = 0u; y < surface.height(); y++) {
                for (auto x = 0u; x < surface.width(); x++) {
                    auto pixel = surface.data()[static_cast<size_t>(y) * surface.stride() + x];
                    bytes[offset++] = static_cast<uint8_t>(pixel);
                    bytes[offset++] = static_cast<uint8_t>(pixel >> 8);
                    bytes[offset++] = static_cast<uint8_t>(pixel >> 16);
                    bytes[offset++] = static_cast<uint8_t>(pixel >> 24);
                }
            }
        }
        if (std::fwrite(bytes, 1, byteSize, pipe) != byteSize) {
            result = Result::IoError;
            break;
        }
    }
    delete[] bytes;
    auto status = pclose(pipe);
#if !defined(__APPLE__)
    sigpipe.restore();
#endif
    if (status != 0 && result == Result::Success) result = Result::IoError;
    if (result == Result::Success && std::rename(temporary, path) != 0) result = Result::IoError;
    if (result != Result::Success) unlink(temporary);
    delete[] temporary;
    return result;
#endif
}

Result Saver::video(const Scene* scene, SwRenderer* renderer, const char* path, uint32_t fps) noexcept
{
    if (!scene || !renderer || !path || !path[0]) return Result::InvalidArguments;
    auto gif = saver::ends(path, ".gif");
    auto mp4 = saver::ends(path, ".mp4");
    if (!gif && !mp4) return Result::InvalidArguments;
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
    static_cast<void>(fps);
    return Result::NonSupport;
#else
    if (gif) return saver::gif(scene, renderer, path, fps);
    return saver::video(scene, renderer, path, fps);
#endif
}

Result Saver::save(const Scene* scene, Renderer& renderer, const char* path, uint32_t fps) noexcept
{
    if (renderer.engine() != RenderEngine::Cpu) return Result::NonSupport;
    return video(scene, renderer.cpuBackend(), path, fps);
}

Result Saver::save(const Scene* scene, Renderer& renderer, const char* path, uint32_t fps,
                   const char* soundtrack) noexcept
{
    if (!soundtrack) return save(scene, renderer, path, fps);
    if (renderer.engine() != RenderEngine::Cpu) return Result::NonSupport;
    if (!scene || !renderer.cpuBackend() || !path || !soundtrack[0] || !saver::ends(path, ".mp4")) {
        return Result::InvalidArguments;
    }
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
    static_cast<void>(fps);
    return Result::NonSupport;
#else
    return saver::video(scene, renderer.cpuBackend(), path, fps, soundtrack);
#endif
}

}  // namespace vis
