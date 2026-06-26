global gdt_flush

; Point the CPU to the GDT, and reload data segment registers.
; rdi = pointer to struct gdt_ptr
gdt_flush:
    lgdt [rdi]
    
    ; Reload cs register
    push 0x08                   ; Push code segment offset to stack
    lea rax, [rel .reload_cs]   ; Load address of .reload_cs into rax
    push rax                    ; Push this value to the stack
    retfq                       ; Perform a far return
.reload_cs:
    ; Reload data segment registers
    mov   ax, 0x10              ; Data segment offset
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    mov   ss, ax
    ret
