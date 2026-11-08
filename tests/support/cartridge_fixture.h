#pragma once

#include <array>
#include <cstdint>

#include "core/content/cartridge_header.h"

namespace gba::test {

inline constexpr auto kSyntheticCartridgeHeader = [] {
    std::array<std::uint8_t, CartridgeHeader::kSize> bytes{};
    bytes[0x00U] = 0x2EU;
    bytes[0x03U] = 0xEAU;
    bytes[0xA0U] = 'D';
    bytes[0xA1U] = 'E';
    bytes[0xA2U] = 'M';
    bytes[0xA3U] = 'O';
    bytes[0xACU] = 'A';
    bytes[0xADU] = 'B';
    bytes[0xAEU] = 'C';
    bytes[0xAFU] = 'D';
    bytes[0xB0U] = '0';
    bytes[0xB1U] = '1';
    bytes[0xB2U] = 0x96U;
    bytes[0xBCU] = 0x02U;
    bytes[0xBDU] = 0xBFU;
    return bytes;
}();

} // namespace gba::test
