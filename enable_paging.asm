global Check_long_mode_supported

Check_long_mode_supported:
  push  rbx
  push  rdx
  pushf
  pushf
  pop   rax
  and   rax, 0xFFFFFFFFFF2FFFFF
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
