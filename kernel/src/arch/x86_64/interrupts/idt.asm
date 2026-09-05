global idt_flush
global isr_stub_table

extern interrupt_handler

; For interrupts/exceptions that do not push an error code
%macro NO_ERROR_CODE_ISR 1
isr_%1:
    push 0      ; dummy error code
    push %1     ; interrupt number
    jmp isr_common
%endmacro

; For exceptions that push an error code
%macro ERROR_CODE_ISR 1
isr_%1:
    ; CPU already pushed error code
    push %1     ; interrupt number
    jmp isr_common
%endmacro

%macro PUSH_ALL 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro


; Generate the individual ISRs
%assign i 0
%rep 256
    ; Exceptions that push an error code
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21
        ERROR_CODE_ISR i
    %else
        NO_ERROR_CODE_ISR i
    %endif
    %assign i i+1
%endrep


isr_common:
    PUSH_ALL
    mov rdi, rsp            ; pass the pointer to the saved registers
    call interrupt_handler

    POP_ALL
    add rsp, 16             ; clean up the pushed error code and interrupt number
    iretq                   ; return from interrupt


; Load the IDT
; rdi = pointer to struct idt_pointer
idt_flush:
    lidt [rdi]
    ret


; An array of pointers to each ISR generated above
isr_stub_table:
%assign i 0
%rep 256
    dq isr_%+i
%assign i i+1
%endrep
