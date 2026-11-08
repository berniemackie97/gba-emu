#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace gba {

enum class ImageLoadError {
    None,
    Unavailable,
    NotRegularFile,
    TooSmall,
    TooLarge,
    IncompleteRead,
    OutOfMemory
};

struct FileImageLoad {
    ImageLoadError error;
    std::vector<std::uint8_t> bytes;
};

[[nodiscard]] auto image_load_error_message(ImageLoadError error) noexcept -> std::string_view;

[[nodiscard]] auto read_file_image(const std::filesystem::path &path, std::uintmax_t minimum_size,
                                   std::uintmax_t maximum_size) noexcept -> FileImageLoad;

} // namespace gba
