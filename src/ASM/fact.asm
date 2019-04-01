fact:
    loadr r0 r14 1
    cmpi r0 1
    jg skip0
    lc r0 1
    ret 1

skip0:
    push r0 0
    subi r0 1
    push r0 0
    calli fact
    pop r2 0
    mul r0 r2 0
    ret 1

main:
    syscall r0 100
    push r0 0
    calli fact
    syscall r0 102
    lc r0 10
    syscall r0 105
    lc r0 0
    syscall r0 0

end main