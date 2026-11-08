#pragma once

#include <cstddef>
#include <cstdint>

#include "core/io/display.h"
#include "core/io/interrupt.h"
#include "core/io/keypad.h"
#include "core/io/wait_control.h"

namespace gba {

class IORegs {
  public:
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;

    static constexpr std::size_t kSizeBytes = 0x400U;
    static constexpr u32 kOffDISPCNT = DisplayRegisters::kDispcnt;
    static constexpr u32 kOffDISPSTAT = DisplayRegisters::kDispstat;
    static constexpr u32 kOffVCOUNT = DisplayRegisters::kVcount;
    static constexpr u32 kOffKEYINPUT = KeypadRegisters::kInput;
    static constexpr u32 kOffKEYCNT = KeypadRegisters::kControl;
    static constexpr u32 kOffIE = InterruptRegisters::kEnable;
    static constexpr u32 kOffIF = InterruptRegisters::kFlags;
    static constexpr u32 kOffWAITCNT = WaitControlRegisters::kControl;
    static constexpr u32 kOffIME = InterruptRegisters::kMaster;

    static constexpr u32 kBitsPerByte = 8U;
    static constexpr u32 kBitsPerHalf = 16U;
    static constexpr u32 kBits3Bytes = 24U;
    static constexpr u32 kByteMask = 0xFFU;
    static constexpr u16 kVisibleLines = DisplayRegisters::kVisibleLines;
    static constexpr u16 kDispstatFlagVBlank = DisplayRegisters::kFlagVBlank;
    static constexpr u16 kDispstatFlagHBlank = DisplayRegisters::kFlagHBlank;
    static constexpr u16 kDispstatFlagVCount = DisplayRegisters::kFlagVCount;
    static constexpr u16 kDispstatEnableVBlank = DisplayRegisters::kEnableVBlank;
    static constexpr u16 kDispstatEnableHBlank = DisplayRegisters::kEnableHBlank;
    static constexpr u16 kDispstatEnableVCount = DisplayRegisters::kEnableVCount;
    static constexpr u16 kDispstatLycShift = DisplayRegisters::kLycShift;
    static constexpr u16 kDispstatLycMask = DisplayRegisters::kLycMask;

    enum class Device : u8 {
        Display,
        Audio,
        Dma,
        Timer,
        Serial,
        Keypad,
        Interrupt,
        WaitControl,
        System,
        Unmapped
    };

    [[nodiscard]] static auto owner(u32 offset) noexcept -> Device;

    void reset() noexcept;
    [[nodiscard]] auto read8(u32 offset) const noexcept -> u8;
    void write8(u32 offset, u8 value) noexcept;
    [[nodiscard]] auto read16(u32 offset) const noexcept -> u16;
    void write16(u32 offset, u16 value) noexcept;
    [[nodiscard]] auto read32(u32 offset) const noexcept -> u32;
    void write32(u32 offset, u32 value) noexcept;

    void debug_set_vcount_for_tests(u16 scanline) noexcept { display_.set_vcount(scanline); }
    void debug_set_hblank_for_tests(bool hblank) noexcept { display_.set_hblank(hblank); }
    void debug_set_pressed_keys_for_tests(u16 pressed) noexcept { keypad_.set_pressed(pressed); }
    void debug_request_interrupt_for_tests(u16 mask) noexcept { interrupts_.request(mask); }
    [[nodiscard]] auto interrupt_pending() const noexcept -> bool { return interrupts_.pending(); }

  private:
    DisplayRegisters display_{};
    KeypadRegisters keypad_{};
    InterruptRegisters interrupts_{};
    WaitControlRegisters wait_{};
};

} // namespace gba
