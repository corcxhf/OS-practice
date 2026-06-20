#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int failures;

static void fail(const char *name)
{
	printf("FAIL %s\n", name);
	failures++;
}

static void expect_flag(const char *name, unsigned int flags, unsigned int flag)
{
	if ((flags & flag) == 0)
		fail(name);
}

static void expect_str(const char *name, const char *got, const char *want)
{
	if (strcmp(got, want) != 0) {
		printf("FAIL %s got=[%s] want=[%s]\n", name, got, want);
		failures++;
	}
}

static void read_line(char *buf, int cap)
{
	int i = 0;
	int c;

	while (i + 1 < cap) {
		c = getchar();
		if (c == EOF)
			break;
		if (c == '\r')
			continue;
		if (c == '\n')
			break;
		buf[i++] = c;
	}
	buf[i] = 0;
}

static int test_echo_modes(void)
{
	struct termios saved;
	struct termios raw;
	char buf[32];

	if (tcgetattr(STDIN_FILENO, &saved) < 0) {
		fail("tcgetattr");
		return -1;
	}
	expect_flag("default-echo", saved.c_lflag, ECHO);
	expect_flag("default-icanon", saved.c_lflag, ICANON);
	expect_flag("default-isig", saved.c_lflag, ISIG);

	raw = saved;
	raw.c_lflag &= ~ECHO;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
		fail("tcsetattr-noecho");
		return -1;
	}
	printf("TTY_NOECHO_READY\n");
	read_line(buf, sizeof(buf));
	printf("TTY_NOECHO_GOT:%s\n", buf);
	expect_str("noecho-input", buf, "secret");

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved) < 0) {
		fail("tcsetattr-restore");
		return -1;
	}
	printf("TTY_ECHO_READY\n");
	read_line(buf, sizeof(buf));
	printf("TTY_ECHO_GOT:%s\n", buf);
	expect_str("echo-input", buf, "visible");

	return 0;
}

static int block_until_input(void)
{
	printf("TTY_BLOCK_READY\n");
	getchar();
	printf("TTY_BLOCK_AFTER_READ\n");
	return 1;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "block") == 0)
		return block_until_input();

	test_echo_modes();

	if (failures) {
		printf("TTY_CONTRACT_FAIL %d\n", failures);
		return 1;
	}
	printf("TTY_CONTRACT_PASS\n");
	return 0;
}
