#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define T_FILE 2

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

static void expect_file_stat(const char *name, const struct stat *st, long size)
{
	expect_int(name, st->st_size, size);
	expect_int("stat-type", st->st_mode, T_FILE);
}

static int write_file(const char *path, const char *text)
{
	int fd;
	int len = strlen(text);
	int n;

	remove(path);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0) {
		fail("write-file-open");
		return -1;
	}
	n = write(fd, text, len);
	close(fd);
	if (n != len) {
		fail("write-file-write");
		return -1;
	}
	return 0;
}

static int read_file(const char *path, char *buf, int cap)
{
	int fd;
	int n;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fail("read-file-open");
		return -1;
	}
	memset(buf, 0, cap);
	n = read(fd, buf, cap - 1);
	close(fd);
	if (n < 0)
		fail("read-file-read");
	return n;
}

static void test_create_read_stat(void)
{
	const char *path = "fs_basic";
	const char *text = "hello fs\n";
	char buf[32];
	struct stat st;
	int fd;

	remove(path);
	fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		fail("basic-open");
		return;
	}
	expect_int("basic-write", write(fd, text, strlen(text)), strlen(text));
	expect_int("basic-fstat", fstat(fd, &st), 0);
	expect_file_stat("basic-fstat-size", &st, strlen(text));
	expect_int("basic-rewind", lseek(fd, 0, SEEK_SET), 0);
	memset(buf, 0, sizeof(buf));
	expect_int("basic-read", read(fd, buf, sizeof(buf) - 1), strlen(text));
	expect_str("basic-content", buf, text);
	close(fd);

	memset(&st, 0, sizeof(st));
	expect_int("basic-stat", stat(path, &st), 0);
	expect_file_stat("basic-stat-size", &st, strlen(text));
}

static void test_open_trunc_releases_tail(void)
{
	const char *path = "fs_trunc";
	char buf[32];
	struct stat st;
	int fd;

	if (write_file(path, "abcdef\n") < 0)
		return;

	fd = open(path, O_WRONLY | O_TRUNC);
	if (fd < 0) {
		fail("trunc-open");
		return;
	}
	expect_int("trunc-empty-size", fstat(fd, &st), 0);
	expect_file_stat("trunc-empty", &st, 0);
	expect_int("trunc-write", write(fd, "x\n", 2), 2);
	close(fd);

	expect_int("trunc-read-size", read_file(path, buf, sizeof(buf)), 2);
	expect_str("trunc-content", buf, "x\n");
}

static void test_unlink_recreate(void)
{
	const char *path = "fs_unlink";
	char buf[16];
	int fd;

	if (write_file(path, "old") < 0)
		return;
	expect_int("unlink-existing", unlink(path), 0);

	fd = open(path, O_RDONLY);
	if (fd >= 0) {
		close(fd);
		fail("unlink-removes-name");
	}

	if (write_file(path, "new") < 0)
		return;
	expect_int("recreate-size", read_file(path, buf, sizeof(buf)), 3);
	expect_str("recreate-content", buf, "new");
}

static void test_rename_file(void)
{
	char buf[32];
	int fd;

	remove("fs_ren_a");
	remove("fs_ren_b");
	remove("fs_ren_c");
	remove("fs_ren_d");
	remove("fs_ren_e");
	remove("fs_ren_f");
	remove("/src/fsrenf");

	if (write_file("fs_ren_a", "alpha") < 0)
		return;
	expect_int("rename-basic", rename("fs_ren_a", "fs_ren_b"), 0);
	fd = open("fs_ren_a", O_RDONLY);
	if (fd >= 0) {
		close(fd);
		fail("rename-removes-old-name");
	}
	expect_int("rename-basic-size", read_file("fs_ren_b", buf, sizeof(buf)), 5);
	expect_str("rename-basic-content", buf, "alpha");

	if (write_file("fs_ren_c", "old") < 0)
		return;
	if (write_file("fs_ren_d", "newer") < 0)
		return;
	expect_int("rename-overwrite", rename("fs_ren_c", "fs_ren_d"), 0);
	fd = open("fs_ren_c", O_RDONLY);
	if (fd >= 0) {
		close(fd);
		fail("rename-overwrite-removes-old");
	}
	expect_int("rename-overwrite-size", read_file("fs_ren_d", buf, sizeof(buf)), 3);
	expect_str("rename-overwrite-content", buf, "old");

	expect_int("rename-missing", rename("fs_ren_z", "fs_ren_e"), -1);
	fd = open("fs_ren_e", O_RDONLY);
	if (fd >= 0) {
		close(fd);
		fail("rename-missing-no-target");
	}

	if (write_file("fs_ren_f", "cross") < 0)
		return;
	expect_int("rename-cross-dir", rename("fs_ren_f", "/src/fsrenf"), 0);
	fd = open("fs_ren_f", O_RDONLY);
	if (fd >= 0) {
		close(fd);
		fail("rename-cross-removes-old");
	}
	expect_int("rename-cross-size", read_file("/src/fsrenf", buf, sizeof(buf)), 5);
	expect_str("rename-cross-content", buf, "cross");
}

static void slot_name(char *name, int slot)
{
	name[0] = 'f';
	name[1] = 's';
	name[2] = 'r';
	name[3] = 'a' + slot;
	name[4] = 0;
}

static void test_dirent_reuse(void)
{
	char name[8];
	char buf[8];
	int i;

	for (i = 0; i < 16; i++) {
		slot_name(name, i);
		remove(name);
		if (write_file(name, "a") < 0)
			return;
	}

	for (i = 0; i < 16; i++) {
		slot_name(name, i);
		expect_int("reuse-unlink", unlink(name), 0);
	}

	for (i = 0; i < 16; i++) {
		slot_name(name, i);
		if (write_file(name, "b") < 0)
			return;
	}

	for (i = 0; i < 16; i++) {
		slot_name(name, i);
		expect_int("reuse-read-size", read_file(name, buf, sizeof(buf)), 1);
		expect_str("reuse-content", buf, "b");
		remove(name);
	}
}

static void test_lseek_overwrite(void)
{
	const char *path = "fs_seek";
	char buf[16];
	struct stat st;
	int fd;

	remove(path);
	fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		fail("seek-open");
		return;
	}
	expect_int("seek-write", write(fd, "abcdef", 6), 6);
	expect_int("seek-cur-end", lseek(fd, 0, SEEK_CUR), 6);
	expect_int("seek-set", lseek(fd, 2, SEEK_SET), 2);
	expect_int("seek-overwrite", write(fd, "ZZ", 2), 2);
	expect_int("seek-after-write", lseek(fd, 0, SEEK_CUR), 4);
	expect_int("seek-end", lseek(fd, 0, SEEK_END), 6);
	expect_int("seek-fstat", fstat(fd, &st), 0);
	expect_file_stat("seek-size", &st, 6);
	expect_int("seek-rewind", lseek(fd, 0, SEEK_SET), 0);
	memset(buf, 0, sizeof(buf));
	expect_int("seek-read", read(fd, buf, 6), 6);
	close(fd);
	expect_str("seek-content", buf, "abZZef");
}

static void test_independent_open_offsets(void)
{
	const char *path = "fs_offset";
	char one[4];
	char two[4];
	int fd1;
	int fd2;

	if (write_file(path, "abcdef") < 0)
		return;
	fd1 = open(path, O_RDONLY);
	fd2 = open(path, O_RDONLY);
	if (fd1 < 0 || fd2 < 0) {
		fail("offset-open");
		if (fd1 >= 0)
			close(fd1);
		if (fd2 >= 0)
			close(fd2);
		return;
	}

	memset(one, 0, sizeof(one));
	memset(two, 0, sizeof(two));
	expect_int("offset-fd1-first", read(fd1, one, 2), 2);
	expect_str("offset-fd1-content", one, "ab");
	expect_int("offset-fd2-first", read(fd2, two, 3), 3);
	expect_str("offset-fd2-content", two, "abc");
	memset(one, 0, sizeof(one));
	expect_int("offset-fd1-second", read(fd1, one, 2), 2);
	expect_str("offset-fd1-second-content", one, "cd");

	close(fd1);
	close(fd2);
}

int main(void)
{
	test_create_read_stat();
	test_open_trunc_releases_tail();
	test_unlink_recreate();
	test_rename_file();
	test_dirent_reuse();
	test_lseek_overwrite();
	test_independent_open_offsets();

	if (failures) {
		printf("FS_CONTRACT_FAIL %d\n", failures);
		return 1;
	}
	printf("FS_CONTRACT_PASS\n");
	return 0;
}
