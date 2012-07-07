bits 64

global longjmp:function longjmp.end-longjmp
longjmp:
        mov     rcx, rdi         ; First parameter = jmp_buf
        mov     rax, rsi         ; Second = return value
        
        mov     rsp, [rcx+0]
        mov     rbp, [rcx+8]
        mov     rbx, [rcx+24]
        mov     rsi, [rcx+32]
        mov     rdi, [rcx+40]
        
        mov     rdx, [rcx+48]
        push    rdx
        popf

        mov     r8,  [rcx+56]
        mov     r9,  [rcx+64]
        mov     r10, [rcx+72]
        mov     r11, [rcx+80]
        mov     r12, [rcx+88]
        mov     r13, [rcx+96]
        mov     r14, [rcx+104]
        mov     r15, [rcx+112]

        mov     rcx, [rcx+16]
        jmp     rcx
.end:
        
global setjmp:function setjmp.end-setjmp
setjmp:
        mov     rcx, rdi          ; First parameter = jmp_buf
        ;; Stack pointer for longjmp should ignore setjmp's return address
        ;; i.e. rsp+4.
        mov     rax, rsp
        add     rax, 8
        mov     [rcx+0], rax
        mov     [rcx+8], rbp

        mov     rax, [rsp]      ; Return address at [rsp]
        mov     [rcx+16], rax

        mov     [rcx+24], rbx
        mov     [rcx+32], rsi
        mov     [rcx+40], rdi

        pushf                   ; RFLAGS
        pop     rax
        mov     [rcx+48], rax

        mov     [rcx+56], r8
        mov     [rcx+64], r9
        mov     [rcx+72], r10
        mov     [rcx+80], r11
        mov     [rcx+88], r12
        mov     [rcx+96], r13
        mov     [rcx+104], r14
        mov     [rcx+112], r15

        xor     rax, rax        ; Rrturn 0
        ret
.end:
        