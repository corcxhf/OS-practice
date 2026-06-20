#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void fail(const char *name)
{
	printf("FAIL %s\n", name);
	failures++;
}

static void expect_int(const char *name, long got, long want)
{
	if (got != want) {
		printf("FAIL %s got=%ld want=%ld\n", name, got, want);
		failures++;
	}
}

static void expect_str(const char *name, const char *got, const char *want)
{
	if (strcmp(got, want) != 0) {
		printf("FAIL %s got=[%s] want=[%s]\n", name, got, want);
		failures++;
	}
}

static void test_format_scan(void)
{
	char buf[64];
	int a = 0;
	int b = 0;
	char word[16];

	snprintf(buf, sizeof(buf), "n=%d s=%s", 42, "ok");
	expect_str("snprintf", buf, "n=42 s=ok");

	if (sscanf("17 23 done", "%d %d %s", &a, &b, word) != 3)
		fail("sscanf-count");
	expect_int("sscanf-a", a, 17);
	expect_int("sscanf-b", b, 23);
	expect_str("sscanf-word", word, "done");
}

static void test_open_trunc(void)
{
	int fd;
	char buf[16];
	int n;
	const char *path = "ct_trunc";

	remove(path);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0) {
		fail("open-create");
		return;
	}
	if (write(fd, "abcdef\n", 7) != 7)
		fail("write-long");
	close(fd);

	fd = open(path, O_WRONLY | O_TRUNC);
	if (fd < 0) {
		fail("open-trunc");
		return;
	}
	if (write(fd, "x\n", 2) != 2)
		fail("write-short");
	close(fd);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fail("open-read");
		return;
	}
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	expect_int("trunc-size", n, 2);
	expect_str("trunc-content", buf, "x\n");
}

static void test_stdio_rw(void)
{
	FILE *f;
	char buf[16];
	const char *path = "ct_stdio";

	remove(path);
	f = fopen(path, "w");
	if (!f) {
		fail("fopen-w");
		return;
	}
	expect_int("fwrite-long", fwrite("abcdef", 1, 6, f), 6);
	fclose(f);

	f = fopen(path, "w");
	if (!f) {
		fail("fopen-w-trunc");
		return;
	}
	expect_int("fwrite-short", fwrite("xy", 1, 2, f), 2);
	fclose(f);

	f = fopen(path, "r");
	if (!f) {
		fail("fopen-r");
		return;
	}
	memset(buf, 0, sizeof(buf));
	expect_int("fread-short", fread(buf, 1, sizeof(buf) - 1, f), 2);
	fclose(f);
	expect_str("stdio-content", buf, "xy");
}

static void test_lseek_rw(void)
{
	int fd;
	char buf[8];
	const char *path = "ct_seek";

	remove(path);
	fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		fail("open-seek");
		return;
	}
	expect_int("seek-write", write(fd, "abc", 3), 3);
	expect_int("seek-set", lseek(fd, 1, SEEK_SET), 1);
	expect_int("seek-overwrite", write(fd, "Z", 1), 1);
	expect_int("seek-rewind", lseek(fd, 0, SEEK_SET), 0);
	memset(buf, 0, sizeof(buf));
	expect_int("seek-read", read(fd, buf, 3), 3);
	close(fd);
	expect_str("seek-content", buf, "aZc");
}

static void test_malloc_realloc(void)
{
	char *p = malloc(8);
	char *q;

	if (!p) {
		fail("malloc");
		return;
	}
	strcpy(p, "abc");
	q = realloc(p, 32);
	if (!q) {
		fail("realloc");
		free(p);
		return;
	}
	expect_str("realloc-content", q, "abc");
	free(q);
}

int main(void)
{
	test_format_scan();
	test_open_trunc();
	test_stdio_rw();
	test_lseek_rw();
	test_malloc_realloc();

	if (failures) {
		printf("LIBC_CONTRACT_FAIL %d\n", failures);
		return 1;
	}
	printf("LIBC_CONTRACT_PASS\n");
	return 0;
}
