// src/core/mmu/mmu.h
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <sys/stat.h>
#include <vector>

namespace gba {

    using u8 = std::uint8_t;
    using u32 = std::uint32_t;

    class MMU {
      public:
        // --- Region bases & sizes (GBATEK) ---
        static constexpr u32 BIOS_BASE = 0x00000000U;
        static constexpr std::size_t BIOS_SIZE = 0x00004000U; // 16 KiB

        static constexpr u32 EWRAM_BASE = 0x02000000U;
        static constexpr std::size_t EWRAM_SIZE = 0x00040000U; // 256 KiB

        static constexpr u32 IWRAM_BASE = 0x03000000U;
        static constexpr std::size_t IWRAM_SIZE = 0x00008000U; // 32 KiB

        static constexpr u32 IO_BASE = 0x04000000U;
        static constexpr std::size_t IO_SIZE = 0x00000400U; // 1 KiB of regs (0x000..0x3FE)

        static constexpr u32 PAL_BASE = 0x05000000U;
        static constexpr std::size_t PAL_SIZE = 0x00000400U; // 1 KiB, mirrored

        static constexpr u32 VRAM_BASE = 0x06000000U;
        static constexpr std::size_t VRAM_SIZE = 0x00018000U; // 96 KiB

        static constexpr u32 OAM_BASE = 0x07000000U;
        static constexpr std::size_t OAM_SIZE = 0x00000400U; // 1 KiB, mirrored

        // GamePak ROM windows (3 wait state regions each 32 MiB)
        static constexpr u32 WS0_BASE = 0x08000000U;
        static constexpr u32 WS1_BASE = 0x0A000000U;
        static constexpr u32 WS2_BASE = 0x0C000000U;
        static constexpr u32 WS_REGION_SIZE_32MiB = 0x02000000U; // 32 MiB per region

        // --- Window constants used for aliasing behaviour (names beat hex) ---
        static constexpr u32 kWindow16MiB = 0x01000000U;                     // palette/OAM mirror window
        static constexpr u32 kVRAMWindow128KiB = 0x00020000U;                // 128 KiB window 0x06000000–0x0601FFFF
        static constexpr u32 kVRAMTailBytes = kVRAMWindow128KiB - VRAM_SIZE; // 32 KiB tail that mirrors first 32 KiB

        static constexpr u8 kOpenBus = 0xFFU;

        // lifecycle & ROM
        void reset() noexcept;

        auto load_bios(const std::filesystem::path &file) noexcept -> bool;

        // GamePak: file based and in memory loading
        auto load_gamepak(const std::filesystem::path &file) noexcept -> bool;
        void load_gamepak(std::span<const u8> bytes) noexcept;

        // byte access (we’ll add timings and alignment rules later per width)
        [[nodiscard]] auto read8(u32 addr) const noexcept -> u8;
        void write8(u32 addr, u8 value) noexcept;

        // typed widths (unaligned permitted on GBA; CPU handles rotation)
        [[nodiscard]] auto read16(u32 addr) const noexcept -> std::uint16_t;
        void write16(u32 addr, std::uint16_t value) noexcept;

        [[nodiscard]] auto read32(u32 addr) const noexcept -> std::uint32_t;
        void write32(u32 addr, std::uint32_t value) noexcept;

      private:
        // helpers
        [[nodiscard]] static constexpr auto in(u32 addr, u32 base, std::size_t size) noexcept -> bool {
            return addr >= base && addr < base + static_cast<u32>(size);
        }
        [[nodiscard]] static constexpr auto in_window(u32 addr, u32 base, u32 window) noexcept -> bool {
            return addr >= base && addr < base + window;
        }

        [[nodiscard]] static constexpr auto in_any_ws(u32 addr) noexcept -> bool {
            return (addr >= WS0_BASE && addr < WS0_BASE + WS_REGION_SIZE_32MiB) ||
                   (addr >= WS1_BASE && addr < WS1_BASE + WS_REGION_SIZE_32MiB) ||
                   (addr >= WS2_BASE && addr < WS2_BASE + WS_REGION_SIZE_32MiB);
        }
        [[nodiscard]] static constexpr auto ws_base_of(u32 addr) noexcept -> u32 {
            // Which 32 MiB wait state is this address inside?
            if (addr >= WS2_BASE) {
                return WS2_BASE;
            }
            if (addr >= WS1_BASE) {
                return WS1_BASE;
            }
            return WS0_BASE;
        }

        // aliasing helpers
        [[nodiscard]] static auto vram_offset(u32 addr) noexcept -> std::size_t {
            // In first 128 KiB window: 0..(96 KiB-1) map to VRAM; 96 KiB..128 KiB mirror 0..32 KiB
            const u32 off = addr - VRAM_BASE;
            return static_cast<std::size_t>((off < static_cast<u32>(VRAM_SIZE)) ? off
                                                                                : (off - static_cast<u32>(VRAM_SIZE)));
        }
        [[nodiscard]] static auto pal_offset(u32 addr) noexcept -> std::size_t {
            // 1 KiB mirrored across 16 MiB block
            return static_cast<std::size_t>((addr - PAL_BASE) & (static_cast<u32>(PAL_SIZE) - 1U));
        }
        [[nodiscard]] static auto oam_offset(u32 addr) noexcept -> std::size_t {
            // 1 KiB mirrored across 16 MiB block
            return static_cast<std::size_t>((addr - OAM_BASE) & (static_cast<u32>(OAM_SIZE) - 1U));
        }

        // GamePak helpers
        [[nodiscard]] auto gamepak_index(u32 addr) const noexcept -> std::size_t;

        // backing stores (simple arrays for now)
        std::array<u8, BIOS_SIZE> bios_{};
        std::array<u8, EWRAM_SIZE> ewram_{};
        std::array<u8, IWRAM_SIZE> iwram_{};
        std::array<u8, IO_SIZE> io_{}; // stubbed register space
        std::array<u8, PAL_SIZE> pal_{};
        std::array<u8, VRAM_SIZE> vram_{};
        std::array<u8, OAM_SIZE> oam_{};

        // GamePak ROM (dynamic size mirrors by size inside each 32 MiB window)
        std::vector<u8> gamepak_;

        bool bios_loaded_ = false;
    };

} // namespace gba
