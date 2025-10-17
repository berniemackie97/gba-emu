// src/core/bus/bus.h
#pragma once
#include <cstdint>
#include <filesystem>
#include "core/mmu/mmu.h"

namespace gba {
    class Bus {
      public:
        // Value seen on unmapped reads (a.k.a. open-bus). Named to avoid magic numbers.
        static constexpr std::uint8_t kOpenBus = MMU::kOpenBus;

        auto reset() noexcept -> void { mmu_.reset(); }
        auto load_bios(const std::filesystem::path &path) noexcept -> bool { return mmu_.load_bios(path); }

        [[nodiscard]] auto read8(std::uint32_t addr) const noexcept -> std::uint8_t { return mmu_.read8(addr); }

        // Keep (addr, value) order to match hardware docs.
        // Clang-tidy dislikes adjacent convertible params; suppress at this boundary.
        auto write8(std::uint32_t addr, std::uint8_t value) noexcept -> void {
            mmu_.write8(addr, value);
        } // NOLINT(bugprone-easily-swappable-parameters)
      private:
        MMU mmu_{};
    };
} // namespace gba
