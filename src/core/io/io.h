// src/core/io/io.h
#pragma once
#include <array>
#include <cstdint>
#include <algorithm> // std::ranges::fill lives here

namespace gba {

    /**
     * I/O register block (0x04000000 – 0x040003FE).
     *
     * We model a small, typed subset:
     *   - DISPCNT  (0x0000, 16-bit, read/write)
     *   - DISPSTAT (0x0004, 16-bit, read/write placeholder)
     *   - VCOUNT   (0x0006, 16-bit, READ-ONLY; written value is ignored)
     *
     * Notes
     * - The real hardware has many more regs. We’ll add them incrementally.
     * - Reads/writes are little-endian. Unaligned 8/16/32 is permitted (CPU handles rotate on fetch).
     * - 32-bit accesses span adjacent 16-bit regs as on hardware.
     */
    class IORegs {
      public:
        using u8 = std::uint8_t;
        using u16 = std::uint16_t;
        using u32 = std::uint32_t;

        // Region size (1 KiB of mappable I/O)
        static constexpr std::size_t kSizeBytes = 0x00000400U;

        // Offsets of specific registers within the I/O window
        static constexpr u32 kOffDISPCNT = 0x0000U;  // 16-bit
        static constexpr u32 kOffDISPSTAT = 0x0004U; // 16-bit
        static constexpr u32 kOffVCOUNT = 0x0006U;   // 16-bit (read-only)

        // Bit/byte helpers
        static constexpr u32 kBitsPerByte = 8U;
        static constexpr u32 kBitsPerHalf = 16U;
        static constexpr u32 kBits3Bytes = 24U;
        static constexpr u32 kByteMask = 0xFFU;

        // Video timing constants we need for flags
        static constexpr u16 kVisibleLines = 160U; // 0..159 visible; >=160 is VBlank

        // DISPSTAT bit positions/masks (bits 0..2 are flags on read; write ignores them)
        static constexpr u16 kDispstatFlagVBlank = static_cast<u16>(1U << 0);
        static constexpr u16 kDispstatFlagHBlank = static_cast<u16>(1U << 1);
        static constexpr u16 kDispstatFlagVCount = static_cast<u16>(1U << 2);
        static constexpr u16 kDispstatEnableVBlank = static_cast<u16>(1U << 3); // IRQ enable (kept for future)
        static constexpr u16 kDispstatEnableHBlank = static_cast<u16>(1U << 4);
        static constexpr u16 kDispstatEnableVCount = static_cast<u16>(1U << 5);
        static constexpr u16 kDispstatLycShift = 8U;
        static constexpr u16 kDispstatLycMask = static_cast<u16>(0xFFU << kDispstatLycShift);

        void reset() noexcept {
            std::ranges::fill(raw_, u8{0x00});
            vcount_ = 0; // PPU will drive this later; 0..227 lines on GBA
        }

        // ---- 8/16/32-bit API (offset is relative to 0x04000000) ----
        [[nodiscard]] auto read8(u32 offset) const noexcept -> u8 {
            // VCOUNT low/high bytes are system driven
            if (offset == kOffVCOUNT) {
                return static_cast<u8>(vcount_ & kByteMask);
            }
            if (offset == kOffVCOUNT + 1U) {
                return static_cast<u8>((vcount_ >> kBitsPerByte) & kByteMask);
            }

            // DISPSTAT computed on read (flags are dynamic)
            if (offset == kOffDISPSTAT || offset == kOffDISPSTAT + 1U) {
                const u16 disp = composed_dispstat();
                const bool high = (offset == kOffDISPSTAT + 1U);
                return static_cast<u8>((high ? (disp >> kBitsPerByte) : disp) & kByteMask);
            }
            return raw_.at(static_cast<std::size_t>(offset));
        }

        void write8(u32 offset, u8 value) noexcept {
            // VCOUNT is read-only
            if (offset == kOffVCOUNT || offset == kOffVCOUNT + 1U) {
                // read-only: ignore
                return;
            }

            // DISPSTAT only writable bits are IRQ enables + LYC (bits 8..15)
            if (offset == kOffDISPSTAT || offset == kOffDISPSTAT + 1U) {
                const bool high = (offset == kOffDISPSTAT + 1U);
                const u16 current = dispstat_shadow_;
                const u16 merged =
                    static_cast<u16>(high ? ((current & 0x00FFU) | (static_cast<u16>(value) << kBitsPerByte))
                                          : ((current & 0xFF00U) | static_cast<u16>(value)));
                write16_dispstat(merged);
                return;
            }
            raw_.at(static_cast<std::size_t>(offset)) = value;
        }

        [[nodiscard]] auto read16(u32 offset) const noexcept -> u16 {
            if (offset == kOffVCOUNT) {
                return vcount_;
            }
            if (offset == kOffDISPSTAT) {
                return composed_dispstat();
            }

            const u8 low = read8(offset);
            const u8 high = read8(offset + 1U);
            return static_cast<u16>(static_cast<u16>(low) | (static_cast<u16>(high) << kBitsPerByte));
        }

        void write16(u32 offset, u16 value) noexcept {
            if (offset == kOffVCOUNT) {
                return;
            } // read-only
            if (offset == kOffDISPSTAT) {
                write16_dispstat(value);
                return;
            }

            write8(offset + 0U, static_cast<u8>(value & kByteMask));
            write8(offset + 1U, static_cast<u8>((value >> kBitsPerByte) & kByteMask));
        }

        [[nodiscard]] auto read32(u32 offset) const noexcept -> u32 {
            const u8 byte0 = read8(offset + 0U);
            const u8 byte1 = read8(offset + 1U);
            const u8 byte2 = read8(offset + 2U);
            const u8 byte3 = read8(offset + 3U);
            return (static_cast<u32>(byte0)) | (static_cast<u32>(byte1) << kBitsPerByte) |
                   (static_cast<u32>(byte2) << kBitsPerHalf) | (static_cast<u32>(byte3) << kBits3Bytes);
        }

        void write32(u32 offset, u32 value) noexcept {
            write8(offset + 0U, static_cast<u8>(value & kByteMask));
            write8(offset + 1U, static_cast<u8>((value >> kBitsPerByte) & kByteMask));
            write8(offset + 2U, static_cast<u8>((value >> kBitsPerHalf) & kByteMask));
            write8(offset + 3U, static_cast<u8>((value >> kBits3Bytes) & kByteMask));
        }

        // Hook the PPU/scheduler will use later
        void debug_set_vcount_for_tests(u16 scanline) noexcept { vcount_ = scanline; }
        void debug_set_hblank_for_tests(bool hblank) noexcept { hblank_ = hblank; }

      private:
        std::array<u8, kSizeBytes> raw_{};
        u16 vcount_ = 0;          // system-driven (PPU)
        bool hblank_ = false;     // system-driven (PPU)
        u16 dispstat_shadow_ = 0; // writable bits of DISPSTAT (IRQ enables + LYC)

        // Compose DISPSTAT value on read: flags are live, others are the shadow
        [[nodiscard]] auto composed_dispstat() const noexcept -> u16 {
            // Start with writable bits (IRQ enables + LYC field)
            const u16 writable =
                static_cast<u16>(dispstat_shadow_ & static_cast<u16>(kDispstatEnableVBlank | kDispstatEnableHBlank |
                                                                     kDispstatEnableVCount | kDispstatLycMask));

            // Live flags
            const bool inVBlank = (vcount_ >= kVisibleLines);
            const u16 lyc = static_cast<u16>((dispstat_shadow_ & kDispstatLycMask) >> kDispstatLycShift);
            const bool vcountMatches = (vcount_ == lyc);

            u16 composed = writable;
            if (inVBlank) {
                composed = static_cast<u16>(composed | kDispstatFlagVBlank);
            }
            if (hblank_) {
                composed = static_cast<u16>(composed | kDispstatFlagHBlank);
            }
            if (vcountMatches) {
                composed = static_cast<u16>(composed | kDispstatFlagVCount);
            }
            return composed;
        }

        // Only allow writable bits
        void write16_dispstat(u16 value) noexcept {
            constexpr u16 kWritableMask = static_cast<u16>(kDispstatEnableVBlank | kDispstatEnableHBlank |
                                                           kDispstatEnableVCount | kDispstatLycMask);
            dispstat_shadow_ =
                static_cast<u16>((dispstat_shadow_ & static_cast<u16>(~kWritableMask)) | (value & kWritableMask));
        }
    };

} // namespace gba
