#pragma once

#include <cstdint>

namespace gba {

struct RegisterByteWrite {
    std::uint32_t lane;
    std::uint8_t value;
    std::uint16_t writable_mask;
};

[[nodiscard]] constexpr auto register_byte(std::uint16_t value, std::uint32_t lane) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>((value >> ((lane & 1U) * 8U)) & 0xFFU);
}

[[nodiscard]] constexpr auto write_register_byte(std::uint16_t original, RegisterByteWrite update) noexcept
    -> std::uint16_t {
    const auto shift = (update.lane & 1U) * 8U;
    const auto byteMask = static_cast<std::uint16_t>(0xFFU << shift);
    const auto selected = static_cast<std::uint16_t>(update.writable_mask & byteMask);
    return static_cast<std::uint16_t>((original & ~selected) |
                                      ((static_cast<std::uint16_t>(update.value) << shift) & selected));
}

} // namespace gba
