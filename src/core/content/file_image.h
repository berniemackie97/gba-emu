#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace gba {

enum class ImageLoadError : std::uint8_t {
    None,
    Unavailable,
    NotRegularFile,
    TooSmall,
    TooLarge,
    IncompleteRead,
    InvalidHeader,
    OutOfMemory
};

struct FileImageLoad {
    ImageLoadError error;
    std::vector<std::uint8_t> bytes;
};

struct FileImageLimits {
    std::uintmax_t minimum;
    std::uintmax_t maximum;
};

[[nodiscard]] auto image_load_error_message(ImageLoadError error) noexcept -> std::string_view;

[[nodiscard]] auto read_file_image(const std::filesystem::path &path, FileImageLimits limits) noexcept -> FileImageLoad;

} // namespace gba
