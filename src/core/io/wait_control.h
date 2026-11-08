#pragma once

#include <cstdint>

#include "core/io/register_bytes.h"

namespace gba {

class WaitControlRegisters {
  public:
    static constexpr std::uint32_t kControl = 0x204U;
    static constexpr std::uint16_t kWritable = 0x5FFFU;

    void reset() noexcept { control_ = 0; }

    [[nodiscard]] auto read8(std::uint32_t offset) const noexcept -> std::uint8_t {
        return register_byte(control_, offset);
    }

    void write8(std::uint32_t offset, std::uint8_t value) noexcept {
        control_ = write_register_byte(control_, {.lane = offset, .value = value, .writable_mask = kWritable});
    }

  private:
    std::uint16_t control_ = 0;
};

} // namespace gba
