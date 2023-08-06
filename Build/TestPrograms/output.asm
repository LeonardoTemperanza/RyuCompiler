add:
  mov RAX, 2
  add RAX, 2
.ret:
main:
  mov RBX, add
  call RBX
  mov RBX, add
  # spill D2
  mov dword [RBP + -24], RAX
  call RBX
  # reload D2
  mov RAX, dword [RBP + -24]
  mov RCX, RAX
  add RCX, RAX
  mov RAX, RCX
.ret:
