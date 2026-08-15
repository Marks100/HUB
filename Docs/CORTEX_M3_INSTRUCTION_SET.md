# Cortex-M3 Instruction Set Reference

Complete list of instructions supported by the ARM Cortex-M3, the core used by this project's
STM32F103 target.

The Cortex-M3 implements the **ARMv7-M** architecture profile and executes **Thumb-2 only**.
There is no ARM (A32) state, no FPU, and no DSP/SIMD extension. Every instruction below is either
a 16-bit or a 32-bit Thumb encoding; the assembler picks the narrow form automatically where one
exists, which is why `.n` / `.w` suffixes occasionally appear in disassembly.

This project builds with:

```
-mcpu=cortex-m3 -mthumb
```

with no `-mfloat-abi`, so all floating point is soft-float library calls. See [rules.mk](../rules.mk).

---

## Contents

- [Notation](#notation)
- [Branch and control flow](#branch-and-control-flow)
- [Data movement](#data-movement)
- [Arithmetic](#arithmetic)
- [Multiply and divide](#multiply-and-divide)
- [Logical](#logical)
- [Shift and rotate](#shift-and-rotate)
- [Bit field and bit manipulation](#bit-field-and-bit-manipulation)
- [Sign and zero extension](#sign-and-zero-extension)
- [Saturation](#saturation)
- [Load and store, single](#load-and-store-single)
- [Load and store, multiple](#load-and-store-multiple)
- [Exclusive access](#exclusive-access)
- [System and hint](#system-and-hint)
- [Memory barriers](#memory-barriers)
- [Addressing modes](#addressing-modes)
- [Conditional execution and IT blocks](#conditional-execution-and-it-blocks)
- [Condition codes](#condition-codes)
- [What the Cortex-M3 does NOT have](#what-the-cortex-m3-does-not-have)
- [Instruction timings](#instruction-timings)
- [Practical notes](#practical-notes)

---

## Notation

| Symbol | Meaning |
|---|---|
| `Rd` | Destination register |
| `Rn`, `Rm` | Source registers |
| `Ra` | Accumulate register |
| `RdLo`, `RdHi` | Low / high halves of a 64-bit result |
| `#imm` | Immediate constant |
| `{S}` | Optional suffix — update the condition flags (N, Z, C, V) |
| `{cond}` | Optional condition code; requires an enclosing `IT` block except on `B` |
| `Operand2` | A flexible second operand: register, register with shift, or modified immediate |
| `16` / `32` | Available encoding widths in bits |

**Low registers** are R0–R7; most 16-bit encodings can only reach these. **High registers** are
R8–R12. R13 = SP, R14 = LR, R15 = PC.

---

## Branch and control flow

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `B` | `B{cond} label` | 16, 32 | Branch. Ranges: ±256 B (16-bit cond), ±2 KB (16-bit uncond), ±1 MB (32-bit cond), ±16 MB (32-bit uncond) |
| `BL` | `BL label` | 32 | Branch with link. Writes return address to LR. ±16 MB |
| `BX` | `BX Rm` | 16 | Branch and exchange to address in `Rm`. Bit[0] must be 1 (Thumb) |
| `BLX` | `BLX Rm` | 16 | Branch with link and exchange, register form only |
| `CBZ` | `CBZ Rn, label` | 16 | Compare and branch if zero. **Forward only**, +4 to +130 bytes, `Rn` must be R0–R7. Does not affect flags |
| `CBNZ` | `CBNZ Rn, label` | 16 | Compare and branch if non-zero. Same restrictions as `CBZ` |
| `TBB` | `TBB [Rn, Rm]` | 32 | Table branch, byte offsets. Forward only |
| `TBH` | `TBH [Rn, Rm, LSL #1]` | 32 | Table branch, halfword offsets. Forward only |
| `IT` | `IT{x{y{z}}} cond` | 16 | If-Then. Makes the next 1–4 instructions conditional |

> `BLX #imm` (immediate form) is **not** available — it would require a switch to ARM state.
> `BX` to an address with bit[0] clear raises a UsageFault (INVSTATE).

---

## Data movement

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `MOV` | `MOV{S}{cond} Rd, Operand2` | 16, 32 | Move register or modified immediate |
| `MOVW` | `MOVW{cond} Rd, #imm16` | 32 | Move 16-bit immediate into `Rd[15:0]`, zeroing `Rd[31:16]` |
| `MOVT` | `MOVT{cond} Rd, #imm16` | 32 | Move 16-bit immediate into `Rd[31:16]`, leaving `Rd[15:0]` unchanged |
| `MVN` | `MVN{S}{cond} Rd, Operand2` | 16, 32 | Move bitwise NOT |
| `ADR` | `ADR{cond} Rd, label` | 16, 32 | Form PC-relative address |

`MOVW` + `MOVT` is the standard idiom for loading an arbitrary 32-bit constant without a literal
pool. `MOV PC, Rm` is a branch.

---

## Arithmetic

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `ADD` | `ADD{S}{cond} Rd, Rn, Operand2` | 16, 32 | Add |
| `ADDW` | `ADDW{cond} Rd, Rn, #imm12` | 32 | Add 12-bit immediate, never sets flags |
| `ADC` | `ADC{S}{cond} Rd, Rn, Operand2` | 16, 32 | Add with carry |
| `SUB` | `SUB{S}{cond} Rd, Rn, Operand2` | 16, 32 | Subtract |
| `SUBW` | `SUBW{cond} Rd, Rn, #imm12` | 32 | Subtract 12-bit immediate, never sets flags |
| `SBC` | `SBC{S}{cond} Rd, Rn, Operand2` | 16, 32 | Subtract with carry |
| `RSB` | `RSB{S}{cond} Rd, Rn, Operand2` | 16, 32 | Reverse subtract (`Operand2 - Rn`) |
| `CMP` | `CMP{cond} Rn, Operand2` | 16, 32 | Compare — sets flags on `Rn - Operand2` |
| `CMN` | `CMN{cond} Rn, Operand2` | 16, 32 | Compare negative — sets flags on `Rn + Operand2` |

---

## Multiply and divide

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `MUL` | `MUL{S}{cond} Rd, Rn, Rm` | 16, 32 | 32 × 32 → lower 32. Only the 16-bit encoding can set flags |
| `MLA` | `MLA{cond} Rd, Rn, Rm, Ra` | 32 | Multiply-accumulate: `Rd = Ra + (Rn × Rm)` |
| `MLS` | `MLS{cond} Rd, Rn, Rm, Ra` | 32 | Multiply-subtract: `Rd = Ra - (Rn × Rm)` |
| `SMULL` | `SMULL{cond} RdLo, RdHi, Rn, Rm` | 32 | Signed 32 × 32 → 64 |
| `UMULL` | `UMULL{cond} RdLo, RdHi, Rn, Rm` | 32 | Unsigned 32 × 32 → 64 |
| `SMLAL` | `SMLAL{cond} RdLo, RdHi, Rn, Rm` | 32 | Signed multiply-accumulate → 64 |
| `UMLAL` | `UMLAL{cond} RdLo, RdHi, Rn, Rm` | 32 | Unsigned multiply-accumulate → 64 |
| `SDIV` | `SDIV{cond} Rd, Rn, Rm` | 32 | Signed divide |
| `UDIV` | `UDIV{cond} Rd, Rn, Rm` | 32 | Unsigned divide |

Division by zero returns 0 by default, or raises a UsageFault if `SCB->CCR.DIVBYZERO` is set.
None of the long multiplies set flags.

---

## Logical

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `AND` | `AND{S}{cond} Rd, Rn, Operand2` | 16, 32 | Bitwise AND |
| `ORR` | `ORR{S}{cond} Rd, Rn, Operand2` | 16, 32 | Bitwise OR |
| `ORN` | `ORN{S}{cond} Rd, Rn, Operand2` | 32 | Bitwise OR NOT — **Thumb-2 only, no 16-bit form** |
| `EOR` | `EOR{S}{cond} Rd, Rn, Operand2` | 16, 32 | Bitwise exclusive OR |
| `BIC` | `BIC{S}{cond} Rd, Rn, Operand2` | 16, 32 | Bit clear (`Rn AND NOT Operand2`) |
| `TST` | `TST{cond} Rn, Operand2` | 16, 32 | Test — flags from `Rn AND Operand2` |
| `TEQ` | `TEQ{cond} Rn, Operand2` | 32 | Test equivalence — flags from `Rn EOR Operand2` |

---

## Shift and rotate

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `LSL` | `LSL{S}{cond} Rd, Rm, #n \| Rs` | 16, 32 | Logical shift left |
| `LSR` | `LSR{S}{cond} Rd, Rm, #n \| Rs` | 16, 32 | Logical shift right |
| `ASR` | `ASR{S}{cond} Rd, Rm, #n \| Rs` | 16, 32 | Arithmetic shift right (sign preserving) |
| `ROR` | `ROR{S}{cond} Rd, Rm, #n \| Rs` | 16, 32 | Rotate right |
| `RRX` | `RRX{S}{cond} Rd, Rm` | 32 | Rotate right one bit through carry |

These are aliases of `MOV` with a shifted operand. Shifts can also be folded directly into most
data-processing instructions at no cost, e.g. `ADD R0, R1, R2, LSL #3`.

---

## Bit field and bit manipulation

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `BFC` | `BFC{cond} Rd, #lsb, #width` | 32 | Clear `width` bits starting at `lsb` |
| `BFI` | `BFI{cond} Rd, Rn, #lsb, #width` | 32 | Insert `width` low bits of `Rn` at `lsb` in `Rd` |
| `SBFX` | `SBFX{cond} Rd, Rn, #lsb, #width` | 32 | Extract and sign-extend a bit field |
| `UBFX` | `UBFX{cond} Rd, Rn, #lsb, #width` | 32 | Extract and zero-extend a bit field |
| `CLZ` | `CLZ{cond} Rd, Rm` | 32 | Count leading zeros |
| `RBIT` | `RBIT{cond} Rd, Rm` | 32 | Reverse bit order in a word |
| `REV` | `REV{cond} Rd, Rn` | 16, 32 | Reverse byte order in a word (endian swap) |
| `REV16` | `REV16{cond} Rd, Rn` | 16, 32 | Reverse byte order in each halfword |
| `REVSH` | `REVSH{cond} Rd, Rn` | 16, 32 | Reverse bytes in low halfword, sign-extend to 32 |

`REV` / `REV16` / `REVSH` are what the compiler emits for `__builtin_bswap32` and friends — useful
for network byte order without a shift-and-mask sequence.

---

## Sign and zero extension

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `SXTB` | `SXTB{cond} Rd, Rm{, ROR #n}` | 16, 32 | Sign-extend byte to 32 bits |
| `SXTH` | `SXTH{cond} Rd, Rm{, ROR #n}` | 16, 32 | Sign-extend halfword to 32 bits |
| `UXTB` | `UXTB{cond} Rd, Rm{, ROR #n}` | 16, 32 | Zero-extend byte to 32 bits |
| `UXTH` | `UXTH{cond} Rd, Rm{, ROR #n}` | 16, 32 | Zero-extend halfword to 32 bits |

The optional `ROR #8 | #16 | #24` is only available in the 32-bit encoding.

---

## Saturation

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `SSAT` | `SSAT{cond} Rd, #n, Rm{, shift}` | 32 | Saturate to signed `n`-bit range, `n` = 1–32 |
| `USAT` | `USAT{cond} Rd, #n, Rm{, shift}` | 32 | Saturate to unsigned `n`-bit range, `n` = 0–31 |

Both set the sticky `Q` flag in APSR on saturation. The halfword variants `SSAT16` / `USAT16` and
the saturating arithmetic `QADD` / `QSUB` family are **DSP extension only** — not on Cortex-M3.

---

## Load and store, single

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `LDR` | `LDR{cond} Rd, [addr]` | 16, 32 | Load word |
| `LDRB` | `LDRB{cond} Rd, [addr]` | 16, 32 | Load byte, zero-extended |
| `LDRH` | `LDRH{cond} Rd, [addr]` | 16, 32 | Load halfword, zero-extended |
| `LDRSB` | `LDRSB{cond} Rd, [addr]` | 16, 32 | Load byte, sign-extended |
| `LDRSH` | `LDRSH{cond} Rd, [addr]` | 16, 32 | Load halfword, sign-extended |
| `LDRD` | `LDRD{cond} Rd1, Rd2, [addr]` | 32 | Load two words into two registers |
| `STR` | `STR{cond} Rd, [addr]` | 16, 32 | Store word |
| `STRB` | `STRB{cond} Rd, [addr]` | 16, 32 | Store byte |
| `STRH` | `STRH{cond} Rd, [addr]` | 16, 32 | Store halfword |
| `STRD` | `STRD{cond} Rd1, Rd2, [addr]` | 32 | Store two words |

**Unprivileged variants** — perform the access with user privilege even from handler mode. All are
32-bit encodings with an 8-bit immediate offset only:

`LDRT`, `LDRBT`, `LDRHT`, `LDRSBT`, `LDRSHT`, `STRT`, `STRBT`, `STRHT`

---

## Load and store, multiple

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `LDM` | `LDM{IA}{cond} Rn{!}, reglist` | 16, 32 | Load multiple, increment after |
| `LDMDB` | `LDMDB{cond} Rn{!}, reglist` | 32 | Load multiple, decrement before |
| `STM` | `STM{IA}{cond} Rn{!}, reglist` | 16, 32 | Store multiple, increment after |
| `STMDB` | `STMDB{cond} Rn{!}, reglist` | 32 | Store multiple, decrement before |
| `PUSH` | `PUSH{cond} reglist` | 16, 32 | Push onto full-descending stack (`STMDB SP!`) |
| `POP` | `POP{cond} reglist` | 16, 32 | Pop from full-descending stack (`LDMIA SP!`) |

Stack-oriented aliases are accepted: `LDMFD` = `LDMIA`, `LDMEA` = `LDMDB`, `STMEA` = `STMIA`,
`STMFD` = `STMDB`. `POP` with PC in the register list performs a return.

---

## Exclusive access

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `LDREX` | `LDREX{cond} Rd, [Rn, #offset]` | 32 | Load word exclusive, tags the address |
| `LDREXB` | `LDREXB{cond} Rd, [Rn]` | 32 | Load byte exclusive |
| `LDREXH` | `LDREXH{cond} Rd, [Rn]` | 32 | Load halfword exclusive |
| `STREX` | `STREX{cond} Rd, Rt, [Rn, #offset]` | 32 | Store word exclusive. `Rd` = 0 on success, 1 on failure |
| `STREXB` | `STREXB{cond} Rd, Rt, [Rn]` | 32 | Store byte exclusive |
| `STREXH` | `STREXH{cond} Rd, Rt, [Rn]` | 32 | Store halfword exclusive |
| `CLREX` | `CLREX{cond}` | 32 | Clear the local exclusive monitor |

This is the only atomic read-modify-write mechanism on Cortex-M3 — `SWP`/`SWPB` were removed from
the architecture. Exclusive accesses must be naturally aligned.

---

## System and hint

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `MRS` | `MRS{cond} Rd, spec_reg` | 32 | Read a special register into a general register |
| `MSR` | `MSR{cond} spec_reg, Rn` | 32 | Write a general register into a special register |
| `CPSIE` | `CPSIE i \| f` | 16 | Enable interrupts — clears PRIMASK (`i`) or FAULTMASK (`f`) |
| `CPSID` | `CPSID i \| f` | 16 | Disable interrupts — sets PRIMASK (`i`) or FAULTMASK (`f`) |
| `SVC` | `SVC{cond} #imm8` | 16 | Supervisor call — raises the SVCall exception |
| `BKPT` | `BKPT #imm8` | 16 | Breakpoint — halts to debugger, or HardFault if none attached |
| `WFI` | `WFI{cond}` | 16 | Wait for interrupt — enter low-power sleep |
| `WFE` | `WFE{cond}` | 16 | Wait for event |
| `SEV` | `SEV{cond}` | 16 | Send event |
| `NOP` | `NOP{cond}` | 16, 32 | No operation |
| `YIELD` | `YIELD{cond}` | 16 | Hint: yield to another thread (NOP on Cortex-M3) |
| `DBG` | `DBG{cond} #imm4` | 32 | Debug hint (NOP on Cortex-M3) |

**Special registers accessible via `MRS` / `MSR`:**

`APSR`, `IPSR`, `EPSR`, `IAPSR`, `EAPSR`, `IEPSR`, `XPSR`, `MSP`, `PSP`, `PRIMASK`, `BASEPRI`,
`BASEPRI_MAX`, `FAULTMASK`, `CONTROL`

`PLD` and `PLI` (preload hints) assemble but execute as `NOP` — the Cortex-M3 has no cache to
preload into.

---

## Memory barriers

| Instruction | Syntax | Bits | Description |
|---|---|---|---|
| `DMB` | `DMB{cond}` | 32 | Data memory barrier — orders memory accesses either side |
| `DSB` | `DSB{cond}` | 32 | Data synchronisation barrier — completes all accesses before continuing |
| `ISB` | `ISB{cond}` | 32 | Instruction synchronisation barrier — flushes the pipeline |

Required after writes that must take effect before the next instruction, e.g. `DSB` + `ISB` after
changing `VTOR`, `CONTROL`, or the MPU configuration.

---

## Addressing modes

| Mode | Syntax | Notes |
|---|---|---|
| Offset | `[Rn, #offset]` | Base unchanged |
| Offset, register | `[Rn, Rm{, LSL #shift}]` | Shift is 0–3 |
| Pre-indexed | `[Rn, #offset]!` | Base updated **before** the access |
| Post-indexed | `[Rn], #offset` | Base updated **after** the access |
| PC-relative literal | `LDR Rd, =value` / `LDR Rd, label` | Assembler builds a literal pool |

Immediate offset ranges depend on encoding: 16-bit forms give a small scaled unsigned offset,
32-bit forms give unsigned 12-bit (offset only) or signed 8-bit (all indexing modes).

---

## Conditional execution and IT blocks

Unlike ARM state, Thumb instructions are **not** individually conditional. Conditional execution
requires an `IT` (If-Then) block:

```asm
    CMP     R0, #0
    ITE     NE          ; IT block: 1st instr if NE, 2nd if EQ
    MOVNE   R1, #1      ; Then
    MOVEQ   R1, #0      ; Else
```

- `IT` covers the **next 1 to 4** instructions.
- Suffix letters: `T` = then, `E` = else. `IT`, `ITT`, `ITE`, `ITTT`, `ITTE`, `ITET`, ... up to
  three suffix letters.
- Every instruction in the block must carry the matching condition suffix.
- Branches inside an IT block are only allowed as the **last** instruction.
- `B{cond}` outside an IT block is still legal — it is the one natively conditional instruction.

The compiler generates these automatically; you mostly see them in disassembly. GCC will also
choose `CBZ`/`CBNZ` over `CMP`+`B` where the range allows.

---

## Condition codes

| Code | Meaning | Flags |
|---|---|---|
| `EQ` | Equal | Z = 1 |
| `NE` | Not equal | Z = 0 |
| `CS` / `HS` | Carry set / unsigned higher or same | C = 1 |
| `CC` / `LO` | Carry clear / unsigned lower | C = 0 |
| `MI` | Minus, negative | N = 1 |
| `PL` | Plus, positive or zero | N = 0 |
| `VS` | Overflow set | V = 1 |
| `VC` | Overflow clear | V = 0 |
| `HI` | Unsigned higher | C = 1 and Z = 0 |
| `LS` | Unsigned lower or same | C = 0 or Z = 1 |
| `GE` | Signed greater than or equal | N = V |
| `LT` | Signed less than | N ≠ V |
| `GT` | Signed greater than | Z = 0 and N = V |
| `LE` | Signed less than or equal | Z = 1 or N ≠ V |
| `AL` | Always (default) | — |

---

## What the Cortex-M3 does NOT have

This is the section worth reading twice — most Cortex-M porting problems land here.

### No floating point unit

No `VADD.F32`, `VMUL.F64`, `VLDR`, `VSTR`, `VCVT`, or any other VFP instruction. All `float` and
`double` arithmetic compiles to soft-float library calls (`__aeabi_fadd`, `__aeabi_dmul`, …),
which are hundreds of cycles. Added by Cortex-M4F / M7.

### No DSP or SIMD extension (ARMv7E-M)

All of the following are **absent** on Cortex-M3 and present on Cortex-M4 / M7:

- SIMD arithmetic: `SADD8`, `SADD16`, `UADD8`, `SSUB16`, `UHADD8`, `SEL`, …
- Saturating arithmetic: `QADD`, `QSUB`, `QDADD`, `QDSUB`, `QADD8`, `QSUB16`, …
- Packing: `PKHBT`, `PKHTB`
- Halfword multiplies: `SMULBB`, `SMLABB`, `SMLAWB`, `SMULWT`, …
- Dual multiplies: `SMUAD`, `SMUSD`, `SMLAD`, `SMLSD`, `SMLALD`, `SMLSLD`
- Most significant word multiplies: `SMMUL`, `SMMLA`, `SMMLS`
- `UMAAL`
- Saturating halfword: `SSAT16`, `USAT16`
- Extend-and-add: `SXTAB`, `SXTAH`, `UXTAB`, `UXTAH`, `SXTAB16`, `UXTB16`, …
- `USAD8`, `USADA8`

### No ARM (A32) state

Thumb-2 only. Consequences:
- `BLX #imm` does not exist.
- Every function pointer must have bit[0] set. Branching to an even address raises a UsageFault
  with `INVSTATE` set.
- `SETEND` does not exist — endianness is fixed at reset, not switchable per instruction.

### No coprocessor interface

`MCR`, `MRC`, `MCRR`, `MRRC`, `CDP`, `LDC`, `STC` are not implemented. There is no CP15 — cache,
MMU and ID registers are memory-mapped in the System Control Space instead.

### Removed or not applicable

- `SWP` / `SWPB` — deprecated and removed; use `LDREX`/`STREX`.
- `RFE`, `SRS`, `LDM {reglist}^` — A/R-profile exception return mechanisms, not in M-profile.
- `MSR CPSR_c, #mode` — M-profile `CPS` only masks interrupts; there is no mode field to write.
- No cache maintenance instructions — the Cortex-M3 has no cache.

---

## Instruction timings

Cycle counts from the Cortex-M3 Technical Reference Manual, assuming zero-wait-state memory.
`P` denotes the pipeline refill penalty, typically 1–3 cycles.

| Instruction group | Cycles |
|---|---|
| ALU (`MOV`, `ADD`, `SUB`, `AND`, shifts, …) | 1 |
| `MUL` | 1 |
| `MLA`, `MLS` | 2 |
| `SMULL`, `UMULL`, `SMLAL`, `UMLAL` | 3–7 |
| `SDIV`, `UDIV` | 2–12 (early terminating — small quotients are fast) |
| `LDR`, `LDRB`, `LDRH` | 2 (pipelines to ~1 in back-to-back sequences) |
| `STR`, `STRB`, `STRH` | 2 |
| `LDRD`, `STRD` | 3 |
| `LDM`, `STM`, `PUSH`, `POP` | 1 + N (N = number of registers) |
| `B{cond}` not taken | 1 |
| `B`, `BL`, `BX`, `BLX` taken | 1 + P |
| `TBB`, `TBH` | 2 + P |
| `DMB`, `DSB`, `ISB` | 1–3 |
| `WFI`, `WFE` | 2 + sleep duration |

Wait states on flash and the bus matrix dominate in practice — treat these as a floor, not a
prediction. On STM32F103 above 24 MHz the flash prefetch buffer and its wait states matter more
than any individual instruction's cost.

---

## Practical notes

**Alignment.** `LDR`, `LDRH`, `STR`, `STRH` support unaligned access unless `SCB->CCR.UNALIGN_TRP`
is set. `LDM`, `STM`, `LDRD`, `STRD`, `PUSH`, `POP` and all exclusive accesses **always** require
natural alignment and fault otherwise.

**Code size.** Prefer R0–R7 in hot paths — high registers force 32-bit encodings. `ORN`, `TEQ`,
bit-field and long-multiply instructions have no 16-bit form at all.

**Constants.** A 32-bit constant is either `MOVW`+`MOVT` (8 bytes, 2 cycles, no data fetch) or a
literal pool `LDR` (4 bytes plus 4 bytes of pool, plus a data access). GCC decides; `-Os` tends
towards literal pools.

**Atomicity.** With no `SWP`, an atomic read-modify-write is `LDREX` / modify / `STREX` / retry on
failure. For a single-core M3 the cheaper option is usually masking interrupts with `CPSID i`
around a short critical section.

**Bit-banding** is not an instruction — it is an address aliasing feature of the memory map that
makes single-bit access atomic via ordinary `LDR`/`STR` to the bit-band alias region.

---

## References

- *ARMv7-M Architecture Reference Manual* (ARM DDI 0403) — authoritative instruction definitions
- *Cortex-M3 Technical Reference Manual* (ARM DDI 0337) — cycle timings and implementation detail
- *Cortex-M3 Devices Generic User Guide* (ARM DUI 0552) — programmer-oriented summary
