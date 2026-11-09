# ARM7TDMI CPU interpreter

The CPU currently executes a subset of 16-bit Thumb instructions. ARM
instructions, exceptions, timing and many Thumb formats remain unimplemented.
The supported instruction set must not be interpreted as complete game
compatibility.

## PC and control flow

The stored R15 value is the address of the next instruction to fetch.
During Thumb execution, fetching advances it by two bytes. PC-relative
branches use the architectural operand value of the current instruction
address plus four bytes; literal loads align that value down to four bytes.

The signed 8-bit and 11-bit branch displacements are encoded in halfwords.
Both are sign-extended using unsigned modulo arithmetic, so negative
displacements cannot invoke signed-shift or signed-overflow undefined
behavior.

## Stack and status register

Thumb PUSH uses a descending full stack; the lowest selected register is
stored at the lowest address. POP loads registers in ascending order and
advances SP. Optional LR is stored last, and optional PC is loaded last.

ARM7TDMI implements ARMv4T semantics: loading PC using Thumb POP does
not change the T state bit. Only BX and exception transitions can switch
execution state. PUSH/POP and ordinary Thumb LDR/STR/LDRB/STRB variants
must preserve all CPSR condition flags. Arithmetic and compare operations
update their specified condition flags.

## Thumb memory and address instructions

The interpreter supports format 7 (word/byte register-offset transfers),
format 8 (signed byte and halfword register-offset transfers), format 10
(immediate halfword transfers), format 11 (SP-relative word transfers),
format 12 (PC/SP address generation), and format 13 (signed SP adjustment).
Implemented loads and stores preserve all CPSR flags. No instruction-cycle
timing, instruction exceptions or hardware open-bus behavior is implied.

On ARM7TDMI, a word LDR reads a word-aligned address and rotates the
result right by eight times the original address's low two bits.
Word STR ignores the address's low two bits and stores the **unchanged**
source word. Halfword STR also aligns down, while LDRH from an odd
address rotates the loaded halfword right by eight bits within a 32-bit
destination. LDRSH from an odd address reads and sign-extends the byte
at that address. LDRSB always sign-extends its selected byte.

PC-relative address generation uses the current Thumb instruction
address plus four, aligned to four bytes. SP-relative offsets and SP
adjustment use the specified scaled unsigned immediate with 32-bit
wraparound. Relative address arithmetic does not alter NZCV.

All sixteen Thumb register-ALU operation encodings are dispatched, including
register-controlled shifts, ADC/SBC, TST, NEG, CMP/CMN and MUL. The register
shift amount uses the low byte, with separate carry behavior for zero,
32 and larger shift counts. MUL updates N/Z; its C output is
architecturally unspecified and is not used as a conformance assertion.

Format 15 LDMIA/STMIA transfers selected low registers in increasing
address order. With the base in the list, LDM retains the loaded base
instead of applying writeback; STM stores the old base when the base is
the first selected register, and otherwise stores the final base.
The ARMv4T empty-list behavior transfers PC and advances the base by
64 bytes. All these instructions preserve CPSR flags.

Format 19 BL consists of two halfwords. The first computes a signed,
high-offset LR intermediate from instruction address plus four; the
second branches using the low offset and records the Thumb return address
in LR. High-register instructions read PC as instruction address plus four;
high-register writes to PC align to halfwords and preserve Thumb state.
BX uses the same architectural PC operand and rejects reserved encoding bits.

Software interrupt entry still requires the exception-mode implementation
in issue #20. Full opcode inventory and independent reference vectors are
tracked in issue #21. Timing, ARM-state execution, IRQ handling and
CPU mode bank switching remain outside this slice.

## Execution failures

The interpreter does not silently treat unimplemented opcodes as NOPs.
`ARM7TDMI::execution_fault()` reports a sticky failure kind together with
the faulting fetch address, 16-bit opcode (when fetched) and CPSR.

- UnsupportedThumb: known unsupported instruction forms, including SWI pending exception entry.
- UndefinedThumb: invalid conditional-branch encodings and empty register lists.
- UnsupportedArmState: BX entered ARM state; the next step is refused until
  ARM instruction decoding is implemented.
- MissingBus: a step was requested before attaching a bus.

The fault is cleared on CPU reset, and stepping again without reset does
not advance execution. A supported high-register `MOV r8,r8` is a valid,
non-flag-changing no-op.

## Verification

`tests/cpu_thumb_branch.cpp` covers signed branch boundaries, taken and
not-taken conditions, and a generated cartridge instruction sequence.
`tests/cpu_thumb_stack.cpp` covers stack ordering, SP changes, LR push,
PC pop and a generated cartridge return path.
`tests/cpu_thumb_load_flags.cpp` covers full CPSR preservation across
loads/stores and conditional branches that follow loads.
`tests/cpu_thumb_faults.cpp` verifies diagnostic context, undefined
encodings, sticky failures and ARM/Thumb BX transitions.
`tests/cpu_thumb_memory_register.cpp` tests register-offset word/byte
transfers and signed/unaligned halfword behavior with fixed expected values.
`tests/cpu_thumb_memory_address.cpp` checks immediate halfword, SP-relative
word, aligned PC address, signed SP adjustment and a generated cartridge
program.
`tests/cpu_thumb_register_alu.cpp` checks all register ALU opcodes, shift
boundaries, carry/borrow overflow and preservation of condition flags.
`tests/cpu_thumb_multiple_transfer.cpp` verifies LDM/STM order, writeback,
empty lists, base alignment and a generated cartridge program.
`tests/cpu_thumb_long_branch.cpp` verifies BL sign extension and returns,
high-register PC semantics and invalid BX encodings.

Instruction encodings and odd-address rules are cross-checked with
[GBATEK's Thumb instruction formats and ARM CPU memory alignment reference](https://rust-console.github.io/gbatek-gbaonly/).

The R1 roadmap tracks remaining Thumb and ARM instruction coverage and
independent conformance vectors. All tests run through CTest and the
cross-platform quality gate.
