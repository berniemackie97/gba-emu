#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

// namespace gba
namespace gba {
    class MMU {
      public:
        static constexpr std::size_t BIOS_SIZE = 0x4000U; // 16 KiB, avoid int->size_t widening
        static constexpr std::uint8_t kOpenBus = 0xFFU;

        auto reset() noexcept -> void;
        auto load_bios(const std::filesystem::path &file) noexcept -> bool;

        [[nodiscard]] auto read8(std::uint32_t addr) const noexcept -> std::uint8_t;
        auto write8(std::uint32_t /*addr*/, std::uint8_t /*value*/) noexcept -> void {}

      private:
        std::array<std::uint8_t, BIOS_SIZE> bios_{};
        bool bios_loaded_ = false;
    };
} // namespace gba
