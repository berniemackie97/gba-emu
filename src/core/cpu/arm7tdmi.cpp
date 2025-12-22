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
            // Switch to ARM state (not yet implemented)
            // For now, we'll just branch and clear T flag
            cpsr_ &= ~kFlagT;
            regs_[kRegPC] = targetAddr & ~0x3U; // Word aligned for ARM
            // TODO: ARM mode execution not yet implemented
        }
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

    // 01111 LDRB Rd, [Rb, #imm5]
    void ARM7TDMI::exec_ldr_imm_b(u16 insn) noexcept {
        const u32 imm5 = (insn >> 6U) & 0x1FU;
        const u32 baseReg = (insn >> 3U) & 0x7U;
        const u32 destReg = insn & 0x7U;
        const u32 address = regs_.at(baseReg) + imm5;

        regs_.at(destReg) = static_cast<u32>(bus_->read8(address));
        set_nz(regs_.at(destReg)); // C/V unaffected
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

            // Bit 0 determines Thumb/ARM state (should always be 1 for Thumb)
            if ((targetAddr & 0x1U) != 0U) {
                // Stay in Thumb - clear bit 0 for halfword alignment
                regs_[kRegPC] = targetAddr & ~0x1U;
                cpsr_ |= kFlagT;
            } else {
                // Switch to ARM mode (not yet implemented, but set PC anyway)
                regs_[kRegPC] = targetAddr & ~0x3U; // word aligned
                cpsr_ &= ~kFlagT;
            }
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

        // Sign-extend 8-bit to 32-bit, then shift left by 1 (halfword offset)
        const auto signedOff8 = static_cast<std::int8_t>(imm8);
        const auto signedOff = static_cast<std::int32_t>(signedOff8) << 1;

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
            case 0xEU: conditionMet = true;             break; // BAL: always (undefined in Thumb)
            case 0xFU: conditionMet = false;            break; // Reserved (SWI in some docs)
            default: break;
        }

        // Branch if condition met
        if (conditionMet) {
            // PC was already advanced by fetch, so offset is relative to current PC
            regs_[kRegPC] = static_cast<u32>(static_cast<std::int32_t>(regs_[kRegPC]) + signedOff);
        }
        // flags unaffected
    }

    // -------------------------- fetch/decode/dispatch --------------------------

    void ARM7TDMI::step() noexcept {
        // Fetch Thumb16 at PC, then advance PC by 2
        const u32 fetchAddr = regs_[kRegPC];
        const u16 insn = bus_->read16(fetchAddr);
        regs_[kRegPC] = fetchAddr + 2U;

        // Decode by top 5 bits (mask 0b11111_00000000000) for most formats
        const u16 top5 = static_cast<u16>(insn & 0xF800U);
        // For Format 2, we need top 7 bits (0b0001_1xx_xxxxxxxxx)
        const u16 top7 = static_cast<u16>(insn & 0xFE00U);
        // For Format 5, we need top 8 bits (0b01000_1xx_xxxxxxxx)
        const u16 top8 = static_cast<u16>(insn & 0xFF00U);
        // For Format 14 (PUSH/POP), we need top 7 bits + bit 3 (0b1011_x10_xxxxxxxxx)
        const u16 top7_pushpop = static_cast<u16>(insn & 0xF600U);
        // For Format 16 (Bcond), we need top 4 bits (0b1101_xxxx_xxxxxxxx)
        const u16 top4 = static_cast<u16>(insn & 0xF000U);

        constexpr u16 kMovImm = 0x2000U;     // 00100
        constexpr u16 kAddImm = 0x3000U;     // 00110
        constexpr u16 kSubImm = 0x3800U;     // 00111
        constexpr u16 kLdrLiteral = 0x4800U; // 01001
        constexpr u16 kStrImmW = 0x6000U;    // 01100
        constexpr u16 kLdrImmW = 0x6800U;    // 01101
        constexpr u16 kStrImmB = 0x7000U;    // 01110
        constexpr u16 kLdrImmB = 0x7800U;    // 01111
        constexpr u16 kBranch = 0xE000U;     // 11100

        // Format 2: Add/subtract (7-bit decode)
        constexpr u16 kAddReg = 0x1800U;    // 0001100
        constexpr u16 kSubReg = 0x1A00U;    // 0001101
        constexpr u16 kAddImm3 = 0x1C00U;   // 0001110
        constexpr u16 kSubImm3 = 0x1E00U;   // 0001111

        // Format 5: High register operations / BX (8-bit decode)
        constexpr u16 kAddHigh = 0x4400U;   // 01000100
        constexpr u16 kCmpHigh = 0x4500U;   // 01000101
        constexpr u16 kMovHigh = 0x4600U;   // 01000110
        constexpr u16 kBX = 0x4700U;        // 01000111

        // Format 14: PUSH/POP (top 7 bits + bit 3)
        constexpr u16 kPush = 0xB400U;      // 1011010
        constexpr u16 kPop = 0xB600U;       // 1011110 (note: bit 3 is set for POP)

        // Format 16: Conditional branch (top 4 bits)
        constexpr u16 kBCond = 0xD000U;     // 1101

        // Check Format 5 first (8-bit match) - most specific
        if (top8 == kAddHigh) {
            exec_add_high(insn);
        } else if (top8 == kCmpHigh) {
            exec_cmp_high(insn);
        } else if (top8 == kMovHigh) {
            exec_mov_high(insn);
        } else if (top8 == kBX) {
            exec_bx(insn);
        } else if (top7_pushpop == kPush) {
            exec_push(insn);
        } else if (top7_pushpop == kPop) {
            exec_pop(insn);
        } else if (top7 == kAddReg) {
            exec_add_reg(insn);
        } else if (top7 == kSubReg) {
            exec_sub_reg(insn);
        } else if (top7 == kAddImm3) {
            exec_add_imm3(insn);
        } else if (top7 == kSubImm3) {
            exec_sub_imm3(insn);
        } else if (top5 == kMovImm) {
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
        } else if (top5 == kStrImmB) {
            exec_str_imm_b(insn);
        } else if (top5 == kLdrImmB) {
            exec_ldr_imm_b(insn);
        } else if (top4 == kBCond) {
            exec_bcond(insn);
        } else if (top5 == kBranch) {
            exec_b(insn);
        } else {
            // NOP for unimplemented in this milestone
        }
    }

} // namespace gba
