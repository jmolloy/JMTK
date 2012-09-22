;;; #1
;;; Machine bringup #1
;;; ==================
;;; 
;;; Now we need to write the code to get the machine to call ``main(argc, argv)``,
;;; and that means explaining a little about the way an x86 machine boots up.
;;; 
;;; .. note::
;;; 
;;;     This explanation does not include EFI, the new BIOS replacement. But note
;;;     that EFI has a BIOS emulation mode, so this is still valid.
;;; 
;;; On poweron, the first thing the processor does is set the program counter (EIP)
;;; to the address 0x7C00 and start executing. It will find BIOS code there, which
;;; will peform a power on self test (POST) and initialise a load of the
;;; peripherals.
;;; 
;;; The BIOS will eventually try and transfer control to user code - it does this by
;;; loading the first sector of whatever disk you specify. This is usually a hard
;;; disk, but could be a floppy drive or CD-ROM.
;;; 
;;; I'll only talk about the hard and floppy disk cases here, as they're both
;;; handled the same and are the most common. If you want to work out how
;;; bootloaders work with CD-ROMs, google for "El Torito".
;;; 
;;; Anyway, the BIOS loads the first sector of the boot media - this is 512
;;; bytes. Note also that this must contain two "signature" bits at the end, so the
;;; amount of available space for instructions is actually 510 bytes.
;;; 
;;; For obvious reasons this is called a "stage 1" bootloader. It needs to, in 510
;;; bytes of instruction space, work out how to load its companion "stage 2"
;;; bootloader, which is more featureful and can provide an interface etc - this is
;;; what you perceive as GRUB's interface when you see it on linux bootup.
;;; 
;;; The stage 1 and stage 2 bootloaders can make use of an API the BIOS provides to
;;; load and store sectors to media, write to the screen and other stuff. However if
;;; it wants to use this, it has some restrictions too - it must be in the legacy
;;; 16-bit mode of the x86, which can only address a maximum of 1MB of memory (2^16
;;; << 4 if you're interested).
;;; 
;;; Normally a bootloader will transition to 32-bit mode at this point and load a
;;; kernel (this is a simplified view but it'll do for the moment).
;;; 
;;; There is a lot of legacy cruft that must be dealt with when writing a
;;; bootloader, so for this tutorial we are going to assume one already exists and
;;; use that.
;;; 
;;; Multiboot
;;; =========
;;; 
;;; There are a multitude of bootloaders, but for x86 there is a de facto
;;; standard way of interfacing between kernel and bootloader, called the
;;; [Multiboot
;;; specification](http://www.gnu.org/software/grub/manual/multiboot/multiboot.html).
;;; 
;;; Most of the actions in this specification are for the bootloader to
;;; perform, but we must do at least one thing, and that is expose a
;;; *multiboot header* somewhere within the first 8KB of our kernel image.
;;; 
;;; The multiboot header consists of a magic number and a set of flags,
;;; along with a checksum.
;;; 
;;; In return, the bootloader will:
;;;   * Load an ELF image and start executing at the image's entry point.
;;;   * Leave the machine in a predictable state (with interrupts disabled).
;;;   * Leave a pointer to a structure containing information about the
;;;     environment (*multiboot info struct*) in the ``EBX`` register.
;;; 
;;; We'll talk a bit more about the multiboot information struct later,
;;; but for now I should mention that there is only one flag in the
;;; multiboot header that we care about, and that has the value ``1<<1``
;;; (i.e. ``2``, the 1st bit set). This flag will instruct the bootloader
;;; to give the kernel a memory map of where all the RAM is in the system
;;; as part of the multiboot info struct. More on that later.
;;; 
;;; Enough talk, let's begin...
;;; 
;;; Firstly we need to inform the assembler (NASM in this case) that we
;;; are assembling for 32-bit mode. {
bits 32

;;; Then, we can define the *multiboot header*. The following are the NASM
;;; equivalent of C #defines.
;;; 
;;; The checksum field is to ensure the magic and flags got read correctly, and
;;; is defined as the number required to add to the magic number and flags in
;;; order to make the result zero. Another important role checksum field serves
;;; is to guarantee that this is actually a multiboot header that the bootloader 
;;; has found and not some random bytes just looking the same way. {

        ;; Flag to request memory map information from the bootloader.
MBOOT_MEM_INFO      equ 1<<1
        ;; Multiboot magic value
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_FLAGS         equ MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC+MBOOT_FLAGS)

;;; Now let's define the header. {
        
section .init
mboot:  dd      MBOOT_HEADER_MAGIC
        dd      MBOOT_FLAGS
        dd      MBOOT_CHECKSUM
;;; }
;;; That's it, that's all that is required to create a multiboot compliant image.
;;; 
;;; Higher half kernels
;;; ===================
;;; 
;;; You might notice that we put the header in a section called ``.init``. What is
;;; ``.init``? And why do we need it?
;;; 
;;; We are kind of jumping the gun here - I'd have preferred to keep this
;;; explanation until the chapter on virtual memory and paging. But there's no way
;;; to move it later, so here goes.
;;; 
;;; Under normal operation, the addresses given to the CPU such as pointers or call
;;; addresses are *virtual* - they may be transformed before the hardware goes and
;;; accesses memory.
;;; 
;;; The address you as a programmer give to the CPU is a *virtual address*. The
;;; address the CPU gives in turn to the memory controller is a *physical
;;; address*. The CPU maintains a set of mappings (via several mechanisms) between
;;; the two.
;;; 
;;; This is used for multiple reasons - one reason is that the RAM in a system may
;;; not be contiguous from zero up to however-much-you-have - it may appear in
;;; clumps throughout the memory space and there may be holes in between.
;;; 
;;; The other major reason for this is *protection* - under normal operating system
;;; conditions every user process has its own *virtual address space*. Because of
;;; this, it cannot address and therefore cannot maliciously interact with memory
;;; belonging to another process. This is a hardware enforced isolation mechanism.
;;; 
;;; Why am I telling you this now and not later? Well, the de facto standard is to
;;; have the first *N* GB of address space available for user process use, and the
;;; higher *4-N* GB reserved for the kernel. The usual value for *N* is either 2 or
;;; 3, by the way. It is 3GB in Linux by default, for a 32-bit x86 system. Obviously
;;; for a 64-bit system it will be much higher.
;;; 
;;; A kernel that resides in the higher regions of virtual memory is called a
;;; *higher half* kernel.
;;; 
;;; Because we really want to stick to de facto standards (it makes our life easier
;;; in the long run), it's a good idea to move to the higher half early. There are
;;; multiple ways to do this, but what we're going to do is enable *paging*, which
;;; is one of the mechanisms the x86 has to perform virtual -> physical mappings.
;;; 
;;; I'd really like to explain it all to you now, but I feel it really should wait
;;; until the chapter on paging for a full explanation. So for the moment, please
;;; think of it as *magic* and it'll make sense later :)
;;; 
;;; Anyway, most of our code will be linked to run in the higher half, but we
;;; need some code to run in the lower half as the bootloader will leave us with
;;; paging *disabled*. That is what the ``.init`` section is for, and will be
;;; defined later in the linker script.
;;; 
;;; With that out of the way, lets go ahead and define our first few bytes of code,
;;; which will essentially be magic for now. {
        
        ;; Kernel entry point from bootloader.
        ;; At this point EBX is a pointer to the multiboot struct.
global _start:function _start.end-_start
_start: mov     eax, pd         ; MAGIC START!
        mov     dword [eax], pt + 3 ; addrs 0x0..0x400000 = pt | WRITE | PRESENT
        mov     dword [eax+0xC00], pt + 3 ; addrs 0xC0000000..0xC0400000 = same

        ;; Loop through all 1024 pages in page table 'pt', setting them to be
        ;; identity mapped with full permissions.
        mov     edx, pt
        mov     ecx, 0          ; Loop induction variable: start at 0
        
.loop:  mov     eax, ecx        ; tmp = (%ecx << 12) | WRITE | PRESENT
        shl     eax, 12
        or      eax, 3
        mov     [edx+ecx*4], eax ; pt[ecx * sizeof(entry)] = tmp
        
        inc     ecx
        cmp     ecx, 1024       ; End at %ecx == 1024
        jnz     .loop

        mov     eax, pd+3       ; Load page directory | WRITE | PRESENT
        mov     cr3, eax        ; Store into cr3.
        mov     eax, cr0
        or      eax, 0x80000000 ; Set PG bit in cr0 to enable paging.
        mov     cr0, eax

        jmp     higherhalf
.end:

section .init.bss nobits
pd:     resb    0x1000
pt:     resb    0x1000          ; MAGIC END!

;;; All you need to know about that code is that it set up some mappings such that
;;; addresses 0xC0000000 .. 0xC0400000 virtual get mapped to 0x00000000
;;; .. 0x00400000 physical. So we can basically just add 0xC0000000 (which is 3GB,
;;; by the way) to any physical address and get the virtual address.
;;; 
;;; It then jumped to a function called "higherhalf", which we are about to define.

;;; Now, when I explained about multiboot, you may not have noticed that setting up
;;; a valid stack (value in the ESP register) was not part of the contract between
;;; bootloader and kernel.
;;; 
;;; Therefore we need to set one up now before we can perform any ``CALL``
;;; instructions. Remember also that the multiboot info struct was passed in the
;;; ``EBX`` register by the bootloader (and we deliberately didn't clobber it in
;;; the above code.

;;; Here we created a stack, zeroed the ``EBP`` register (remember that ``x xor x``
;;; is always zero), pushed the multiboot info struct onto the stack as the first
;;; parameter to the function ``bringup``, which we're about to define and which
;;; will be the first time we can run code written in C.
;;; 
;;; Once that function returns, we perform a ``cli/hlt`` in order to stop the
;;; processor entirely. This should ideally never happen, but it's better than
;;; running off into undefined memory. {

extern bringup

        ;; Note that we're now defining functions in the normal .text section,
        ;; which means we're linked in the higher half (based at 3GB).
section .text
global higherhalf:function higherhalf.end-higherhalf
higherhalf:
        mov     esp, stack      ; Ensure we have a valid stack.
        xor     ebp, ebp        ; Zero the frame pointer for backtraces.
        push    ebx             ; Pass multiboot struct as a parameter
        call    bringup         ; Call kernel bringup function.
        cli                     ; Kernel has finished, so disable interrupts ...
        hlt                     ; ... And halt the processor.
.end:

section .bss
align 8192
global stack_base
stack_base:
        resb    0x2000
stack:
