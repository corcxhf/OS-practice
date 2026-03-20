/* main.c — 内核 C 语言主函数（Lab1 任务4，后续实验持续扩展）
 *
 * 注意：随着实验推进，你需要在 start_main() 中依次添加新模块的初始化调用。
 * 每个实验结束后，start_main() 大约会是什么样子，注释中有说明。
 */

/* =============================================================
 * Lab1 完成后，这个文件应该像这样：
 *
 *   extern void uart_puts(char *s);
 *   void start_main() {
 *       uart_puts("Hello OS from RISC-V Bare-metal!\n");
 *       while(1);
 *   }
 *
 * Lab2 完成后，扩展为调用 printf 和 clear_screen。
 * Lab3 完成后，增加 kinit(), kvmininit(), kvminithart()。
 * Lab4 完成后，增加 trapinithart(), start()（移至 start.c）。
 * Lab5 完成后，增加 procinit(), scheduler()。
 * ============================================================= */

/* 声明在 uart.c 中实现的函数（Lab2完成后改用 defs.h 统一管理）*/
extern void uart_puts(char *s);
extern void printf(char *fmt, ...);
extern void clear_screen();
extern void kinit();
extern void kvmininit();
extern void kvminithart();

void start_main()
{

  kinit();     // 1. 必须第一步：建立 free_mem_list，此后才能 kalloc
  kvmininit(); // 2. 建立内核页表（内部会 kalloc 页表页）
  // kvminithart(); // 3. 写 satp，开启 MMU
  // clear_screen();
  printf("Memory initialized. Paging enabled!\n");
  while (1)
    ;
}
