#include "core/io/io.h"

namespace gba {

auto IORegs::owner(u32 offset) noexcept -> Device {
    if (offset < 0x056U) {
        return Device::Display;
    }
    if (offset >= 0x060U && offset < 0x0A8U) {
        return Device::Audio;
    }
    if (offset >= 0x0B0U && offset < 0x0E0U) {
        return Device::Dma;
    }
    if (offset >= 0x100U && offset < 0x110U) {
        return Device::Timer;
    }
    if ((offset >= 0x120U && offset < 0x12CU) ||
        (offset >= 0x134U && offset < 0x15AU)) {
        return Device::Serial;
    }
    if (offset >= kOffKEYINPUT && offset < kOffKEYCNT + 2U) {
        return Device::Keypad;
    }
    if ((offset >= kOffIE && offset < kOffIF + 2U) ||
        (offset >= kOffIME && offset < kOffIME + 2U)) {
        return Device::Interrupt;
    }
    if (offset >= kOffWAITCNT && offset < kOffWAITCNT + 2U) {
        return Device::WaitControl;
    }
    if (offset >= 0x300U && offset < 0x302U) {
        return Device::System;
    }
    return Device::Unmapped;
}

void IORegs::reset() noexcept {
    display_.reset();
    keypad_.reset();
    interrupts_.reset();
    wait_.reset();
}

auto IORegs::read8(u32 offset) const noexcept -> u8 {
    switch (owner(offset)) {
    case Device::Display:
        return display_.read8(offset);
    case Device::Keypad:
        return keypad_.read8(offset);
    case Device::Interrupt:
        return interrupts_.read8(offset);
    case Device::WaitControl:
        return wait_.read8(offset);
    default:
        return 0xFFU;
    }
}

void IORegs::write8(u32 offset, u8 value) noexcept {
    switch (owner(offset)) {
    case Device::Display:
        display_.write8(offset, value);
        break;
    case Device::Keypad:
        keypad_.write8(offset, value);
        break;
    case Device::Interrupt:
        interrupts_.write8(offset, value);
        break;
    case Device::WaitControl:
        wait_.write8(offset, value);
        break;
    default:
        break;
    }
}

auto IORegs::read16(u32 offset) const noexcept -> u16 {
    const auto low = static_cast<u16>(read8(offset));
    const auto high = static_cast<u16>(read8(offset + 1U));
    return static_cast<u16>(low | (high << kBitsPerByte));
}

void IORegs::write16(u32 offset, u16 value) noexcept {
    write8(offset, static_cast<u8>(value & kByteMask));
    write8(offset + 1U, static_cast<u8>((value >> kBitsPerByte) & kByteMask));
}

auto IORegs::read32(u32 offset) const noexcept -> u32 {
    const auto low = static_cast<u32>(read16(offset));
    const auto high = static_cast<u32>(read16(offset + 2U));
    return low | (high << kBitsPerHalf);
}

void IORegs::write32(u32 offset, u32 value) noexcept {
    write16(offset, static_cast<u16>(value & 0xFFFFU));
    write16(offset + 2U, static_cast<u16>(value >> kBitsPerHalf));
}

} // namespace gba
