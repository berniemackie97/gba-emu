// src/core/cpu/arm7tdmi.h
#pragma once
#include <array>
#include <cstdint>

namespace gba {

    class Bus; // fwd

    class ARM7TDMI {
      public:
        using u8 = std::uint8_t;
        using u16 = std::uint16_t;
        using u32 = std::uint32_t;

        // Register file geometry
        static constexpr int kNumRegs = 16;
        static constexpr int kRegPC = 15;
        static constexpr u32 kRegIndexMask = 0x0FU; // for low-4-bit masks on r#
        static constexpr u32 kSignBit = 1U << 31;   // MSB of 32-bit

        // Bit geometry
        static constexpr u32 kByteBits = 8U;
        static constexpr u32 kWordBits = 32U;
        static constexpr u32 kRotateMask = kWordBits - 1U; // 31
        static constexpr u32 kWordMask = ~u32{3};          // align to 4 bytes
        static constexpr u32 kLow3Mask = 0x07U;            // 3-bit register field
        static constexpr u32 kImm11Mask = 0x07FFU;         // 11-bit immediate

        // CPSR flags
        static constexpr u32 kFlagN = 1U << 31;
        static constexpr u32 kFlagZ = 1U << 30;
        static constexpr u32 kFlagC = 1U << 29;
        static constexpr u32 kFlagV = 1U << 28;
        static constexpr u32 kFlagT = 1U << 5;

        ARM7TDMI() = default;

        void attach(Bus &bus) noexcept { bus_ = &bus; }
        void reset() noexcept;

        void step() noexcept; // execute one Thumb16

        // -------- Debug/test hooks --------
        void debug_set_program_counter(u32 addr) noexcept { regs_[kRegPC] = addr & ~u32{1}; }
        void debug_set_reg(int index, u32 value) noexcept {
            regs_.at(static_cast<std::size_t>(index & static_cast<int>(kRegIndexMask))) = value;
        }
        [[nodiscard]] auto debug_pc() const noexcept -> u32 { return regs_[kRegPC]; }
        [[nodiscard]] auto debug_reg(int index) const noexcept -> u32 {
            return regs_.at(static_cast<std::size_t>(index & static_cast<int>(kRegIndexMask)));
        }
        [[nodiscard]] auto debug_cpsr() const noexcept -> u32 { return cpsr_; }

      private:
        std::array<u32, kNumRegs> regs_{}; // r0..r15 (r15==PC)
        u32 cpsr_ = kFlagT;
        Bus *bus_ = nullptr; // not owned

        // ----- Helpers -----
        // For B(imm11): after shift-left 1, we have a 12-bit signed offset.
        static constexpr auto sign_extend12(u32 value) noexcept -> u32 {
            constexpr u32 kWidth = 12U;
            const u32 mask = (1U << kWidth) - 1U;
            const u32 signMask = 1U << (kWidth - 1U);
            const u32 masked = value & mask;
            return (masked ^ signMask) - signMask; // classic 2's-complement sign extend
        }

        void set_nz(u32 result) noexcept;
        void set_add_cv(u32 augend, u32 addend, u32 result) noexcept;
        void set_sub_cv(u32 minuend, u32 subtrahend, u32 result) noexcept;

        // Thumb decoders for the subset we support
        void exec_mov_imm(u16 insn) noexcept; // 00100 Rd imm8
        void exec_add_imm(u16 insn) noexcept; // 00110 Rd imm8
        void exec_sub_imm(u16 insn) noexcept; // 00111 Rd imm8
        void exec_b(u16 insn) noexcept;       // 11100 imm11

        // ----- BIT HELPERS -----
        // Rotate-right by 'amount' (mod 32). Second param is a byte on purpose
        // (avoids the easily-swappable-parameters lint warning).

        struct RotRight {
            explicit constexpr RotRight(std::uint8_t n) : n(n) {}
            std::uint8_t n;
        };
        static constexpr auto rotr(u32 value, RotRight amt) noexcept -> u32;

        // ---- Unaligned word access semantics per ARM7TDMI ----
        // LDR word: read aligned word then rotate right by 8 * (addr & 3)
        [[nodiscard]] auto read_u32_unaligned(u32 addr) const noexcept -> u32;

        // STR word: rotate left by 8 * (addr & 3) then write at aligned address
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — (address, value) matches bus API
        void write_u32_unaligned(u32 addr, u32 value) noexcept;

        // ---- Thumb exec handlers (subset) ----
        void exec_ldr_literal(u16 insn) noexcept; // 01001 Rd, [PC, #imm8*4]
        void exec_ldr_imm_w(u16 insn) noexcept;   // 01101 LDR  Rd, [Rb, #imm5*4]
        void exec_str_imm_w(u16 insn) noexcept;   // 01100 STR  Rd, [Rb, #imm5*4]
        void exec_ldr_imm_b(u16 insn) noexcept;   // 01111 LDRB Rd, [Rb, #imm5]
        void exec_str_imm_b(u16 insn) noexcept;   // 01110 STRB Rd, [Rb, #imm5]
    };

} // namespace gba
