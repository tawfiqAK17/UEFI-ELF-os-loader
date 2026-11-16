__attribute__((noreturn)) __attribute__((naked)) void kernel_entry(void) {
  asm("cli");
  asm("hlt");
}
