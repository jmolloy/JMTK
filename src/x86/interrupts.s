
%macro ISR_NOERRCODE 1
       global isr%1
isr%1: push byte 0
       push byte %1
       jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
       global isr%1
isr%1: push byte %1
       jmp isr_common
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14

%assign i 15
%rep 48-15
ISR_NOERRCODE i
%assign i i+1
%endrep

extern interrupt_handler

global isr_common:function isr_common.end-isr_common
isr_common:
        pusha

        mov ax, ds
        push eax

        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax

        push esp
        call interrupt_handler

        pop eax
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax

        popa
        add esp, 8

        iret
.end:
