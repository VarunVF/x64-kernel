global vmm_switch_stack_and_cr3

; Switch to the new stack by setting rsp and rbp, and switch page tables.
; rdi = masked pml4 physical address
; rsi = new value of rsp
; rdx = function to jump to after switching
vmm_switch_stack_and_cr3:
    mov rsp, rsi        ; Switch to new stack
    mov rbp, rsp        ; Reset frame pointer
    mov cr3, rdi        ; Switch to new page tables
    push 0              ; Null return address
    jmp rdx
