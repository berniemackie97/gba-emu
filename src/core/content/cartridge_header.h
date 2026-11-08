#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace gba {

struct CartridgeHeader {
    static constexpr std::size_t kSize = 0xC0U;

    std::array<std::uint8_t, 12> title{};
    std::array<std::uint8_t, 4> game_code{};
    std::array<std::uint8_t, 2> maker_code{};
    std::uint8_t software_version = 0;
};

[[nodiscard]] auto parse_cartridge_header(std::span<const std::uint8_t> image) noexcept
    -> std::optional<CartridgeHeader>;

} // namespace gba
