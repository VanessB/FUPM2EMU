start:
    lc r6 0
    lc r0 0

l0:
    cmpi r0 10
    jge e0
    lc r1 0

l1:
    cmpi r1 10
    jge e1
    lc r2 0

l2:
    cmpi r2 10
    jge e2
    lc r3 0

l3:
    cmpi r3 10
    jge e3
    lc r4 0

l4:
    cmpi r4 10
    jge e4
    lc r5 0

l5:
    cmpi r5 10
    jge e5
    mov r7 r0 0
    add r7 r1 0
    add r7 r2 0
    mov r8 r3 0
    add r8 r4 0
    add r8 r5 0
    cmp r7 r8 0
    jne skip
    addi r6 1

skip:
    addi r5 1
    jmp l5

e5:
    addi r4 1
    jmp l4

e4:
    addi r3 1
    jmp l3

e3:
    addi r2 1
    jmp l2

e2:
    addi r1 1
    jmp l1

e1:
    addi r0 1
    jmp l0

e0:
    syscall r6 102
    lc r0 10
    syscall r0 105
    lc r0 0
    syscall r0 0

end start