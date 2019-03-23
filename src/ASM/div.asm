; DIVISION DEMO
; Считывает пару чисел (делимое) и третье число (делитель) и находит частное.
main:
syscall r0 100
syscall r1 100
syscall r2 100
div     r0 r2 0
syscall r0 102
lc      r0 10
syscall r0 105
syscall r1 102
syscall r0 105
jmp     main
end main