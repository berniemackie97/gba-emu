#pragma once

#include <cstdint>

#include "core/io/register_bytes.h"

namespace gba {

class KeypadRegisters {
  public:
    static constexpr std::uint32_t kInput = 0x130U;
    static constexpr std::uint32_t kControl = 0x132U;
    static constexpr std::uint16_t kKeys = 0x03FFU;
    static constexpr std::uint16_t kControlWritable = 0xC3FFU;

    void reset() noexcept {
        input_ = kKeys;
        control_ = 0;
    }

    [[nodiscard]] auto read8(std::uint32_t offset) const noexcept -> std::uint8_t {
        return register_byte(offset < kControl ? input_ : control_, offset);
    }

    void write8(std::uint32_t offset, std::uint8_t value) noexcept {
        if (offset >= kControl) {
            control_ = write_register_byte(control_, offset, value, kControlWritable);
        }
    }

    void set_pressed(std::uint16_t pressed) noexcept {
        input_ = static_cast<std::uint16_t>(kKeys & ~pressed);
    }

  private:
    std::uint16_t input_ = kKeys;
    std::uint16_t control_ = 0;
};

} // namespace gba
