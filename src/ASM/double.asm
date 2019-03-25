fout:
    syscall r0 103
    lc r0 10
    syscall r0 105
    ret 0

main:
    syscall r2 101
    syscall r4 101
    mov r0 r2 0
    mov r1 r3 0
    addd r0 r4 0
    calli fout
    mov r0 r2 0
    mov r1 r3 0
    subd r0 r4 0
    calli fout
    mov r0 r2 0
    mov r1 r3 0
    muld r0 r4 0
    calli fout
    mov r0 r2 0
    mov r1 r3 0
    divd r0 r4 0
    calli fout
    lc r0 100
    itod r0 r0 0
    calli fout
    lc r0 0
    syscall r0 0

end main