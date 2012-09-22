;;; #2
;;; 
;;; Interrupt handlers
;;; ==================
;;; 
;;; The last thing we need to do is define our raw, assembly interrupt handlers.
;;; 
;;; When an exception is taken, the CPU may push an error code on the stack. Or it might not. It depends upon the exception vector. Vectors #8, #10, #11, #12, #13 and #14 provide error codes.
;;; 
;;; What we do is provide two types of handler - one which expects an error code and one which does not. The one which does not expect an error code first pushes a dummy code (0) on the stack to ensure the stack layout is identical between vectors when we jump to common interrupt handling code.
;;; 
;;; Then, we can define these as NASM macros and replicate them for all interrupts we want to handle! {

%macro ISR_NOERRCODE 1
       global isr%1
isr%1: push byte 0              ; Push a dummy error code.
       push byte %1             ; Push the interrupt number.
       jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
       global isr%1
isr%1: push byte %1             ; Push the interrupt number
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
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14

        ;; Repeat ISR_NOERRCODE's up to interrupt 48.
%assign i 15
%rep 48-15
ISR_NOERRCODE i
%assign i i+1
%endrep

extern interrupt_handler

;;; The common interrupt handler does several things.
;;; 
;;; 1. Saves all of the register state in the machine. This is done using the ``pusha`` instruction.
;;; 2. Ensures all segment registers are set to kernel data segments.
;;; 3. Pushes ``%esp`` on to the stack. This serves as a pointer to the registers we just pushed (and the processor pushed some state for us too) for the C function ``interrupt_handler``.
;;; 4. After the C function returns, it resets the data segments to what they were before, restores all registers it saved and pops the error code and interrupt number from the stack.
;;; 5. It then performs an interrupt return ``iret`` to continue execution. {

global isr_common:function isr_common.end-isr_common
isr_common:
        pusha

        mov ax, ds              ; We can't push '%ds' directly, need to spill
        push eax                ; to a temporary.

        mov ax, 0x10            ; 0x10 is the kernel data selector.
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax

        push esp                ; Push pointer to the stack as x86_regs_t* arg.
        call interrupt_handler
        add esp, 4              ; Clean up x86_regs_t* argument from stack.
      
        pop eax
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax

        popa
        add esp, 8

        iret
.end:
