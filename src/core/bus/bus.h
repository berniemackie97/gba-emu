// src/core/bus/bus.h
#pragma once
#include "core/mmu/mmu.h"
#include <filesystem>

namespace gba {

    class Bus {
      public:
        // lifecycle
        void reset() noexcept { mmu_.reset(); }

        // BIOS plumbing exposed for tests & future UI
        [[nodiscard]] auto load_bios(const std::filesystem::path &file) noexcept -> bool {
            return mmu_.load_bios(file);
        }

        // byte access (delegates to MMU)
        [[nodiscard]] auto read8(u32 addr) const noexcept -> u8 { return mmu_.read8(addr); }
        void write8(u32 addr, u8 value) noexcept { mmu_.write8(addr, value); }

      private:
        MMU mmu_{};
    };

} // namespace gba
