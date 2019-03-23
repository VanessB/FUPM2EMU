sqr:
    loadr   r0  r14 1
    mov     r2  r0  0
    mul     r0  r2  0
    ret     r0  1

intout:
    loadr   r0  r14 1
    syscall r0  102
    lc      r0  10
    syscall r0  105
    ret     r0  1

main:
    syscall r0  100
    push    r0  0
    calli   sqr
    push    r0  0
    calli   intout
    lc      r0  0
    syscall r0  0

end main