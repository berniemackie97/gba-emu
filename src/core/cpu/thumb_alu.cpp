#include "core/cpu/arm7tdmi.h"

#include <bit>
#include <cstdint>

namespace gba {

void ARM7TDMI::exec_alu_register(u16 instruction) noexcept {
    const u32 operation = (instruction >> 6U) & 0xFU;
    const u32 destination = instruction & 7U;
    const u32 source = (instruction >> 3U) & 7U;
    const u32 left = regs_[destination];
    const u32 right = regs_[source];
    const bool carryIn = (cpsr_ & kFlagC) != 0U;
    u32 result = left;
    bool writeResult = true;

    const auto setCarry = [this](bool carry) {
        cpsr_ = (cpsr_ & ~kFlagC) | (carry ? kFlagC : 0U);
    };

    switch (operation) {
    case 0U: result = left & right; break; // AND
    case 1U: result = left ^ right; break; // EOR
    case 2U: { // LSL by register
        const u32 amount = right & 0xFFU;
        if (amount == 0U) {
            break;
        }
        result = amount < 32U ? left << amount : 0U;
        setCarry(amount <= 32U && ((left >> (32U - amount)) & 1U) != 0U);
        break;
    }
    case 3U: { // LSR by register
        const u32 amount = right & 0xFFU;
        if (amount == 0U) {
            break;
        }
        result = amount < 32U ? left >> amount : 0U;
        setCarry(amount <= 32U && ((left >> (amount - 1U)) & 1U) != 0U);
        break;
    }
    case 4U: { // ASR by register
        const u32 amount = right & 0xFFU;
        if (amount == 0U) {
            break;
        }
        const bool negative = (left & kSignBit) != 0U;
        if (amount >= 32U) {
            result = negative ? ~u32{0} : 0U;
            setCarry(negative);
        } else {
            result = (left >> amount) | (negative ? (~u32{0} << (32U - amount)) : 0U);
            setCarry(((left >> (amount - 1U)) & 1U) != 0U);
        }
        break;
    }
    case 5U: { // ADC
        const std::uint64_t sum = static_cast<std::uint64_t>(left) + right + (carryIn ? 1U : 0U);
        result = static_cast<u32>(sum);
        set_add_cv(left, right, result);
        setCarry((sum >> 32U) != 0U);
        break;
    }
    case 6U: { // SBC
        const std::uint64_t subtrahend = static_cast<std::uint64_t>(right) + (carryIn ? 0U : 1U);
        result = left - static_cast<u32>(subtrahend);
        set_sub_cv(left, right, result);
        setCarry(static_cast<std::uint64_t>(left) >= subtrahend);
        break;
    }
    case 7U: { // ROR by register
        const u32 amount = right & 0xFFU;
        if (amount == 0U) {
            break;
        }
        result = std::rotr(left, static_cast<int>(amount & 31U));
        setCarry((result & kSignBit) != 0U);
        break;
    }
    case 8U: result = left & right; writeResult = false; break; // TST
    case 9U: { // NEG
        result = 0U - right;
        set_sub_cv(0U, right, result);
        break;
    }
    case 10U: { // CMP
        result = left - right;
        set_sub_cv(left, right, result);
        writeResult = false;
        break;
    }
    case 11U: { // CMN
        result = left + right;
        set_add_cv(left, right, result);
        writeResult = false;
        break;
    }
    case 12U: result = left | right; break; // ORR
    case 13U: result = left * right; break; // MUL: N/Z defined, C unspecified, V unchanged
    case 14U: result = left & ~right; break; // BIC
    case 15U: result = ~right; break; // MVN
    default: return;
    }

    if (writeResult) {
        regs_[destination] = result;
    }
    set_nz(result);
}

} // namespace gba
