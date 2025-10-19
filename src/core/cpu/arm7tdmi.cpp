// src/core/cpu/arm7tdmi.cpp
#include "core/cpu/arm7tdmi.h"
#include "core/bus/bus.h"

namespace gba {

    // -------------------------- lifecycle --------------------------

    void ARM7TDMI::reset() noexcept {
        regs_.fill(0U);
        cpsr_ = kFlagT; // start in Thumb state
    }

    // -------------------------- flag helpers --------------------------

    void ARM7TDMI::set_nz(u32 result) noexcept {
        cpsr_ &= ~(kFlagN | kFlagZ);
        if (result == 0U) {
            cpsr_ |= kFlagZ;
        }
        if ((result & kSignBit) != 0U) {
            cpsr_ |= kFlagN;
        }
    }

    void ARM7TDMI::set_add_cv(u32 augend, u32 addend, u32 result) noexcept {
        // Carry = unsigned overflow on addition
        const auto wideSum = static_cast<unsigned long long>(augend) + static_cast<unsigned long long>(addend);
        const bool carry = (wideSum >> 32U) != 0ULL;
        if (carry) {
            cpsr_ |= kFlagC;
        } else {
            cpsr_ &= ~kFlagC;
        }

        // Overflow = signed overflow when operands share sign and result flips
        const u32 ovMask = (~(augend ^ addend) & (augend ^ result)) & kSignBit;
        if (ovMask != 0U) {
            cpsr_ |= kFlagV;
        } else {
            cpsr_ &= ~kFlagV;
        }
    }

    void ARM7TDMI::set_sub_cv(u32 minuend, u32 subtrahend, u32 result) noexcept {
        // For SUB: C means NOT borrow (a >= b)
        if (minuend >= subtrahend) {
            cpsr_ |= kFlagC;
        } else {
            cpsr_ &= ~kFlagC;
        }

        // Overflow = signs differ between operands and result sign differs from minuend
        const u32 ovMask = ((minuend ^ subtrahend) & (minuend ^ result)) & kSignBit;
        if (ovMask != 0U) {
            cpsr_ |= kFlagV;
        } else {
            cpsr_ &= ~kFlagV;
        }
    }

    // -------------------------- tiny bit helpers --------------------------

    // constexpr, header declares, here we define
    constexpr auto ARM7TDMI::rotr(u32 value, RotRight amt) noexcept -> u32 {
        const auto amt8 = static_cast<std::uint8_t>(amt.n & static_cast<std::uint8_t>(kRotateMask));
        // (x >> n) | (x << (32 - n)) with mod-32 semantics
        return (value >> amt8) | (value << static_cast<std::uint8_t>((kWordBits - amt8) & kRotateMask));
    }

    // -------------------------- unaligned word semantics --------------------------

    auto ARM7TDMI::read_u32_unaligned(u32 address) const noexcept -> u32 {
        const u32 aligned = address & kWordMask;
        const u32 raw = bus_->read32(aligned);
        const auto byteRot = static_cast<std::uint8_t>((address & 0x3U) * kByteBits);
        return rotr(raw, RotRight{byteRot});
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void ARM7TDMI::write_u32_unaligned(u32 addr, u32 value) noexcept {
        const u32 aligned = addr & kWordMask;
        // rotate left by 8*(addr&3) == rotr by (32 - 8*(addr&3))
        const auto rotL = static_cast<std::uint8_t>((kWordBits - ((addr & 0x3U) * kByteBits)) & kRotateMask);
        const u32 rotated = rotr(value, RotRight{rotL});
        bus_->write32(aligned, rotated);
    }

    // -------------------------- Thumb subset: data-processing --------------------------

    void ARM7TDMI::exec_mov_imm(u16 insn) noexcept {
        const u32 destReg = (insn >> 8U) & 0x7U;
        const u32 imm8 = insn & 0xFFU;
        regs_.at(destReg) = imm8;
        set_nz(regs_.at(destReg)); // C/V unaffected
    }

    void ARM7TDMI::exec_add_imm(u16 insn) noexcept {
        const u32 destReg = (insn >> 8U) & 0x7U;
        const u32 immValue = insn & 0xFFU;
        const u32 regValue = regs_.at(destReg);
        const u32 sum = regValue + immValue;
        regs_.at(destReg) = sum;
        set_nz(sum);
        set_add_cv(regValue, immValue, sum);
    }

    void ARM7TDMI::exec_sub_imm(u16 insn) noexcept {
        const u32 destReg = (insn >> 8U) & 0x7U;
        const u32 immValue = insn & 0xFFU;
        const u32 regValue = regs_.at(destReg);
        const u32 difference = regValue - immValue;
        regs_.at(destReg) = difference;
        set_nz(difference);
        set_sub_cv(regValue, immValue, difference);
    }

    void ARM7TDMI::exec_b(u16 insn) noexcept {
        // imm11 in bits [10:0], target = sign_extend((imm11 << 1))
        constexpr u16 kImm11Mask = 0x07FFU;
        constexpr u32 kAlignShift = 1U;
        const u32 imm11 = insn & kImm11Mask;
        const u32 offset12 = imm11 << kAlignShift;
        const u32 signedOff = sign_extend12(offset12);
        regs_[kRegPC] += signedOff; // PC already advanced by fetch
    }

    // -------------------------- Thumb subset: LDR/STR (word) --------------------------

    // 01001 Rd, [PC, #imm8*4]  — base = (PC_old + 4) aligned to 4
    void ARM7TDMI::exec_ldr_literal(u16 insn) noexcept {
        const u32 destReg = (insn >> 8U) & 0x7U;
        const u32 imm8Words = insn & 0xFFU;
        const u32 offsetBytes = imm8Words << 2U;

        // We fetched at PC_old; step() has advanced PC to PC_old + 2 here.
        const u32 pcAfterFetch = regs_[kRegPC];
        const u32 pcForLdr = (pcAfterFetch + 2U) & kWordMask; // (addr+4) aligned

        const u32 address = pcForLdr + offsetBytes;
        regs_.at(destReg) = read_u32_unaligned(address);
        set_nz(regs_.at(destReg));
    }

    // 01101 LDR Rd, [Rb, #imm5*4]
    void ARM7TDMI::exec_ldr_imm_w(u16 insn) noexcept {
        const u32 imm5Words = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + (imm5Words << 2U);

        regs_.at(destReg) = read_u32_unaligned(address);
        set_nz(regs_.at(destReg));
    }

    // 01100 STR Rd, [Rb, #imm5*4]
    void ARM7TDMI::exec_str_imm_w(u16 insn) noexcept {
        const u32 imm5Words = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + (imm5Words << 2U);

        write_u32_unaligned(address, regs_.at(destReg));
        // flags unaffected
    }

    // -------------------------- fetch/decode/dispatch --------------------------

    void ARM7TDMI::step() noexcept {
        // Fetch Thumb16 at PC, then advance PC by 2
        const u32 fetchAddr = regs_[kRegPC];
        const u16 insn = bus_->read16(fetchAddr);
        regs_[kRegPC] = fetchAddr + 2U;

        // Decode by top 5 bits (mask 0b11111_00000000000)
        const u16 top5 = static_cast<u16>(insn & 0xF800U);

        constexpr u16 kMovImm = 0x2000U;     // 00100
        constexpr u16 kAddImm = 0x3000U;     // 00110
        constexpr u16 kSubImm = 0x3800U;     // 00111
        constexpr u16 kLdrLiteral = 0x4800U; // 01001
        constexpr u16 kStrImmW = 0x6000U;    // 01100
        constexpr u16 kLdrImmW = 0x6800U;    // 01101
        constexpr u16 kBranch = 0xE000U;     // 11100

        if (top5 == kMovImm) {
            exec_mov_imm(insn);
        } else if (top5 == kAddImm) {
            exec_add_imm(insn);
        } else if (top5 == kSubImm) {
            exec_sub_imm(insn);
        } else if (top5 == kLdrLiteral) {
            exec_ldr_literal(insn);
        } else if (top5 == kStrImmW) {
            exec_str_imm_w(insn);
        } else if (top5 == kLdrImmW) {
            exec_ldr_imm_w(insn);
        } else if (top5 == kBranch) {
            exec_b(insn);
        } else {
            // Unknown in this milestone: behave like NOP
        }
    }

} // namespace gba
