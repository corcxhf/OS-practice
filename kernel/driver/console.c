#include <stdarg.h>

extern void uart_putc(char c);

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign)
{
  char buf[16];
  int i = 0;
  unsigned int x;
  if (sign && xx < 0)
  {
    x = (unsigned int)(-xx);
    sign = 1;
  }
  else
  {
    x = (unsigned int)xx;
    sign = 0;
  }

  do
  {
    buf[i++] = digits[x % base];
    x /= base;
  } while (x != 0);

  if (sign)
    buf[i++] = '-';

  while (i--)
    uart_putc(buf[i]);
}

/* 单字符输出包装（方便后续扩展，比如同时写入日志缓冲区）*/
static void consputc(int c) { uart_putc((char)c); }

void printf(char *fmt, ...)
{
  va_list ap;
  int i, c;
  char *s;

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

    /* 遇到 '%'，读取格式符 */
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;

    switch (c)
    {
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;

    case 'x':
      printint(va_arg(ap, int), 16, 0);

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
