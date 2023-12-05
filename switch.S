/* Context switch https://samwho.dev/blog/context-switching-on-x86/ */
.global switch

switch: 
    testl %esi,%esi
    jz 1f
    pushfl
    pushal
    pushw %ds
    pushw %es
    pushw %fs
    pushw %gs
    movl %esp, (%esi)

1:
    movl (%edi) , %esp
    popw %gs
    popw %fs
    popw %es
    popw %ds
    popal
    popfl

    ret