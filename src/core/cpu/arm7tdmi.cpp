// src/core/cpu/arm7tdmi.cpp
#include "core/cpu/arm7tdmi.h"
#include "core/bus/bus.h"

namespace gba {

    // -------------------------- lifecycle --------------------------

    void ARM7TDMI::reset() noexcept {
        regs_.fill(0U);
        cpsr_ = kFlagT;
        fault_ = {};
    }

    void ARM7TDMI::record_fault(FaultKind kind, u32 instructionAddress, u16 instruction) noexcept {
        fault_ = {kind, instructionAddress, instruction, cpsr_};
        regs_[kRegPC] = instructionAddress;
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
        bus_->write32(addr & kWordMask, value);
    }

    // -------------------------- Thumb subset: data-processing --------------------------

    // Format 1: Shift by immediate

    // 00000 LSL Rd, Rs, #imm5 - Logical Shift Left
    void ARM7TDMI::exec_lsl_imm(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 srcValue = regs_.at(srcReg);

        u32 result;
        if (imm5 == 0U) {
            // LSL #0 is a special case - no shift, no carry change
            result = srcValue;
        } else {
            result = srcValue << imm5;
            // Carry = last bit shifted out (bit 32-imm5 of original value)
            const bool carryOut = ((srcValue >> (32U - imm5)) & 0x1U) != 0U;
            if (carryOut) {
                cpsr_ |= kFlagC;
            } else {
                cpsr_ &= ~kFlagC;
            }
        }

        regs_.at(destReg) = result;
        set_nz(result);
        // V unaffected
    }

    // 00001 LSR Rd, Rs, #imm5 - Logical Shift Right
    void ARM7TDMI::exec_lsr_imm(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 srcValue = regs_.at(srcReg);

        u32 result;
        // LSR #0 is encoded as LSR #32
        const u32 shiftAmount = (imm5 == 0U) ? 32U : imm5;

        if (shiftAmount == 32U) {
            result = 0U;
            // Carry = bit 31 of original value
            const bool carryOut = ((srcValue >> 31U) & 0x1U) != 0U;
            if (carryOut) {
                cpsr_ |= kFlagC;
            } else {
                cpsr_ &= ~kFlagC;
            }
        } else {
            result = srcValue >> shiftAmount;
            // Carry = last bit shifted out
            const bool carryOut = ((srcValue >> (shiftAmount - 1U)) & 0x1U) != 0U;
            if (carryOut) {
                cpsr_ |= kFlagC;
            } else {
                cpsr_ &= ~kFlagC;
            }
        }

        regs_.at(destReg) = result;
        set_nz(result);
        // V unaffected
    }

    // 00010 ASR Rd, Rs, #imm5 - Arithmetic Shift Right (sign-extend)
    void ARM7TDMI::exec_asr_imm(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 srcValue = regs_.at(srcReg);

        u32 result;
        // ASR #0 is encoded as ASR #32
        const u32 shiftAmount = (imm5 == 0U) ? 32U : imm5;

        if (shiftAmount >= 32U) {
            // Shift by 32 or more: result is all 0s (positive) or all 1s (negative)
            const bool isNegative = ((srcValue & kSignBit) != 0U);
            result = isNegative ? 0xFFFFFFFFU : 0U;
            // Carry = sign bit
            if (isNegative) {
                cpsr_ |= kFlagC;
            } else {
                cpsr_ &= ~kFlagC;
            }
        } else {
            // Arithmetic shift right: preserve sign bit
            const auto signedSrc = static_cast<std::int32_t>(srcValue);
            result = static_cast<u32>(signedSrc >> shiftAmount);
            // Carry = last bit shifted out
            const bool carryOut = ((srcValue >> (shiftAmount - 1U)) & 0x1U) != 0U;
            if (carryOut) {
                cpsr_ |= kFlagC;
            } else {
                cpsr_ &= ~kFlagC;
            }
        }

        regs_.at(destReg) = result;
        set_nz(result);
        // V unaffected
    }

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

    // 00101 CMP Rd, #imm8 - Compare immediate (like SUB but doesn't store result)
    void ARM7TDMI::exec_cmp_imm(u16 insn) noexcept {
        const u32 reg = (insn >> 8U) & 0x7U;
        const u32 immValue = insn & 0xFFU;
        const u32 regValue = regs_.at(reg);
        const u32 difference = regValue - immValue;
        // Don't store result - only update flags
        set_nz(difference);
        set_sub_cv(regValue, immValue, difference);
    }

    void ARM7TDMI::exec_b(u16 insn) noexcept {
        // imm11 in bits [10:0], target = sign_extend((imm11 << 1))
        const u32 offset12 = static_cast<u32>(insn & kImm11Mask) << 1U;
        regs_[kRegPC] = thumb_pc() + sign_extend<12U>(offset12);
    }

    // -------------------------- Thumb Format 2: Add/Subtract --------------------------

    // 0001100 ADD Rd, Rs, Rn
    void ARM7TDMI::exec_add_reg(u16 insn) noexcept {
        const u32 offsetReg = (insn >> 6U) & 0x7U;  // bits [8:6]
        const u32 srcReg = (insn >> 3U) & 0x7U;     // bits [5:3]
        const u32 destReg = insn & 0x7U;            // bits [2:0]

        const u32 srcValue = regs_.at(srcReg);
        const u32 offsetValue = regs_.at(offsetReg);
        const u32 result = srcValue + offsetValue;

        regs_.at(destReg) = result;
        set_nz(result);
        set_add_cv(srcValue, offsetValue, result);
    }

    // 0001101 SUB Rd, Rs, Rn
    void ARM7TDMI::exec_sub_reg(u16 insn) noexcept {
        const u32 offsetReg = (insn >> 6U) & 0x7U;  // bits [8:6]
        const u32 srcReg = (insn >> 3U) & 0x7U;     // bits [5:3]
        const u32 destReg = insn & 0x7U;            // bits [2:0]

        const u32 srcValue = regs_.at(srcReg);
        const u32 offsetValue = regs_.at(offsetReg);
        const u32 result = srcValue - offsetValue;

        regs_.at(destReg) = result;
        set_nz(result);
        set_sub_cv(srcValue, offsetValue, result);
    }

    // 0001110 ADD Rd, Rs, #imm3
    void ARM7TDMI::exec_add_imm3(u16 insn) noexcept {
        const u32 imm3 = (insn >> 6U) & 0x7U;   // bits [8:6]
        const u32 srcReg = (insn >> 3U) & 0x7U; // bits [5:3]
        const u32 destReg = insn & 0x7U;        // bits [2:0]

        const u32 srcValue = regs_.at(srcReg);
        const u32 result = srcValue + imm3;

        regs_.at(destReg) = result;
        set_nz(result);
        set_add_cv(srcValue, imm3, result);
    }

    // 0001111 SUB Rd, Rs, #imm3
    void ARM7TDMI::exec_sub_imm3(u16 insn) noexcept {
        const u32 imm3 = (insn >> 6U) & 0x7U;   // bits [8:6]
        const u32 srcReg = (insn >> 3U) & 0x7U; // bits [5:3]
        const u32 destReg = insn & 0x7U;        // bits [2:0]

        const u32 srcValue = regs_.at(srcReg);
        const u32 result = srcValue - imm3;

        regs_.at(destReg) = result;
        set_nz(result);
        set_sub_cv(srcValue, imm3, result);
    }

    // -------------------------- Thumb Format 4: ALU operations --------------------------

    // 0100000000 AND Rd, Rs - Bitwise AND
    void ARM7TDMI::exec_and(u16 insn) noexcept {
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 result = regs_.at(destReg) & regs_.at(srcReg);
        regs_.at(destReg) = result;
        set_nz(result);
        // C and V unaffected
    }

    // 0100000001 EOR Rd, Rs - Bitwise Exclusive OR
    void ARM7TDMI::exec_eor(u16 insn) noexcept {
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 result = regs_.at(destReg) ^ regs_.at(srcReg);
        regs_.at(destReg) = result;
        set_nz(result);
        // C and V unaffected
    }

    // 0100001100 ORR Rd, Rs - Bitwise OR
    void ARM7TDMI::exec_orr(u16 insn) noexcept {
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 result = regs_.at(destReg) | regs_.at(srcReg);
        regs_.at(destReg) = result;
        set_nz(result);
        // C and V unaffected
    }

    // 0100001110 BIC Rd, Rs - Bit Clear (AND NOT)
    void ARM7TDMI::exec_bic(u16 insn) noexcept {
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 result = regs_.at(destReg) & ~regs_.at(srcReg);
        regs_.at(destReg) = result;
        set_nz(result);
        // C and V unaffected
    }

    // 0100001111 MVN Rd, Rs - Move NOT
    void ARM7TDMI::exec_mvn(u16 insn) noexcept {
        const u32 srcReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 result = ~regs_.at(srcReg);
        regs_.at(destReg) = result;
        set_nz(result);
        // C and V unaffected
    }

    // -------------------------- Thumb Format 5: High register ops / BX --------------------------

    // 01000100 ADD Rd/Hd, Rs/Hs
    // ARM7TDMI spec: H1 and H2 bits extend the register number to 4 bits (access r8-r15)
    void ARM7TDMI::exec_add_high(u16 insn) noexcept {
        const u32 h1 = (insn >> 7U) & 0x1U; // bit 7: dest is high register
        const u32 h2 = (insn >> 6U) & 0x1U; // bit 6: src is high register
        const u32 destRegLow = insn & 0x7U;
        const u32 srcRegLow = (insn >> 3U) & 0x7U;

        const u32 destReg = destRegLow | (h1 << 3U); // 0..15
        const u32 srcReg = srcRegLow | (h2 << 3U);   // 0..15

        const u32 destValue = regs_.at(destReg);
        const u32 srcValue = regs_.at(srcReg);
        const u32 result = destValue + srcValue;

        regs_.at(destReg) = result;
        // Flags are NOT affected by ADD (high register form)
    }

    // 01000101 CMP Rd/Hd, Rs/Hs
    void ARM7TDMI::exec_cmp_high(u16 insn) noexcept {
        const u32 h1 = (insn >> 7U) & 0x1U;
        const u32 h2 = (insn >> 6U) & 0x1U;
        const u32 destRegLow = insn & 0x7U;
        const u32 srcRegLow = (insn >> 3U) & 0x7U;

        const u32 destReg = destRegLow | (h1 << 3U);
        const u32 srcReg = srcRegLow | (h2 << 3U);

        const u32 destValue = regs_.at(destReg);
        const u32 srcValue = regs_.at(srcReg);
        const u32 result = destValue - srcValue;

        // CMP always updates flags
        set_nz(result);
        set_sub_cv(destValue, srcValue, result);
    }

    // 01000110 MOV Rd/Hd, Rs/Hs
    void ARM7TDMI::exec_mov_high(u16 insn) noexcept {
        const u32 h1 = (insn >> 7U) & 0x1U;
        const u32 h2 = (insn >> 6U) & 0x1U;
        const u32 destRegLow = insn & 0x7U;
        const u32 srcRegLow = (insn >> 3U) & 0x7U;

        const u32 destReg = destRegLow | (h1 << 3U);
        const u32 srcReg = srcRegLow | (h2 << 3U);

        regs_.at(destReg) = regs_.at(srcReg);
        // Flags are NOT affected by MOV (high register form)
    }

    // 01000111 BX Rs/Hs (Branch and Exchange)
    // ARM7TDMI spec: BX switches between ARM and Thumb state based on bit 0 of the target address
    void ARM7TDMI::exec_bx(u16 insn) noexcept {
        const u32 h2 = (insn >> 6U) & 0x1U;
        const u32 srcRegLow = (insn >> 3U) & 0x7U;
        const u32 srcReg = srcRegLow | (h2 << 3U);

        const u32 targetAddr = regs_.at(srcReg);

        // Bit 0 of target determines new state: 0=ARM, 1=Thumb
        if ((targetAddr & 0x1U) != 0U) {
            // Stay in Thumb state
            cpsr_ |= kFlagT;
            regs_[kRegPC] = targetAddr & ~0x1U; // Clear bit 0, keep halfword aligned
        } else {
            cpsr_ &= ~kFlagT;
            regs_[kRegPC] = targetAddr & ~u32{3};
        }
    }

    // -------------------------- Thumb subset: LDR/STR (word) --------------------------

    // 01001 Rd, [PC, #imm8*4]  — base = (PC_old + 4) aligned to 4
    void ARM7TDMI::exec_ldr_literal(u16 insn) noexcept {
        const u32 destReg = (insn >> 8U) & 0x7U;
        const u32 imm8Words = insn & 0xFFU;
        const u32 offsetBytes = imm8Words << 2U;

        // We fetched at PC_old; step() has advanced PC to PC_old + 2 here.
        const u32 pcForLdr = thumb_pc() & kWordMask;

        const u32 address = pcForLdr + offsetBytes;
        regs_.at(destReg) = read_u32_unaligned(address);
    }

    // 01101 LDR Rd, [Rb, #imm5*4]
    void ARM7TDMI::exec_ldr_imm_w(u16 insn) noexcept {
        const u32 imm5Words = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + (imm5Words << 2U);

        regs_.at(destReg) = read_u32_unaligned(address);
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

    // 01111 LDRB Rd, [Rb, #imm5]
    void ARM7TDMI::exec_ldr_imm_b(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + imm5;

        regs_.at(destReg) = static_cast<u32>(bus_->read8(address));
    }

    // 01110 STRB Rd, [Rb, #imm5]
    void ARM7TDMI::exec_str_imm_b(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 srcReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + imm5;

        bus_->write8(address, static_cast<u8>(regs_.at(srcReg) & 0xFFU));
        // flags unaffected
    }

    // 1011010 PUSH {Rlist}{LR}
    // ARM7TDMI spec: Store registers to stack (full descending)
    // Bit 8 (R bit): 1 = also push LR
    // Bits 0-7: register list (R0-R7)
    // SP is decremented first, then registers stored in ascending order
    void ARM7TDMI::exec_push(u16 insn) noexcept {
        const u32 rlist = insn & 0xFFU;          // bits 0-7: R0-R7
        const bool pushLR = ((insn >> 8U) & 0x1U) != 0U; // bit 8: push LR (R14)

        u32 sp = regs_[kRegSP];

        // Count how many registers to push
        u32 count = 0;
        for (u32 i = 0; i < 8U; ++i) {
            if ((rlist & (1U << i)) != 0U) {
                ++count;
            }
        }
        if (pushLR) {
            ++count;
        }

        // Decrement SP by 4 * count (full descending stack)
        sp -= count * 4U;
        u32 addr = sp;

        // Store registers in ascending order (R0 first, then R1, ..., then LR if needed)
        for (u32 i = 0; i < 8U; ++i) {
            if ((rlist & (1U << i)) != 0U) {
                bus_->write32(addr, regs_.at(i));
                addr += 4U;
            }
        }
        if (pushLR) {
            bus_->write32(addr, regs_[kRegLR]);
        }

        // Update SP
        regs_[kRegSP] = sp;
        // flags unaffected
    }

    // 1011110 POP {Rlist}{PC}
    // ARM7TDMI spec: Load registers from stack (full descending)
    // Bit 8 (R bit): 1 = also pop PC (causes branch)
    // Bits 0-7: register list (R0-R7)
    // Registers loaded in ascending order, then SP incremented
    void ARM7TDMI::exec_pop(u16 insn) noexcept {
        const u32 rlist = insn & 0xFFU;          // bits 0-7: R0-R7
        const bool popPC = ((insn >> 8U) & 0x1U) != 0U; // bit 8: pop PC (R15)

        u32 addr = regs_[kRegSP];

        // Load registers in ascending order
        for (u32 i = 0; i < 8U; ++i) {
            if ((rlist & (1U << i)) != 0U) {
                regs_.at(i) = bus_->read32(addr);
                addr += 4U;
            }
        }
        if (popPC) {
            // Pop PC causes branch - load address and set PC
            const u32 targetAddr = bus_->read32(addr);
            addr += 4U;

            regs_[kRegPC] = targetAddr & ~u32{1};
        }

        // Update SP
        regs_[kRegSP] = addr;
        // flags unaffected
    }

    // 1101 cond imm8 - Conditional branch
    // ARM7TDMI spec: Branch if condition is met
    // Bits 8-11: condition code
    // Bits 0-7: signed 8-bit offset (shifted left by 1 for halfword alignment)
    void ARM7TDMI::exec_bcond(u16 insn) noexcept {
        const u32 cond = (insn >> 8U) & 0xFU;    // bits 8-11: condition
        const u32 imm8 = insn & 0xFFU;           // bits 0-7: offset

        const u32 signedOffset = sign_extend<9U>(imm8 << 1U);

        // Evaluate condition based on CPSR flags
        bool conditionMet = false;
        const bool N = (cpsr_ & kFlagN) != 0U;
        const bool Z = (cpsr_ & kFlagZ) != 0U;
        const bool C = (cpsr_ & kFlagC) != 0U;
        const bool V = (cpsr_ & kFlagV) != 0U;

        // ARM7TDMI condition codes
        switch (cond) {
            case 0x0U: conditionMet = Z;                break; // BEQ: Z set (equal)
            case 0x1U: conditionMet = !Z;               break; // BNE: Z clear (not equal)
            case 0x2U: conditionMet = C;                break; // BCS/BHS: C set (unsigned >=)
            case 0x3U: conditionMet = !C;               break; // BCC/BLO: C clear (unsigned <)
            case 0x4U: conditionMet = N;                break; // BMI: N set (negative)
            case 0x5U: conditionMet = !N;               break; // BPL: N clear (positive or zero)
            case 0x6U: conditionMet = V;                break; // BVS: V set (overflow)
            case 0x7U: conditionMet = !V;               break; // BVC: V clear (no overflow)
            case 0x8U: conditionMet = C && !Z;          break; // BHI: C set AND Z clear (unsigned >)
            case 0x9U: conditionMet = !C || Z;          break; // BLS: C clear OR Z set (unsigned <=)
            case 0xAU: conditionMet = (N == V);         break; // BGE: N == V (signed >=)
            case 0xBU: conditionMet = (N != V);         break; // BLT: N != V (signed <)
            case 0xCU: conditionMet = !Z && (N == V);   break; // BGT: Z clear AND N == V (signed >)
            case 0xDU: conditionMet = Z || (N != V);    break; // BLE: Z set OR N != V (signed <=)
            default: return;
        }

        // Branch if condition met
        if (conditionMet) {
            // PC was already advanced by fetch, so offset is relative to current PC
            regs_[kRegPC] = thumb_pc() + signedOffset;
        }
        // flags unaffected
    }

    auto ARM7TDMI::read_u16_unaligned(u32 address) const noexcept -> u32 {
        const u32 halfword = bus_->read16(address & ~u32{1});
        if ((address & 1U) == 0U) {
            return halfword;
        }
        return (halfword >> 8U) | (halfword << 24U);
    }

    void ARM7TDMI::exec_memory_register(u16 instruction) noexcept {
        const u32 offsetRegister = (instruction >> 6U) & kLow3Mask;
        const u32 baseRegister = (instruction >> 3U) & kLow3Mask;
        const u32 dataRegister = instruction & kLow3Mask;
        const u32 address = regs_.at(baseRegister) + regs_.at(offsetRegister);
        const u32 operation = (instruction >> 10U) & 3U;

        if ((instruction & 0x0200U) == 0U) {
            switch (operation) {
            case 0U:
                write_u32_unaligned(address, regs_.at(dataRegister));
                break;
            case 1U:
                bus_->write8(address, static_cast<u8>(regs_.at(dataRegister)));
                break;
            case 2U:
                regs_.at(dataRegister) = read_u32_unaligned(address);
                break;
            case 3U:
                regs_.at(dataRegister) = bus_->read8(address);
                break;
            default:
                break;
            }
            return;
        }

        switch (operation) {
        case 0U:
            bus_->write16(address & ~u32{1}, static_cast<u16>(regs_.at(dataRegister)));
            break;
        case 1U:
            regs_.at(dataRegister) = sign_extend<8U>(bus_->read8(address));
            break;
        case 2U:
            regs_.at(dataRegister) = read_u16_unaligned(address);
            break;
        case 3U:
            regs_.at(dataRegister) = (address & 1U) != 0U
                ? sign_extend<8U>(bus_->read8(address))
                : sign_extend<16U>(bus_->read16(address));
            break;
        default:
            break;
        }
    }

    void ARM7TDMI::exec_memory_halfword(u16 instruction) noexcept {
        const u32 offset = ((instruction >> 6U) & 0x1FU) << 1U;
        const u32 baseRegister = (instruction >> 3U) & kLow3Mask;
        const u32 dataRegister = instruction & kLow3Mask;
        const u32 address = regs_.at(baseRegister) + offset;
        if ((instruction & 0x0800U) != 0U) {
            regs_.at(dataRegister) = read_u16_unaligned(address);
        } else {
            bus_->write16(address & ~u32{1}, static_cast<u16>(regs_.at(dataRegister)));
        }
    }

    void ARM7TDMI::exec_memory_stack(u16 instruction) noexcept {
        const u32 dataRegister = (instruction >> 8U) & kLow3Mask;
        const u32 address = regs_[kRegSP] + ((instruction & 0xFFU) << 2U);
        if ((instruction & 0x0800U) != 0U) {
            regs_.at(dataRegister) = read_u32_unaligned(address);
        } else {
            write_u32_unaligned(address, regs_.at(dataRegister));
        }
    }

    void ARM7TDMI::exec_relative_address(u16 instruction) noexcept {
        const u32 dataRegister = (instruction >> 8U) & kLow3Mask;
        const u32 base = (instruction & 0x0800U) != 0U
            ? regs_[kRegSP]
            : (thumb_pc() & kWordMask);
        regs_.at(dataRegister) = base + ((instruction & 0xFFU) << 2U);
    }

    void ARM7TDMI::exec_adjust_stack(u16 instruction) noexcept {
        const u32 offset = (instruction & 0x7FU) << 2U;
        if ((instruction & 0x80U) != 0U) {
            regs_[kRegSP] -= offset;
        } else {
            regs_[kRegSP] += offset;
        }
    }

    // -------------------------- fetch/decode/dispatch --------------------------

    void ARM7TDMI::step() noexcept {
        if (fault_.kind != FaultKind::None) {
            return;
        }

        const u32 fetchAddr = regs_[kRegPC];
        if (bus_ == nullptr) {
            record_fault(FaultKind::MissingBus, fetchAddr, 0U);
            return;
        }
        if ((cpsr_ & kFlagT) == 0U) {
            record_fault(FaultKind::UnsupportedArmState, fetchAddr, 0U);
            return;
        }

        const u16 instruction = bus_->read16(fetchAddr);
        regs_[kRegPC] = fetchAddr + 2U;
        execute_thumb(instruction, fetchAddr);
    }

    void ARM7TDMI::execute_thumb(u16 instruction, u32 instructionAddress) noexcept {
        constexpr u16 kStackMask = 0xFE00U;
        const u16 stackForm = instruction & kStackMask;
        if (stackForm == 0xB400U || stackForm == 0xBC00U) {
            if ((instruction & 0x1FFU) == 0U) {
                record_fault(FaultKind::UndefinedThumb, instructionAddress, instruction);
            } else if (stackForm == 0xB400U) {
                exec_push(instruction);
            } else {
                exec_pop(instruction);
            }
            return;
        }

        if ((instruction & 0xF000U) == 0xD000U) {
            const u32 condition = (instruction >> 8U) & 0xFU;
            if (condition == 0xEU) {
                record_fault(FaultKind::UndefinedThumb, instructionAddress, instruction);
            } else if (condition == 0xFU) {
                record_fault(FaultKind::UnsupportedThumb, instructionAddress, instruction);
            } else {
                exec_bcond(instruction);
            }
            return;
        }

        struct DecoderEntry {
            u16 mask;
            u16 pattern;
            void (ARM7TDMI::*handler)(u16) noexcept;
        };
        static constexpr std::array<DecoderEntry, 31> kDecoder{{
            {0xFFC0U, 0x4000U, &ARM7TDMI::exec_and},
            {0xFFC0U, 0x4040U, &ARM7TDMI::exec_eor},
            {0xFFC0U, 0x4300U, &ARM7TDMI::exec_orr},
            {0xFFC0U, 0x4380U, &ARM7TDMI::exec_bic},
            {0xFFC0U, 0x43C0U, &ARM7TDMI::exec_mvn},
            {0xFF00U, 0x4400U, &ARM7TDMI::exec_add_high},
            {0xFF00U, 0x4500U, &ARM7TDMI::exec_cmp_high},
            {0xFF00U, 0x4600U, &ARM7TDMI::exec_mov_high},
            {0xFF00U, 0x4700U, &ARM7TDMI::exec_bx},
            {0xFE00U, 0x1800U, &ARM7TDMI::exec_add_reg},
            {0xFE00U, 0x1A00U, &ARM7TDMI::exec_sub_reg},
            {0xFE00U, 0x1C00U, &ARM7TDMI::exec_add_imm3},
            {0xFE00U, 0x1E00U, &ARM7TDMI::exec_sub_imm3},
            {0xF800U, 0x2000U, &ARM7TDMI::exec_mov_imm},
            {0xF800U, 0x2800U, &ARM7TDMI::exec_cmp_imm},
            {0xF800U, 0x3000U, &ARM7TDMI::exec_add_imm},
            {0xF800U, 0x3800U, &ARM7TDMI::exec_sub_imm},
            {0xF800U, 0x4800U, &ARM7TDMI::exec_ldr_literal},
            {0xF000U, 0x5000U, &ARM7TDMI::exec_memory_register},
            {0xF000U, 0x8000U, &ARM7TDMI::exec_memory_halfword},
            {0xF000U, 0x9000U, &ARM7TDMI::exec_memory_stack},
            {0xF000U, 0xA000U, &ARM7TDMI::exec_relative_address},
            {0xFF00U, 0xB000U, &ARM7TDMI::exec_adjust_stack},
            {0xF800U, 0x6000U, &ARM7TDMI::exec_str_imm_w},
            {0xF800U, 0x6800U, &ARM7TDMI::exec_ldr_imm_w},
            {0xF800U, 0x7000U, &ARM7TDMI::exec_str_imm_b},
            {0xF800U, 0x7800U, &ARM7TDMI::exec_ldr_imm_b},
            {0xF800U, 0xE000U, &ARM7TDMI::exec_b},
            {0xF800U, 0x0000U, &ARM7TDMI::exec_lsl_imm},
            {0xF800U, 0x0800U, &ARM7TDMI::exec_lsr_imm},
            {0xF800U, 0x1000U, &ARM7TDMI::exec_asr_imm}
        }};

        for (const auto &entry : kDecoder) {
            if ((instruction & entry.mask) == entry.pattern) {
                (this->*entry.handler)(instruction);
                return;
            }
        }
        record_fault(FaultKind::UnsupportedThumb, instructionAddress, instruction);
    }

} // namespace gba
