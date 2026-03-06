extern void uart_puts(char *s);

void start_main(){
    uart_puts("Hello OS from RISC-V Bare-metal!\n");
    while(1);
}