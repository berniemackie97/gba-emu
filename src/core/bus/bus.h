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
        [[nodiscard]] auto load_gamepak(const std::filesystem::path &file) noexcept -> bool {
            return mmu_.load_gamepak(file);
        }
        void load_gamepak(std::span<const u8> bytes) noexcept { mmu_.load_gamepak(bytes); }

        // I/O debug hook passthroughs
        void debug_set_vcount_for_tests(std::uint16_t scanline) noexcept { mmu_.debug_set_vcount_for_tests(scanline); }
        void debug_set_hblank_for_tests(bool inHBlank) noexcept { mmu_.debug_set_hblank_for_tests(inHBlank); }

        // Byte access
        [[nodiscard]] auto read8(u32 addr) const noexcept -> u8 { return mmu_.read8(addr); }
        void write8(u32 addr, u8 value) noexcept { mmu_.write8(addr, value); }

        // Half/word access (CPU fetch path will use these)
        [[nodiscard]] auto read16(u32 addr) const noexcept -> std::uint16_t { return mmu_.read16(addr); }
        void write16(u32 addr, std::uint16_t value) noexcept { mmu_.write16(addr, value); }

        [[nodiscard]] auto read32(u32 addr) const noexcept -> std::uint32_t { return mmu_.read32(addr); }
        void write32(u32 addr, std::uint32_t value) noexcept { mmu_.write32(addr, value); }

      private:
        MMU mmu_{};
    };

} // namespace gba
