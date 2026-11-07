global Check_long_mode_supported
global Enable_paging

Check_long_mode_supported:
  ; push these reg because they are used by the procedure
  push  rbx
  push  rdx
  ; check if the cpu supports cpuid instruction
  pushf
  pop   rax
  and   rax, 0xFFFFFFFFFFDFFFFF ; flip the 21th bit
  push rax
  popf
  pushf
  pop   rbx
  cmp   rax, rbx
  jne   cpuid_not_supported

  xor   eax, eax
  cpuid
  cmp   eax, 0x80000001
  jl    long_mode_not_supported

  mov   eax, 0x80000001
  cpuid
  shr   edx, 29
  cmp   edx, 1
  jne   long_mode_not_supported
  pop   rdx
  pop   rbx
  xor   rax, rax
  ret

cpuid_not_supported:
  pop   rdx
  pop   rbx
  mov   rax, 1
  ret

long_mode_not_supported:
  pop   rdx
  pop   rbx
  mov   rax, 2
  ret

Enable_paging:
  push  r8
  push  r9
  ; set PE(0) and PG(31) in cr0
  mov   r8, cr0
  mov   r9, 0x0000000080000001 
  or    r8, r9
  mov   cr0, r8

  ; set PAE(5) in cr4
  mov   r8, cr4
  mov   r9, 0x0000000000000020
  or    r8, r9
  mov   cr4, r8

  ; set LME(8) in IA32_EFER
  mov ecx, 0xC0000080
  rdmsr
  or  eax, 0x00000100
  wdmsr
  pop r9
  pop r8
  ret



