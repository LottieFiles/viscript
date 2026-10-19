// Asset and AssetLoader: images from a file, from encoded bytes or from raw pixels, decoded on
// first use.
#include <cstdio>
#include <cstring>
#include <new>

#include "vis.h"
#include "visAssetDecoder.h"

namespace vis
{

static constexpr uint32_t MAX_ASSET_PIXELS = 16777216;
static constexpr long MAX_ASSET_BYTES = 268435456;

static char* _copy(const char* text)
{
    auto size = std::strlen(text) + 1;
    auto copy = new (std::nothrow) char[size];
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

static bool _ends(const char* path, const char* suffix)
{
    auto pathSize = std::strlen(path);
    auto suffixSize = std::strlen(suffix);
    if (pathSize < suffixSize) return false;
    path += pathSize - suffixSize;
    for (size_t i = 0; i < suffixSize; ++i) {
        auto c = path[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != suffix[i]) return false;
    }
    return true;
}

static const char* _mime(const char* path)
{
    if (_ends(path, ".svg")) return "svg";
    if (_ends(path, ".png")) return "png";
    if (_ends(path, ".jpg") || _ends(path, ".jpeg")) return "jpg";
    if (_ends(path, ".webp")) return "webp";
    return nullptr;
}

Asset::Asset(uint32_t* pixels, uint8_t* bytes, uint32_t size, char* mime,
             uint32_t width, uint32_t height) noexcept :
    pixels(pixels), bytes(bytes), type(mime), byteSize(size), pixelWidth(width), pixelHeight(height)
{
}

Asset::~Asset()
{
    delete[] pixels;
    delete[] bytes;
    delete[] type;
}

uint32_t Asset::width() const noexcept
{
    return pixelWidth;
}

uint32_t Asset::height() const noexcept
{
    return pixelHeight;
}

const uint32_t* Asset::data() const noexcept
{
    if (pixels || !bytes) return pixels;
    uint32_t* decoded = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    if (asset::decode(bytes, byteSize, type, decoded, width, height) != Result::Success) return nullptr;
    if (width != pixelWidth || height != pixelHeight) {
        delete[] decoded;
        return nullptr;
    }
    pixels = decoded;
    return pixels;
}

const uint8_t* Asset::encoded() const noexcept
{
    return bytes;
}

uint32_t Asset::encodedSize() const noexcept
{
    return byteSize;
}

const char* Asset::mime() const noexcept
{
    return type;
}

Result AssetLoader::load(const char* path, Asset*& output) noexcept
{
    output = nullptr;
    if (!path || !path[0]) return Result::InvalidArguments;
    auto mime = _mime(path);
    if (!mime) return Result::NonSupport;
    auto file = std::fopen(path, "rb");
    if (!file) return Result::InvalidArguments;
    long size = -1;
    if (std::fseek(file, 0, SEEK_END) == 0) size = std::ftell(file);
    if (size <= 0 || size > MAX_ASSET_BYTES || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return Result::InvalidArguments;
    }
    auto data = new (std::nothrow) uint8_t[size];
    if (!data) {
        std::fclose(file);
        return Result::OutOfMemory;
    }
    auto read = std::fread(data, 1, static_cast<size_t>(size), file);
    std::fclose(file);
    auto result = read == static_cast<size_t>(size) ? load(data, static_cast<uint32_t>(size), mime, output)
                                                    : Result::InvalidArguments;
    delete[] data;
    return result;
}

Result AssetLoader::load(const void* data, uint32_t size, const char* mime,
                         Asset*& output) noexcept
{
    output = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    auto result = asset::probe(data, size, mime, width, height);
    if (result != Result::Success) return result;
    auto bytes = new (std::nothrow) uint8_t[size];
    auto type = _copy(mime);
    if (bytes && type) {
        std::memcpy(bytes, data, size);
        output = new (std::nothrow) Asset(nullptr, bytes, size, type, width, height);
        if (output) return Result::Success;
    }
    delete[] bytes;
    delete[] type;
    return Result::OutOfMemory;
}

Result AssetLoader::load(const uint32_t* pixels, uint32_t width, uint32_t height, Asset*& output) noexcept
{
    output = nullptr;
    if (!pixels || !width || !height || height > MAX_ASSET_PIXELS / width) return Result::InvalidArguments;
    auto copy = new (std::nothrow) uint32_t[static_cast<size_t>(width) * height];
    if (!copy) return Result::OutOfMemory;
    std::memcpy(copy, pixels, static_cast<size_t>(width) * height * sizeof(uint32_t));
    output = new (std::nothrow) Asset(copy, nullptr, 0, nullptr, width, height);
    if (output) return Result::Success;
    delete[] copy;
    return Result::OutOfMemory;
}

}  // namespace vis
