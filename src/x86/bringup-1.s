
bits 32

        ;; Flag to request memory map information from the bootloader.
MBOOT_MEM_INFO      equ 1<<1
        ;; Multiboot magic value
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_FLAGS         equ MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC+MBOOT_FLAGS)

section .init
mboot:  dd      MBOOT_HEADER_MAGIC
        dd      MBOOT_FLAGS
        dd      MBOOT_CHECKSUM

        ;; Kernel entry point from bootloader.
        ;; At this point EBX is a pointer to the multiboot struct.
global _start:function _start.end-_start
_start: mov     eax, pd
        mov     dword [eax], pt + 3
        mov     dword [eax+0xC00], pt + 3

        mov     edx, pt
        mov     ecx, 0
.loop:  mov     eax, ecx
        shl     eax, 12
        or      eax, 3
        mov     [edx+ecx*4], eax
        inc     ecx
        cmp     ecx, 1024
        jnz     .loop

        mov     eax, pd+3
        mov     cr3, eax
        mov     eax, cr0
        or      eax, 0x80000000
        mov     cr0, eax

        jmp     higherhalf
.end:

section .init.bss nobits
pd:     resb    0x1000
pt:     resb    0x1000
        
extern bringup

section .text
global higherhalf:function higherhalf.end-higherhalf
higherhalf:
        mov     esp, stack      ; Ensure we have a valid stack.
        xor     ebp, ebp        ; Zero the frame pointer for backtraces.
        push    ebx             ; Pass multiboot struct as a parameter
        call    bringup
        
        cli
        hlt
.end:
        
section .bss
align 8192
global stack_base
stack_base:
        resb    0x2000
stack:
