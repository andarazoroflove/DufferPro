OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

__sp_usr = 0x03007F00;
__sp_irq = 0x03007FA0;

SECTIONS {
    .crt0 : {
        KEEP(*(.crt0))
    } > rom

    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > rom

    __data_load = LOADADDR(.data);
    .data : {
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > ewram AT > rom

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ewram

    .iwram (NOLOAD) : {
        __iwram_start = .;
        *(.iwram*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
    }
}

