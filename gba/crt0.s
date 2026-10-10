    .arm
    .section .crt0, "ax"
    .global _start
    .align 2

_header:
    b _start                    @ 0x00: Branch to start of game code (offset 0xC0)
    .space 156, 0               @ 0x04..0x9F: Nintendo logo (patched by gbafix)
    .ascii "DUFFER PRO  "       @ 0xA0..0xAB: Title (12 bytes)
    .ascii "ADFP"               @ 0xAC..0xAF: Game code (4 bytes)
    .ascii "01"                 @ 0xB0..0xB1: Maker code (2 bytes)
    .byte 0x96                  @ 0xB2: Fixed value 0x96
    .byte 0x00                  @ 0xB3: Main unit code
    .byte 0x00                  @ 0xB4: Device type
    .space 7, 0                 @ 0xB5..0xBB: Reserved
    .byte 0x00                  @ 0xBC: Software version
    .byte 0x00                  @ 0xBD: Checksum (patched by gbafix)
    .space 2, 0                 @ 0xBE..0xBF: Reserved

    .align 2
_start:
    @ Set processor mode to IRQ and initialize IRQ stack
    mov r0, #0x12               @ CPSR mode: IRQ
    msr cpsr_c, r0
    ldr sp, =__sp_irq

    @ Set processor mode to System/User and initialize User stack
    mov r0, #0x1F               @ CPSR mode: System
    msr cpsr_c, r0
    ldr sp, =__sp_usr

    @ Copy .data from ROM (__data_load) to EWRAM (__data_start .. __data_end)
    ldr r0, =__data_start
    ldr r1, =__data_end
    ldr r2, =__data_load
.copy_data_loop:
    cmp r0, r1
    bge .copy_data_done
    ldr r3, [r2], #4
    str r3, [r0], #4
    b .copy_data_loop
.copy_data_done:

    @ Zero .bss in EWRAM (__bss_start .. __bss_end)
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
.zero_bss_loop:
    cmp r0, r1
    bge .zero_bss_done
    str r2, [r0], #4
    b .zero_bss_loop
.zero_bss_done:

    @ Call C main function
    ldr r0, =main
    mov lr, pc
    bx r0

    @ Hang if main ever exits
.hang:
    b .hang
