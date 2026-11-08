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

The R1 roadmap tracks remaining Thumb and ARM instruction coverage and
independent conformance vectors. All tests run through CTest and the
cross-platform quality gate.
