
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
_start: lgdt    [trick_gdt]     ; Load the trick GDT for higher half mode.
        mov     ax, 0x10        ; Data segment selector = 0x10
        mov     ds, ax
        mov     es, ax
        mov     fs, ax
        mov     gs, ax
        mov     ss, ax

        ;; Ensure the multiboot struct pointer is adjusted for the
        ;; move to the higher half.
        add     ebx, 0xC0000000

        ;; Far jump sets the code segment selector and moves to
        ;; the higher half.
        jmp     0x08:higherhalf
.end:

        ;; Data for Tim Robinson's GDT trick.
trick_gdt:
        dw gdt_end - gdt - 1
        dd gdt

gdt:    dd 0, 0
        db 0xFF, 0xFF, 0, 0, 0, 10011010b, 11001111b, 0x40	; code selector 0x08: base 0x40000000, limit 0xFFFFFFFF, type 0x9A, granularity 0xCF
	db 0xFF, 0xFF, 0, 0, 0, 10010010b, 11001111b, 0x40	; data selector 0x10: base 0x40000000, limit 0xFFFFFFFF, type 0x92, granularity 0xCF
gdt_end:

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
        resb    0x8000
stack:
