#pragma once

#include <cstdint>

namespace gba {

[[nodiscard]] constexpr auto register_byte(std::uint16_t value, std::uint32_t lane) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>((value >> ((lane & 1U) * 8U)) & 0xFFU);
}

[[nodiscard]] constexpr auto write_register_byte(std::uint16_t original, std::uint32_t lane,
                                                 std::uint8_t value, std::uint16_t writable) noexcept
    -> std::uint16_t {
    const auto shift = (lane & 1U) * 8U;
    const auto byteMask = static_cast<std::uint16_t>(0xFFU << shift);
    const auto selected = static_cast<std::uint16_t>(writable & byteMask);
    return static_cast<std::uint16_t>((original & ~selected) |
                                      ((static_cast<std::uint16_t>(value) << shift) & selected));
}

} // namespace gba
