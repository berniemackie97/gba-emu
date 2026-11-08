#include "core/content/cartridge_header.h"

#include <algorithm>

namespace gba {

auto parse_cartridge_header(std::span<const std::uint8_t> image) noexcept
    -> std::optional<CartridgeHeader> {
    constexpr std::size_t kTitleStart = 0xA0U;
    constexpr std::size_t kCodeStart = 0xACU;
    constexpr std::size_t kMakerStart = 0xB0U;
    constexpr std::size_t kFixedByte = 0xB2U;
    constexpr std::size_t kVersion = 0xBCU;
    constexpr std::size_t kComplement = 0xBDU;

    if (image.size() < CartridgeHeader::kSize || image[kFixedByte] != 0x96U) {
        return std::nullopt;
    }

    unsigned checksum = 0x19U;
    for (std::size_t offset = kTitleStart; offset < kComplement; ++offset) {
        checksum += image[offset];
    }
    if (static_cast<std::uint8_t>(0U - checksum) != image[kComplement]) {
        return std::nullopt;
    }

    CartridgeHeader header{};
    std::copy_n(image.begin() + kTitleStart, header.title.size(), header.title.begin());
    std::copy_n(image.begin() + kCodeStart, header.game_code.size(), header.game_code.begin());
    std::copy_n(image.begin() + kMakerStart, header.maker_code.size(), header.maker_code.begin());
    header.software_version = image[kVersion];
    return header;
}

} // namespace gba
