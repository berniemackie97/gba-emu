#include "core/cpu/arm7tdmi.h"

#include <cstdint>

namespace gba {

    auto ARM7TDMI::arm_register(u32 index, bool shiftedRm) const noexcept -> u32 {
        return index == kRegPC ? regs_[kRegPC] + (shiftedRm ? 8U : 4U) : regs_[index];
    }

    auto ARM7TDMI::arm_condition_passed(u32 condition) const noexcept -> bool {
        const bool negative = (cpsr_ & kFlagN) != 0U;
        const bool zero = (cpsr_ & kFlagZ) != 0U;
        const bool carry = (cpsr_ & kFlagC) != 0U;
        const bool overflow = (cpsr_ & kFlagV) != 0U;
        switch (condition) {
        case 0U: return zero;
        case 1U: return !zero;
        case 2U: return carry;
        case 3U: return !carry;
        case 4U: return negative;
        case 5U: return !negative;
        case 6U: return overflow;
        case 7U: return !overflow;
        case 8U: return carry && !zero;
        case 9U: return !carry || zero;
        case 10U: return negative == overflow;
        case 11U: return negative != overflow;
        case 12U: return !zero && negative == overflow;
        case 13U: return zero || negative != overflow;
        case 14U: return true;
        default: return false;
        }
    }

    auto ARM7TDMI::add_with_carry(u32 left, u32 right, bool carry) noexcept -> ArithmeticResult {
        const std::uint64_t wide = static_cast<std::uint64_t>(left) + right + (carry ? 1U : 0U);
        const u32 value = static_cast<u32>(wide);
        return {value, (wide >> 32U) != 0U,
                ((~(left ^ right) & (left ^ value)) & kSignBit) != 0U};
    }

    void ARM7TDMI::execute_arm(u32 instruction, u32 instructionAddress) noexcept {
        if (!arm_condition_passed(instruction >> 28U)) {
            return;
        }

        if ((instruction & 0x0FFFFFF0U) == 0x012FFF10U) {
            const u32 target = arm_register(instruction & 15U);
            if ((target & 1U) != 0U) {
                cpsr_ |= kFlagT;
                regs_[kRegPC] = target & ~u32{1};
            } else {
                cpsr_ &= ~kFlagT;
                regs_[kRegPC] = target & kWordMask;
            }
            return;
        }

        if ((instruction & 0x0F8000F0U) == 0x00800090U ||
            (instruction & 0x0FC000F0U) == 0x00000090U) {
            exec_arm_multiply(instruction, instructionAddress);
            return;
        }

        if ((instruction & 0x0E000000U) == 0x0A000000U) {
            const u32 offset = sign_extend<26U>((instruction & 0x00FFFFFFU) << 2U);
            if ((instruction & 0x01000000U) != 0U) {
                regs_[kRegLR] = instructionAddress + 4U;
            }
            regs_[kRegPC] = instructionAddress + 8U + offset;
            return;
        }

        if ((instruction & 0x0E000000U) == 0x08000000U) {
            exec_arm_block_transfer(instruction, instructionAddress);
            return;
        }
        if ((instruction & 0x0C000000U) == 0x04000000U) {
            exec_arm_single_transfer(instruction, instructionAddress);
            return;
        }

        if ((instruction & 0x0C000000U) == 0U) {
            if ((instruction & 0x0FBF0FFFU) == 0x010F0000U ||
                (instruction & 0x0DB0F000U) == 0x0120F000U ||
                (instruction & 0x0DB0F000U) == 0x0320F000U ||
                ((instruction & 0x0E000090U) == 0x00000090U)) {
                record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
                return;
            }
            exec_arm_data_processing(instruction, instructionAddress);
            return;
        }

        record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
    }

    void ARM7TDMI::exec_arm_data_processing(u32 instruction, u32 instructionAddress) noexcept {
        const u32 operation = (instruction >> 21U) & 15U;
        const bool setFlags = (instruction & 0x00100000U) != 0U;
        const bool compare = operation >= 8U && operation <= 11U;
        const u32 destination = (instruction >> 12U) & 15U;
        if ((compare && !setFlags) || (destination == kRegPC && setFlags && !compare)) {
            record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
            return;
        }
        const u32 left = arm_register((instruction >> 16U) & 15U);
        const ArmOperand right = arm_operand2(instruction);
        const bool carryIn = (cpsr_ & kFlagC) != 0U;
        u32 result = 0U;
        bool carry = right.carry;
        bool overflow = (cpsr_ & kFlagV) != 0U;
        bool arithmeticFlags = false;
        auto compute = [&](u32 a, u32 b, bool cin) {
            const ArithmeticResult values = add_with_carry(a, b, cin);
            result = values.value;
            carry = values.carry;
            overflow = values.overflow;
            arithmeticFlags = true;
        };
        switch (operation) {
        case 0U: result = left & right.value; break;
        case 1U: result = left ^ right.value; break;
        case 2U: compute(left, ~right.value, true); break;
        case 3U: compute(right.value, ~left, true); break;
        case 4U: compute(left, right.value, false); break;
        case 5U: compute(left, right.value, carryIn); break;
        case 6U: compute(left, ~right.value, carryIn); break;
        case 7U: compute(right.value, ~left, carryIn); break;
        case 8U: result = left & right.value; break;
        case 9U: result = left ^ right.value; break;
        case 10U: compute(left, ~right.value, true); break;
        case 11U: compute(left, right.value, false); break;
        case 12U: result = left | right.value; break;
        case 13U: result = right.value; break;
        case 14U: result = left & ~right.value; break;
        case 15U: result = ~right.value; break;
        default: return;
        }

        if (setFlags) {
            set_nz(result);
            cpsr_ = (cpsr_ & ~kFlagC) | (carry ? kFlagC : 0U);
            if (arithmeticFlags) {
                cpsr_ = (cpsr_ & ~kFlagV) | (overflow ? kFlagV : 0U);
            }
        }
        if (!compare) {
            regs_[destination] = destination == kRegPC ? result & kWordMask : result;
        }
    }

    void ARM7TDMI::exec_arm_multiply(u32 instruction, u32 instructionAddress) noexcept {
        const bool longResult = (instruction & 0x00800000U) != 0U;
        const bool setFlags = (instruction & 0x00100000U) != 0U;
        const u32 rm = instruction & 15U;
        const u32 rs = (instruction >> 8U) & 15U;
        const u32 rd = (instruction >> 16U) & 15U;
        const u32 rn = (instruction >> 12U) & 15U;

        if (rm == kRegPC || rs == kRegPC || rd == kRegPC ||
            (longResult && rn == kRegPC)) {
            record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
            return;
        }

        if (!longResult) {
            if ((instruction & 0x00400000U) == 0U && rn != 0U) {
                record_fault(FaultKind::UndefinedArm, instructionAddress, instruction);
                return;
            }
            const u32 result = regs_[rm] * regs_[rs] +
                (((instruction & 0x00200000U) != 0U) ? regs_[rn] : 0U);
            regs_[rd] = result;
            if (setFlags) {
                set_nz(result);
            }
            return;
        }

        std::uint64_t result = (instruction & 0x00400000U) != 0U
            ? static_cast<std::uint64_t>(
                static_cast<std::int64_t>(static_cast<std::int32_t>(regs_[rm])) *
                static_cast<std::int64_t>(static_cast<std::int32_t>(regs_[rs])))
            : static_cast<std::uint64_t>(regs_[rm]) * regs_[rs];
        if ((instruction & 0x00200000U) != 0U) {
            result += (static_cast<std::uint64_t>(regs_[rd]) << 32U) | regs_[rn];
        }
        regs_[rn] = static_cast<u32>(result);
        regs_[rd] = static_cast<u32>(result >> 32U);
        if (setFlags) {
            cpsr_ = (cpsr_ & ~(kFlagN | kFlagZ)) |
                ((result & (std::uint64_t{1} << 63U)) != 0U ? kFlagN : 0U) |
                (result == 0U ? kFlagZ : 0U);
        }
    }

} // namespace gba
