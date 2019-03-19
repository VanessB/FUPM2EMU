; DIVISION DEMO
; Считывает пару чисел (делимое) и третье число (делитель) и находит частное.
syscall r0 100
syscall r1 100
syscall r2 100
div     r0 r2
syscall r0 102
lc      r0 15
syscall r0 105
syscall r1 102
syscall r0 105
