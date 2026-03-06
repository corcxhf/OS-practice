#define UART0 0x10000000L

#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))


void uart_putc(char c){
    *Reg(0) = c;
}

void uart_puts(char *s)
{
    while(*s)
    {
        uart_putc(*s);
        s++;
    }
}