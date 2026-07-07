#include "fs.h"
#include "proc.h"
#include "types.h"

#define BUILD_FILE "/src/Buildfile"
#define RECIPE_CC "cc"
#define RECIPE_COPY "copy"
#define RECIPE_PHONY "phony"
#define MAX_BUILD_FILE 1024
#define MAX_TARGETS 32
#define NO_DEPS "-"
#define COPY_CAP 512
#define BUILD_SIG_PREFIX "/tmp/b"
#define BUILD_SIG_HEX_DIGITS 12
#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

#define STDOUT_FD 1

struct target
{
    char *name;
    char *recipe;
    char *source;
    char *output;
    char *group;
    char *deps;
    int visiting;
    int done;
};

static int build_target(struct target *targets, int count, struct target *target);
static struct target *find_target(struct target *targets, int count, const char *name);
static int compute_signature(struct target *targets, int count, const struct target *target, uint64 *sig);
static int target_current(struct target *targets, int count, const struct target *target);
static int run_recipe(struct target *targets, int count, const struct target *target);
static int open_input(const char *path);
static int open_output(const char *path);
static void unlink_file(const char *path);
static int stat_path(const char *path, struct stat *st);
static int file_exists(const char *path);
static int file_nonempty(const char *path);

static int str_len(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static uint64 hash_init(void)
{
    return FNV_OFFSET;
}

static uint64 hash_byte(uint64 hash, unsigned char c)
{
    hash ^= c;
    return hash * FNV_PRIME;
}

static uint64 hash_bytes(uint64 hash, const char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++)
        hash = hash_byte(hash, (unsigned char)buf[i]);
    return hash;
}

static uint64 hash_cstr(uint64 hash, const char *s)
{
    return hash_bytes(hash, s, str_len(s));
}

static void to_hex64(uint64 value, char *buf)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    for (i = 15; i >= 0; i--)
    {
        buf[i] = hex[value & 0xf];
        value >>= 4;
    }
    buf[16] = 0;
}

static int signature_path_for(const struct target *target, char *path, int cap)
{
    char hex[17];
    uint64 hash;
    int prefix_len = str_len(BUILD_SIG_PREFIX);

    if (cap < prefix_len + BUILD_SIG_HEX_DIGITS + 1)
        return -1;

    hash = hash_init();
    hash = hash_cstr(hash, target->name);
    hash = hash_byte(hash, 0xff);
    hash = hash_cstr(hash, target->recipe);
    hash = hash_byte(hash, 0xff);
    if (target->output && !str_eq(target->output, NO_DEPS))
        hash = hash_cstr(hash, target->output);
    to_hex64(hash, hex);

    for (int i = 0; i < prefix_len; i++)
        path[i] = BUILD_SIG_PREFIX[i];
    for (int i = 0; i < BUILD_SIG_HEX_DIGITS; i++)
        path[prefix_len + i] = hex[i + (16 - BUILD_SIG_HEX_DIGITS)];
    path[prefix_len + BUILD_SIG_HEX_DIGITS] = 0;
    return 0;
}
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(a0_asm)
                     : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                     : "memory");
    return a0_asm;
}

static int open_rw(const char *path)
{
    return (int)syscall(SYS_open, (uint64)path, O_RDWR | O_CREAT | O_TRUNC, 0);
}

static int read_u64_file(const char *path, uint64 *value)
{
    int fd = open_input(path);
    int n;

    if (fd < 0)
        return -1;
    n = (int)syscall(SYS_read, fd, (uint64)value, (uint64)sizeof(*value));
    syscall(SYS_close, fd, 0, 0);
    return n == (int)sizeof(*value) ? 0 : -1;
}

static int write_u64_file(const char *path, uint64 value)
{
    int fd = open_rw(path);
    int n;

    if (fd < 0)
        return -1;
    n = (int)syscall(SYS_write, fd, (uint64)&value, (uint64)sizeof(value));
    syscall(SYS_close, fd, 0, 0);
    return n == (int)sizeof(value) ? 0 : -1;
}

static int hash_file(const char *path, uint64 *hash)
{
    char buf[COPY_CAP];
    int fd;
    int n;

    fd = open_input(path);
    if (fd < 0)
        return -1;

    while ((n = (int)syscall(SYS_read, fd, (uint64)buf, sizeof(buf))) > 0)
        *hash = hash_bytes(*hash, buf, n);

    syscall(SYS_close, fd, 0, 0);
    return n < 0 ? -1 : 0;
}

static int hash_dependency(struct target *targets, int count, uint64 *hash, const char *dep)
{
    struct target *dep_target;
    const char *path;

    if (dep[0] == '@')
    {
        dep_target = find_target(targets, count, dep + 1);
        if (!dep_target)
            return -1;
        path = dep_target->output;
    }
    else
    {
        path = dep;
    }

    *hash = hash_byte(*hash, 0xff);
    return hash_file(path, hash);
}

static int compute_signature(struct target *targets, int count, const struct target *target, uint64 *sig)
{
    const char *p;
    const char *source = target->source;
    const char *output = target->output;

    *sig = hash_init();
    *sig = hash_cstr(*sig, target->name);
    *sig = hash_byte(*sig, 0xff);
    *sig = hash_cstr(*sig, target->recipe);
    *sig = hash_byte(*sig, 0xff);
    if (output && !str_eq(output, NO_DEPS))
        *sig = hash_cstr(*sig, output);
    *sig = hash_byte(*sig, 0xff);
    if (source && !str_eq(source, NO_DEPS))
    {
        if (hash_file(source, sig) < 0)
            return -1;
    }

    p = target->deps;
    if (!p || str_eq(p, NO_DEPS))
        return 0;

    while (*p)
    {
        const char *start;
        int len;
        char dep[128];

        while (*p == ',')
            p++;
        start = p;
        while (*p && *p != ',')
            p++;
        len = p - start;
        if (len > 0)
        {
            if (len >= (int)sizeof(dep))
                return -1;
            for (int i = 0; i < len; i++)
                dep[i] = start[i];
            dep[len] = 0;
            if (hash_dependency(targets, count, sig, dep) < 0)
                return -1;
        }
        if (*p)
            p++;
    }

    return 0;
}

static void write_bytes(const char *s, int n)
{
    syscall(SYS_write, STDOUT_FD, (uint64)s, (uint64)n);
}

static void write_str(const char *s)
{
    write_bytes(s, str_len(s));
}

static int open_output(const char *path)
{
    return (int)syscall(SYS_open, (uint64)path, O_WRONLY | O_CREAT | O_TRUNC, 0);
}

static int open_input(const char *path)
{
    return (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);
}

static void unlink_file(const char *path)
{
    syscall(SYS_unlink, (uint64)path, 0, 0);
}

static int stat_path(const char *path, struct stat *st)
{
    int fd = open_input(path);
    int status;

    if (fd < 0)
        return -1;
    status = (int)syscall(SYS_fstat, fd, (uint64)st, 0);
    syscall(SYS_close, fd, 0, 0);
    return status;
}

static int file_exists(const char *path)
{
    struct stat st;

    return stat_path(path, &st) == 0;
}

static int file_nonempty(const char *path)
{
    struct stat st;

    return stat_path(path, &st) == 0 && st.type == T_FILE && st.size > 0;
}

static int wait_for_child(int *status)
{
    int child_status = -1;
    int pid = (int)syscall(SYS_wait, (uint64)&child_status, 0, 0);

    if (status)
        *status = child_status;
    return pid;
}

static int run_program(char **argv, const char *out_path)
{
    int status = -1;
    int pid = (int)syscall(SYS_fork, 0, 0, 0);

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        if (out_path)
        {
            int fd;
            syscall(SYS_close, STDOUT_FD, 0, 0);
            fd = open_output(out_path);
            if (fd != STDOUT_FD)
                syscall(SYS_exit, -1, 0, 0);
        }

        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
        syscall(SYS_exit, -1, 0, 0);
    }

    if (wait_for_child(&status) < 0)
        return -1;
    return status;
}

static int read_build_file(char *buf, int cap)
{
    int fd;
    int total = 0;
    int n;

    fd = open_input(BUILD_FILE);
    if (fd < 0)
    {
        write_str("missing build file: ");
        write_str(BUILD_FILE);
        write_str("\n");
        return -1;
    }

    while (total + 1 < cap)
    {
        n = (int)syscall(SYS_read, fd, (uint64)(buf + total), cap - 1 - total);
        if (n <= 0)
            break;
        total += n;
    }
    syscall(SYS_close, fd, 0, 0);

    if (total + 1 >= cap)
    {
        write_str("build file too large\n");
        return -1;
    }
    buf[total] = 0;
    return total;
}

static char *next_field(char **cursor)
{
    char *p = *cursor;
    char *start;

    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    if (*p == 0 || *p == '#')
    {
        *cursor = p;
        return 0;
    }

    start = p;
    while (*p && !is_space(*p))
        p++;
    if (*p)
        *p++ = 0;
    *cursor = p;
    return start;
}

static int parse_build_file(char *buf, struct target *targets, int max_targets)
{
    char *p = buf;
    int count = 0;

    while (*p)
    {
        char *line = p;
        char *name;
        char *second;
        char *third;
        char *fourth;
        char *fifth;
        char *sixth;
        char *extra;

        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            *p++ = 0;

        while (*line == ' ' || *line == '\t' || *line == '\r')
            line++;
        if (*line == 0 || *line == '#')
            continue;

        name = next_field(&line);
        second = next_field(&line);
        third = next_field(&line);
        fourth = next_field(&line);
        fifth = next_field(&line);
        sixth = next_field(&line);
        extra = next_field(&line);
        if (!name || !second || !third || !fourth || extra)
        {
            write_str("bad build file line\n");
            return -1;
        }

        if (count >= max_targets)
        {
            write_str("too many build targets\n");
            return -1;
        }

        targets[count].name = name;
        if (fifth)
        {
            targets[count].recipe = second;
            targets[count].source = third;
            targets[count].output = fourth;
            targets[count].group = fifth;
            targets[count].deps = sixth ? sixth : third;
        }
        else
        {
            targets[count].recipe = RECIPE_CC;
            targets[count].source = second;
            targets[count].output = third;
            targets[count].group = fourth;
            targets[count].deps = second;
        }
        targets[count].visiting = 0;
        targets[count].done = 0;
        count++;
    }

    return count;
}

static int load_targets(struct target *targets, int max_targets)
{
    static char build_file[MAX_BUILD_FILE];
    int n;

    n = read_build_file(build_file, sizeof(build_file));
    if (n < 0)
        return -1;
    return parse_build_file(build_file, targets, max_targets);
}

static struct target *find_target(struct target *targets, int count, const char *name)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (str_eq(targets[i].name, name))
            return &targets[i];
    }
    return 0;
}

static int check_file_dependency(const struct target *target, const char *path)
{
    if (file_exists(path))
        return 0;

    write_str("missing dependency for ");
    write_str(target->name);
    write_str(": ");
    write_str(path);
    write_str("\n");
    return -1;
}

static int check_dependency(struct target *targets, int count, struct target *target, const char *dep)
{
    struct target *dep_target;

    if (dep[0] != '@')
        return check_file_dependency(target, dep);

    dep_target = find_target(targets, count, dep + 1);
    if (!dep_target)
    {
        write_str("missing target dependency for ");
        write_str(target->name);
        write_str(": ");
        write_str(dep);
        write_str("\n");
        return -1;
    }

    return build_target(targets, count, dep_target);
}

static int check_dependency_range(struct target *targets, int count, struct target *target, const char *start, int len)
{
    char dep[128];
    int i;

    if (len <= 0)
        return 0;
    if (len >= (int)sizeof(dep))
    {
        write_str("dependency name too long for ");
        write_str(target->name);
        write_str("\n");
        return -1;
    }

    for (i = 0; i < len; i++)
        dep[i] = start[i];
    dep[len] = 0;
    return check_dependency(targets, count, target, dep);
}

static int check_dependencies(struct target *targets, int target_count, struct target *target)
{
    const char *p = target->deps;
    int dep_count = 0;

    if (!p || str_eq(p, NO_DEPS))
        return 0;

    while (*p)
    {
        const char *start;

        while (*p == ',')
            p++;
        start = p;
        while (*p && *p != ',')
            p++;
        if (p == start)
        {
            if (*p)
                p++;
            continue;
        }

        dep_count++;
        if (check_dependency_range(targets, target_count, target, start, p - start) < 0)
            return -1;
        if (*p)
            p++;
    }

    return dep_count > 0 ? 0 : -1;
}

static int target_current(struct target *targets, int count, const struct target *target)
{
    char sig_path[32];
    uint64 old_sig;
    uint64 new_sig;

    if (str_eq(target->recipe, RECIPE_PHONY))
        return 0;
    if (signature_path_for(target, sig_path, sizeof(sig_path)) < 0)
        return 0;
    if (!file_nonempty(target->output))
        return 0;
    if (read_u64_file(sig_path, &old_sig) < 0)
        return 0;
    if (compute_signature(targets, count, (struct target *)target, &new_sig) < 0)
        return 0;
    return old_sig == new_sig;
}

static int copy_file(const char *src, const char *dst)
{
    char buf[COPY_CAP];
    int in_fd;
    int out_fd;
    int n;

    in_fd = open_input(src);
    if (in_fd < 0)
        return -1;

    out_fd = open_output(dst);
    if (out_fd < 0)
    {
        syscall(SYS_close, in_fd, 0, 0);
        return -1;
    }

    while ((n = (int)syscall(SYS_read, in_fd, (uint64)buf, sizeof(buf))) > 0)
    {
        int written = 0;

        while (written < n)
        {
            int m = (int)syscall(SYS_write, out_fd, (uint64)(buf + written), n - written);
            if (m <= 0)
            {
                syscall(SYS_close, in_fd, 0, 0);
                syscall(SYS_close, out_fd, 0, 0);
                return -1;
            }
            written += m;
        }
    }

    if (n < 0)
    {
        syscall(SYS_close, in_fd, 0, 0);
        syscall(SYS_close, out_fd, 0, 0);
        return -1;
    }

    if ((int)syscall(SYS_close, in_fd, 0, 0) < 0)
        return -1;
    if ((int)syscall(SYS_close, out_fd, 0, 0) < 0)
        return -1;
    return 0;
}

static int run_recipe(struct target *targets, int count, const struct target *target)
{
    char *argv[] = {"/bin/tcc", target->source, "-o", target->output, 0};
    char sig_path[32];
    uint64 sig;
    int status;

    if (str_eq(target->recipe, RECIPE_CC))
    {
        unlink_file(target->output);
        status = run_program(argv, "build.log");
        if (status == 0 && compute_signature(targets, count, (struct target *)target, &sig) == 0 &&
            signature_path_for(target, sig_path, sizeof(sig_path)) == 0)
            write_u64_file(sig_path, sig);
        return status;
    }

    if (str_eq(target->recipe, RECIPE_COPY))
    {
        status = copy_file(target->source, target->output);
        if (status == 0 && compute_signature(targets, count, (struct target *)target, &sig) == 0 &&
            signature_path_for(target, sig_path, sizeof(sig_path)) == 0)
            write_u64_file(sig_path, sig);
        return status;
    }

    if (str_eq(target->recipe, RECIPE_PHONY))
        return 0;

    write_str("unknown recipe: ");
    write_str(target->recipe);
    write_str("\n");
    return -1;
}

static int build_target(struct target *targets, int count, struct target *target)
{
    int status;

    if (target->done)
        return 0;
    if (target->visiting)
    {
        write_str("dependency cycle at ");
        write_str(target->name);
        write_str("\n");
        return -1;
    }

    target->visiting = 1;

    if (check_dependencies(targets, count, target) < 0)
    {
        write_str("BUILD_FAIL ");
        write_str(target->name);
        write_str("\n");
        target->visiting = 0;
        return -1;
    }

    if (target_current(targets, count, target))
    {
        write_str("BUILD_SKIP ");
        write_str(target->name);
        write_str("\n");
        target->visiting = 0;
        target->done = 1;
        return 0;
    }

    status = run_recipe(targets, count, target);
    if (status == 0)
    {
        write_str("BUILD_PASS ");
        write_str(target->name);
        write_str("\n");
        target->visiting = 0;
        target->done = 1;
        return 0;
    }

    write_str("BUILD_FAIL ");
    write_str(target->name);
    write_str("\n");
    target->visiting = 0;
    return -1;
}

static int has_group(struct target *targets, int count, const char *group)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (str_eq(targets[i].group, group))
            return 1;
    }
    return 0;
}

static int build_group(struct target *targets, int count, const char *group)
{
    int failed = 0;
    int built = 0;
    int i;

    for (i = 0; i < count; i++)
    {
        if (!str_eq(targets[i].group, group))
            continue;
        built++;
        if (build_target(targets, count, &targets[i]) < 0)
            failed++;
    }

    if (built == 0)
        return -1;
    return failed == 0 ? 0 : -1;
}

static void print_usage(void)
{
    write_str("usage: build [list|help|GROUP|TARGET]\n");
}

static int group_seen(struct target *targets, int upto, const char *group)
{
    int i;

    for (i = 0; i < upto; i++)
    {
        if (str_eq(targets[i].group, group))
            return 1;
    }
    return 0;
}

static void print_list(struct target *targets, int count)
{
    int i;
    int j;

    for (i = 0; i < count; i++)
    {
        if (group_seen(targets, i, targets[i].group))
            continue;

        write_str(targets[i].group);
        write_str(":\n");
        for (j = 0; j < count; j++)
        {
            if (str_eq(targets[j].group, targets[i].group))
            {
                write_str("  ");
                write_str(targets[j].name);
                write_str("\n");
            }
        }
    }
}

void main(int argc, char *argv[])
{
    struct target targets[MAX_TARGETS];
    struct target *target;
    int count;
    int status = -1;

    if (argc != 2)
    {
        print_usage();
        syscall(SYS_exit, 1, 0, 0);
    }

    if (str_eq(argv[1], "help"))
    {
        print_usage();
        syscall(SYS_exit, 0, 0, 0);
    }

    count = load_targets(targets, MAX_TARGETS);
    if (count < 0)
        syscall(SYS_exit, 1, 0, 0);

    if (str_eq(argv[1], "list"))
    {
        print_list(targets, count);
        syscall(SYS_exit, 0, 0, 0);
    }
    target = find_target(targets, count, argv[1]);
    if (target)
        status = build_target(targets, count, target);
    else if (has_group(targets, count, argv[1]))
        status = build_group(targets, count, argv[1]);
    else
    {
        print_usage();
        write_str("unknown target: ");
        write_str(argv[1]);
        write_str("\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    syscall(SYS_exit, status == 0 ? 0 : 1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
