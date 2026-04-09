#include <stdarg.h>

extern void uart_putc(char c);

static char digits[] = "0123456789abcdef";

static void consputc(int c) { uart_putc((char)c); }

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
