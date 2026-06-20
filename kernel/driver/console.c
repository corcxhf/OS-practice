#include <stdarg.h>
#include "proc.h"
#include "riscv.h"
#include "fs.h"
#include "types.h"

#define CONS_BUF_SIZE 1024
extern void uart_putc(char c);
extern struct proc *myproc(void);
extern struct proc proc[NPROC];
extern void acquire(struct spinlock *);
extern void release(struct spinlock *);
static char digits[] = "0123456789abcdef";

static void consputc(int c) { uart_putc((char)c); }
static char cons_buffer[CONS_BUF_SIZE];
static int cons_head = 0; // 读指针
static int cons_tail = 0; // 写指针
int stdin_line_len = 0;
int esc_state = 0;
void printint(long xx, int base, int sign)
{
  char buf[64];
  int i;
  unsigned long x;
  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do
  {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);
  if (sign)
    buf[i++] = '-';
  while (--i >= 0)
    consputc((int)buf[i]);
}

void printf(char *fmt, ...)
{
  va_list ap;
  int i, c;
  char *s;
  int is_long;
  if (fmt == 0)
    return;
  va_start(ap, fmt);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    is_long = 0;
    c = fmt[++i] & 0xff;
    if (c == 'l')
    {
      is_long = 1;
      c = fmt[++i] & 0xff;
    }
    if (c == 0)
      break;
    switch (c)
    {
    case 'd':
      if (is_long)
        printint(va_arg(ap, long), 10, 1);
      else
        printint(va_arg(ap, int), 10, 1);
      break;
    case 'u':
      if (is_long)
        printint(va_arg(ap, unsigned long), 10, 0);
      else
        printint(va_arg(ap, unsigned int), 10, 0);
      break;

    case 'x':
      if (is_long)
        printint(va_arg(ap, unsigned long), 16, 0);
      else
        printint(va_arg(ap, unsigned int), 16, 0);
      break;

    case 'p':
      printint(va_arg(ap, unsigned long), 16, 0);
      break;

    case 's':
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; ++s)
        consputc(*s);
      break;

    case 'c':
      consputc(va_arg(ap, int));
      break;

    case '%':
      consputc('%');
      break;

    default:
      consputc('%');
      if (is_long)
        consputc('l');
      consputc(c);
      break;
    }
  }

  va_end(ap);
}

/* ================================================================
 * TODO [Lab2-任务3]：
 *   实现 clear_screen() 函数。
 *
 *   ANSI 转义序列：
 *     "\x1b[2J" — 清除整个屏幕内容
 *     "\x1b[H"  — 将光标移动到左上角 (0,0) 位置
 *
 *   提示：现在你已经有 printf 了，可以直接用：
 *     printf("\x1b[2J");
 *     printf("\x1b[H");
 * ================================================================ */
void clear_screen(void)
{
  printf("\x1b[2J");
  printf("\x1b[H");
}

/* panic — 内核致命错误处理（已提供，无需修改）
 *
 * 当内核遇到无法恢复的错误时，打印出错信息并进入死循环。
 * __attribute__((noreturn)) 告诉编译器这个函数永远不会返回。
 */
void panic(char *msg)
{
  printf("\n\n");
  printf("!!! KERNEL PANIC !!!\n");
  printf("Reason: %s\n", msg);
  printf("System halted.\n");
  while (1)
    ; /* 死循环，防止CPU继续乱跑 */
}

void console_print_char(int c)
{
  switch (c)
  {
  case '\r':
    consputc('\r');
    break;
  case '\n':
    consputc('\r');
    consputc('\n');
    break;
  case 0x7f:
    consputc('\b');
    consputc(' ');
    consputc('\b');
    break;
  default:
    consputc(c);
    break;
  }
}
void consoleintr(int c)
{
  if (c == 3)
  {
    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->pid > 1 && p->status != TASK_FREE && p->status != TASK_ZOMBIE)
      {
        p->killed = 1;
        if (p->status == TASK_SLEEPING)
          p->status = TASK_READY;
      }
      release(&p->lock);
    }
    extern void wakeup(void *);
    wakeup(&cons_buffer);
    return;
  }

  if (c == '\r')
    c = '\n';

  // 🚨 铁律：内核只负责收件，绝对不准调用 consputc 物理回显任何字符！
  int next_tail = (cons_tail + 1) % CONS_BUF_SIZE;
  if (next_tail != cons_head)
  {
    cons_buffer[cons_tail] = c;
    cons_tail = next_tail;

    // 🌟 核心修复：因为内核不回显，全靠用户态驱动
    // 所以只要有任何一点风吹草动（任何字符），都必须立刻叫醒等待的进程（Shell 或 cat）
    extern void wakeup(void *);
    wakeup(&cons_buffer);
  }
}
int consgetc(void)
{
  while (cons_head == cons_tail)
  {
    struct proc *p = myproc();
    if (p && p->killed)
      return -1;
    // 如果你的内核有实现让出CPU的 yield()，可以在这里调用
    // 从而防止当前的死循环把 CPU 彻底卡死
    extern void yield(void);
    yield();
  }

  if (myproc() && myproc()->killed)
    return -1;

  // 从缓冲区拿出一个字符并返回
  int c = cons_buffer[cons_head];
  cons_head = (cons_head + 1) % CONS_BUF_SIZE;
  return c;
}

int console_pending(void)
{
  if (cons_tail >= cons_head)
    return cons_tail - cons_head;
  return CONS_BUF_SIZE - cons_head + cons_tail;
}
