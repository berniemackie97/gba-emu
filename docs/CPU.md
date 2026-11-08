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

The remaining Thumb ALU operations, multiple transfer, long branch
with link and software interrupts still require implementation and
independent conformance tests in issue #19. ARM-state instruction
execution and exception modes remain in issue #20; the complete
opcode coverage and independent oracle gate remains in issue #21.

## Execution failures

The interpreter does not silently treat unimplemented opcodes as NOPs.
`ARM7TDMI::execution_fault()` reports a sticky failure kind together with
the faulting fetch address, 16-bit opcode (when fetched) and CPSR.

- UnsupportedThumb: known unsupported instruction forms, including SWI.
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

Instruction encodings and odd-address rules are cross-checked with
[GBATEK's Thumb instruction formats and ARM CPU memory alignment reference](https://rust-console.github.io/gbatek-gbaonly/).

The R1 roadmap tracks remaining Thumb and ARM instruction coverage and
independent conformance vectors. All tests run through CTest and the
cross-platform quality gate.
