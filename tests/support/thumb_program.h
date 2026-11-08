#pragma once

#include <cstdint>
#include <span>

#include "core/bus/bus.h"

namespace gba::test {

inline void write_thumb_program(Bus &bus, std::uint32_t address,
                                std::span<const std::uint16_t> instructions) noexcept {
    for (const auto instruction : instructions) {
        bus.write16(address, instruction);
        address += 2U;
    }
}

} // namespace gba::test
