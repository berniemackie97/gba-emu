// src/core/mmu/mmu.cpp
#include "core/mmu/mmu.h"

#include <algorithm>
#include <new>
#include <ranges>
#include <stdexcept>

namespace gba {

    // bit/byte helpers (kill shift “magic numbers”)
    namespace {
        constexpr u32 kByteBits = 8U;
        constexpr u32 kHalfBits = 16U;
        constexpr u32 kThreeBytes = 24U;
        constexpr u32 kByteMask = 0xFFU;
        constexpr u32 kDisplayModeMask = 0x7U;
        constexpr std::size_t kObjectTileBase = 0x10000U;
        constexpr std::size_t kObjectBitmapBase = 0x14000U;
    } // namespace

    void MMU::reset() noexcept {
        bios_loaded_ = false;
        std::ranges::fill(bios_, u8{0x00});
        std::ranges::fill(ewram_, u8{0x00});
        std::ranges::fill(iwram_, u8{0x00});
        io_.reset();
        std::ranges::fill(pal_, u8{0x00});
        std::ranges::fill(vram_, u8{0x00});
        std::ranges::fill(oam_, u8{0x00});
        gamepak_.clear();
        cartridge_header_.reset();
    }

    // ------------------------------ LOADERS -------------------------------------------

    namespace {
        [[nodiscard]] auto finish_load(ImageLoadError *output, ImageLoadError result) noexcept -> bool {
            if (output != nullptr) {
                *output = result;
            }
            return result == ImageLoadError::None;
        }
    }

    auto MMU::load_bios(const std::filesystem::path &file, ImageLoadError *error) noexcept -> bool {
        const auto loaded = read_file_image(file, FileImageLimits{BIOS_SIZE, BIOS_SIZE});
        if (loaded.error != ImageLoadError::None) {
            return finish_load(error, loaded.error);
        }

        std::copy(loaded.bytes.begin(), loaded.bytes.end(), bios_.begin());
        bios_loaded_ = true;
        return finish_load(error, ImageLoadError::None);
    }

    auto MMU::load_gamepak(const std::filesystem::path &file, ImageLoadError *error) noexcept -> bool {
        auto loaded = read_file_image(file, FileImageLimits{GAMEPAK_HEADER_SIZE, WS_REGION_SIZE_32MiB});
        if (loaded.error != ImageLoadError::None) {
            return finish_load(error, loaded.error);
        }

        const auto header = parse_cartridge_header(loaded.bytes);
        if (!header.has_value()) {
            return finish_load(error, ImageLoadError::InvalidHeader);
        }

        gamepak_.swap(loaded.bytes);
        cartridge_header_ = header;
        return finish_load(error, ImageLoadError::None);
    }

    auto MMU::load_gamepak(std::span<const u8> bytes, ImageLoadError *error) noexcept -> bool {
        if (bytes.empty()) {
            return finish_load(error, ImageLoadError::TooSmall);
        }
        if (bytes.size() > WS_REGION_SIZE_32MiB) {
            return finish_load(error, ImageLoadError::TooLarge);
        }

        try {
            std::vector<u8> staged(bytes.begin(), bytes.end());
            gamepak_.swap(staged);
            cartridge_header_.reset();
        } catch (const std::bad_alloc &) {
            return finish_load(error, ImageLoadError::OutOfMemory);
        } catch (const std::length_error &) {
            return finish_load(error, ImageLoadError::OutOfMemory);
        }

        return finish_load(error, ImageLoadError::None);
    }

    // ------------------------------ ADDRESS HELPERS -------------------------------------------

    // ------------------------------ READS/WRITES -------------------------------------------

    auto MMU::read8(u32 addr) const noexcept -> u8 {
        // BIOS (open-bus if not loaded)
        if (in(addr, BIOS_BASE, BIOS_SIZE)) {
            const auto idx = static_cast<std::size_t>(addr - BIOS_BASE);
            return bios_loaded_ ? bios_.at(idx) : kOpenBus;
        }

        // Work RAM
        if (in_window(addr, EWRAM_BASE, kWindow16MiB)) {
            return ewram_.at(static_cast<std::size_t>((addr - EWRAM_BASE) & (EWRAM_SIZE - 1U)));
        }
        if (in_window(addr, IWRAM_BASE, kWindow16MiB)) {
            return iwram_.at(static_cast<std::size_t>((addr - IWRAM_BASE) & (IWRAM_SIZE - 1U)));
        }

        // I/O (stub)
        if (in(addr, IO_BASE, IO_SIZE)) {
            const u32 off = addr - IO_BASE;
            return io_.read8(off);
        }

        // Palette (mirrored every 0x400 within 16 MiB)
        if (in_window(addr, PAL_BASE, kWindow16MiB)) {
            return pal_.at(pal_offset(addr));
        }

        if (in_window(addr, VRAM_BASE, kWindow16MiB)) {
            return vram_.at(vram_offset(addr));
        }

        // OAM (mirrored every 0x400 within 16 MiB)
        if (in_window(addr, OAM_BASE, kWindow16MiB)) {
            return oam_.at(oam_offset(addr));
        }

        // gamepak ROM (three 32 MiB windows)
        if (in_any_ws(addr)) {
            const auto offset = static_cast<std::size_t>(addr - ws_base_of(addr));
            return offset < gamepak_.size() ? gamepak_[offset] : kOpenBus;
        }

        return kOpenBus; // unmapped for now
    }

    void MMU::write8(u32 addr, u8 value) noexcept {
        if (in_window(addr, PAL_BASE, kWindow16MiB)) {
            const auto offset = pal_offset(addr) & ~std::size_t{1};
            pal_[offset] = value;
            pal_[offset + 1U] = value;
            return;
        }
        if (in_window(addr, VRAM_BASE, kWindow16MiB)) {
            const auto offset = vram_offset(addr);
            const u32 mode = io_.read16(IORegs::kOffDISPCNT) & kDisplayModeMask;
            const auto objectStart = (mode >= 3U && mode <= 5U) ? kObjectBitmapBase : kObjectTileBase;
            if (offset >= objectStart) {
                return;
            }

            const auto aligned = offset & ~std::size_t{1};
            vram_[aligned] = value;
            vram_[aligned + 1U] = value;
            return;
        }
        if (in_window(addr, OAM_BASE, kWindow16MiB)) {
            return;
        }
        write_storage8(addr, value);
    }

    void MMU::write_storage8(u32 addr, u8 value) noexcept {
        // BIOS is read-only: ignore writes

        if (in_window(addr, EWRAM_BASE, kWindow16MiB)) {
            ewram_.at(static_cast<std::size_t>((addr - EWRAM_BASE) & (EWRAM_SIZE - 1U))) = value;
            return;
        }
        if (in_window(addr, IWRAM_BASE, kWindow16MiB)) {
            iwram_.at(static_cast<std::size_t>((addr - IWRAM_BASE) & (IWRAM_SIZE - 1U))) = value;
            return;
        }
        if (in(addr, IO_BASE, IO_SIZE)) {
            const u32 off = addr - IO_BASE;
            io_.write8(off, value);
            return;
        }
        if (in_window(addr, PAL_BASE, kWindow16MiB)) {
            pal_.at(pal_offset(addr)) = value;
            return;
        }
        if (in_window(addr, VRAM_BASE, kWindow16MiB)) {
            vram_.at(vram_offset(addr)) = value;
            return;
        }
        if (in_window(addr, OAM_BASE, kWindow16MiB)) {
            oam_.at(oam_offset(addr)) = value;
            return;
        }
        if (in_any_ws(addr)) {
            // ROM is read-only ignore writes
            return;
        }
        // ignore the rest for now
    }

    // ---- 16-bit access (little-endian; unaligned allowed) ----
    auto MMU::read16(u32 addr) const noexcept -> std::uint16_t {
        const u8 low = read8(addr);
        const u8 high = read8(addr + 1);
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(low) |
                                          (static_cast<std::uint16_t>(high) << kByteBits));
    }

    void MMU::write16(u32 addr, std::uint16_t value) noexcept {
        write_storage8(addr, static_cast<u8>(value & kByteMask));
        write_storage8(addr + 1, static_cast<u8>((value >> kByteBits) & kByteMask));
    }

    // ---- 32-bit access (little-endian; unaligned allowed) ----
    auto MMU::read32(u32 addr) const noexcept -> std::uint32_t {
        const u8 byte0 = read8(addr + 0);
        const u8 byte1 = read8(addr + 1);
        const u8 byte2 = read8(addr + 2);
        const u8 byte3 = read8(addr + 3);

        return (static_cast<u32>(byte0)) | (static_cast<u32>(byte1) << kByteBits) |
               (static_cast<u32>(byte2) << kHalfBits) | (static_cast<u32>(byte3) << kThreeBytes);
    }

    void MMU::write32(u32 addr, std::uint32_t value) noexcept {
        write_storage8(addr + 0, static_cast<u8>(value & kByteMask));
        write_storage8(addr + 1, static_cast<u8>((value >> kByteBits) & kByteMask));
        write_storage8(addr + 2, static_cast<u8>((value >> kHalfBits) & kByteMask));
        write_storage8(addr + 3, static_cast<u8>((value >> kThreeBytes) & kByteMask));
    }

} // namespace gba
