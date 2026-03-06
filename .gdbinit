set architecture riscv:rv64
target remote localhost:1234
display/x $sp
set print pretty on