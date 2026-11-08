#pragma once

#include <cstdint>

#include "core/io/register_bytes.h"

namespace gba {

class DisplayRegisters {
  public:
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;

    static constexpr u32 kDispcnt = 0x000U;
    static constexpr u32 kDispstat = 0x004U;
    static constexpr u32 kVcount = 0x006U;
    static constexpr u16 kVisibleLines = 160U;
    static constexpr u16 kFlagVBlank = 0x0001U;
    static constexpr u16 kFlagHBlank = 0x0002U;
    static constexpr u16 kFlagVCount = 0x0004U;
    static constexpr u16 kEnableVBlank = 0x0008U;
    static constexpr u16 kEnableHBlank = 0x0010U;
    static constexpr u16 kEnableVCount = 0x0020U;
    static constexpr u16 kLycShift = 8U;
    static constexpr u16 kLycMask = 0xFF00U;
    static constexpr u16 kStatusWritable = 0xFF38U;

    void reset() noexcept {
        dispcnt_ = 0;
        dispstat_ = 0;
        vcount_ = 0;
        hblank_ = false;
    }

    [[nodiscard]] auto read8(u32 offset) const noexcept -> u8 {
        if (offset < kDispcnt + 2U) {
            return register_byte(dispcnt_, offset);
        }
        if (offset >= kDispstat && offset < kDispstat + 2U) {
            return register_byte(composed_dispstat(), offset);
        }
        if (offset >= kVcount && offset < kVcount + 2U) {
            return register_byte(vcount_, offset);
        }
        return 0xFFU;
    }

    void write8(u32 offset, u8 value) noexcept {
        if (offset < kDispcnt + 2U) {
            dispcnt_ = write_register_byte(dispcnt_, offset, value, 0xFFFFU);
        } else if (offset >= kDispstat && offset < kDispstat + 2U) {
            dispstat_ = write_register_byte(dispstat_, offset, value, kStatusWritable);
        }
    }

    void set_vcount(u16 scanline) noexcept { vcount_ = scanline; }
    void set_hblank(bool value) noexcept { hblank_ = value; }

  private:
    [[nodiscard]] auto composed_dispstat() const noexcept -> u16 {
        u16 status = dispstat_;
        if (vcount_ >= kVisibleLines) {
            status = static_cast<u16>(status | kFlagVBlank);
        }
        if (hblank_) {
            status = static_cast<u16>(status | kFlagHBlank);
        }
        if (vcount_ == (dispstat_ >> kLycShift)) {
            status = static_cast<u16>(status | kFlagVCount);
        }
        return status;
    }

    u16 dispcnt_ = 0;
    u16 dispstat_ = 0;
    u16 vcount_ = 0;
    bool hblank_ = false;
};

} // namespace gba
