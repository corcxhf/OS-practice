#define UART0_BASE 0x10000000L
#define Reg(offset) ((volatile unsigned char *)(UART0_BASE + (offset)))

void uart_putc(char c)
{
  *Reg(0) = c;
  (void)c;
}

void uart_puts(char *s)
{
  while (*s)
  {
    uart_putc(*s);
    s++;
  }
  (void)s;
}