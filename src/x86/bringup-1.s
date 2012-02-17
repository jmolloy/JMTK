
bits 32

section .mboot

MBOOT_MEM_INFO      equ 1<<1    ; Provide your kernel with memory info
MBOOT_HEADER_MAGIC  equ 0x1BADB002 ; Multiboot Magic value
MBOOT_FLAGS         equ MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC+MBOOT_FLAGS)
        
mboot:  dd      MBOOT_HEADER_MAGIC
        dd      MBOOT_FLAGS
        dd      MBOOT_CHECKSUM

section .text

global _start:function _start.end-_start
extern bringup

_start: cli
        mov     esp, stack
        xor     ebp, ebp
        push    ebx
        call    bringup
.stop:  hlt
        jmp .stop
.end:

section .bss
        resb    0x8000
stack:
        