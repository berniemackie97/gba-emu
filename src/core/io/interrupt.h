#pragma once

#include <cstdint>

#include "core/io/register_bytes.h"

namespace gba {

class InterruptRegisters {
  public:
    static constexpr std::uint32_t kEnable = 0x200U;
    static constexpr std::uint32_t kFlags = 0x202U;
    static constexpr std::uint32_t kMaster = 0x208U;
    static constexpr std::uint16_t kInterruptMask = 0x3FFFU;

    void reset() noexcept {
        enabled_ = 0;
        flags_ = 0;
        master_ = 0;
    }

    [[nodiscard]] auto read8(std::uint32_t offset) const noexcept -> std::uint8_t {
        if (offset < kFlags) {
            return register_byte(enabled_, offset);
        }
        if (offset < kFlags + 2U) {
            return register_byte(flags_, offset);
        }
        return register_byte(master_, offset);
    }

    void write8(std::uint32_t offset, std::uint8_t value) noexcept {
        if (offset < kFlags) {
            enabled_ = write_register_byte(enabled_, {.lane = offset, .value = value, .writable_mask = kInterruptMask});
        } else if (offset < kFlags + 2U) {
            const auto acknowledge = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(value) << ((offset & 1U) * 8U)) & kInterruptMask);
            flags_ = static_cast<std::uint16_t>(flags_ & ~acknowledge);
        } else {
            master_ = write_register_byte(master_, {.lane = offset, .value = value, .writable_mask = 0x0001U});
        }
    }

    void request(std::uint16_t flags) noexcept {
        flags_ = static_cast<std::uint16_t>(flags_ | (flags & kInterruptMask));
    }

    [[nodiscard]] auto pending() const noexcept -> bool {
        return master_ != 0 && (enabled_ & flags_) != 0;
    }

  private:
    std::uint16_t enabled_ = 0;
    std::uint16_t flags_ = 0;
    std::uint16_t master_ = 0;
};

} // namespace gba
