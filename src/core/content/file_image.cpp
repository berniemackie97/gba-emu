#include "core/content/file_image.h"

#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace gba {

auto image_load_error_message(ImageLoadError error) noexcept -> std::string_view {
    switch (error) {
    case ImageLoadError::None:
        return "No error";
    case ImageLoadError::Unavailable:
        return "File cannot be opened or inspected";
    case ImageLoadError::NotRegularFile:
        return "Asset must be a regular file";
    case ImageLoadError::TooSmall:
        return "Asset is empty or shorter than the required size";
    case ImageLoadError::TooLarge:
        return "Asset exceeds its supported size limit";
    case ImageLoadError::IncompleteRead:
        return "Asset changed or could not be read completely";
    case ImageLoadError::OutOfMemory:
        return "Insufficient memory to load asset";
    }
    return "Unknown asset load error";
}

auto read_file_image(const std::filesystem::path &path, std::uintmax_t minimum_size,
                     std::uintmax_t maximum_size) noexcept -> FileImageLoad {
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (ec || status.type() == std::filesystem::file_type::not_found) {
        return {ImageLoadError::Unavailable, {}};
    }
    if (!std::filesystem::is_regular_file(status)) {
        return {ImageLoadError::NotRegularFile, {}};
    }

    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return {ImageLoadError::Unavailable, {}};
    }
    if (size < minimum_size) {
        return {ImageLoadError::TooSmall, {}};
    }
    if (size > maximum_size || size > std::numeric_limits<std::size_t>::max()) {
        return {ImageLoadError::TooLarge, {}};
    }

    try {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return {ImageLoadError::Unavailable, {}};
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) ||
            stream.peek() != std::char_traits<char>::eof()) {
            return {ImageLoadError::IncompleteRead, {}};
        }

        return {ImageLoadError::None, std::move(bytes)};
    } catch (const std::bad_alloc &) {
        return {ImageLoadError::OutOfMemory, {}};
    } catch (const std::length_error &) {
        return {ImageLoadError::OutOfMemory, {}};
    } catch (const std::ios_base::failure &) {
        return {ImageLoadError::IncompleteRead, {}};
    } catch (const std::filesystem::filesystem_error &) {
        return {ImageLoadError::Unavailable, {}};
    }
}

} // namespace gba
