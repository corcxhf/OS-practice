#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define STDERR_FD 2
#define MAX_TOOL_ARGS 96
#define MAX_INPUTS 16
#define MAX_EXTRAS 32
#define ITEM_INPUT 1
#define ITEM_LINK_ARG 2

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

static int str_len(const char *s)
{
    int len = 0;
    while (s && s[len])
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

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static int has_suffix(const char *s, const char *suffix)
{
    int slen = str_len(s);
    int tlen = str_len(suffix);
    if (slen < tlen)
        return 0;
    return str_eq(s + slen - tlen, suffix);
}

static void write_str(int fd, const char *s)
{
    syscall(SYS_write, fd, (uint64)s, str_len(s));
}

static int append_arg(char **out, int *count, char *arg)
{
    if (*count >= MAX_TOOL_ARGS - 1)
        return -1;
    out[(*count)++] = arg;
    out[*count] = 0;
    return 0;
}

static int append_item(char **out, int *kinds, int *count, char *arg, int kind)
{
    if (*count >= MAX_TOOL_ARGS - 1)
        return -1;
    out[*count] = arg;
    kinds[*count] = kind;
    (*count)++;
    out[*count] = 0;
    return 0;
}

static int append_wl_items(char *arg, char **out, int *kinds, int *count)
{
    char *p = arg + 4;

    if (!*p)
        return -1;
    while (*p)
    {
        if (append_item(out, kinds, count, p, ITEM_LINK_ARG) < 0)
            return -1;
        while (*p && *p != ',')
            p++;
        if (*p == ',')
            *p++ = 0;
    }
    return 0;
}

static void temp_obj_name(char *buf, int index)
{
    const char *prefix = "__myosgcc";
    int i = 0;
    while (prefix[i])
    {
        buf[i] = prefix[i];
        i++;
    }
    if (index >= 10)
        buf[i++] = (char)('0' + index / 10);
    buf[i++] = (char)('0' + index % 10);
    buf[i++] = '.';
    buf[i++] = 'o';
    buf[i] = 0;
}

static void temp_asm_name(char *buf, int index)
{
    const char *prefix = "__myosgccS";
    int i = 0;
    while (prefix[i])
    {
        buf[i] = prefix[i];
        i++;
    }
    if (index >= 10)
        buf[i++] = (char)('0' + index / 10);
    buf[i++] = (char)('0' + index % 10);
    buf[i++] = '.';
    buf[i++] = 's';
    buf[i] = 0;
}

static char *default_obj_name(char *buf, char *input)
{
    char *base = input;
    int len;
    int end;
    int i;

    for (char *p = input; *p; p++)
        if (*p == '/')
            base = p + 1;
    len = str_len(base);
    end = len;
    if (len > 2 && base[len - 2] == '.')
        end = len - 2;
    if (end > 58)
        end = 58;
    for (i = 0; i < end; i++)
        buf[i] = base[i];
    buf[i++] = '.';
    buf[i++] = 'o';
    buf[i] = 0;
    return buf;
}

static int run_program(char **argv, char **envp)
{
    int status = -1;
    int pid = (int)syscall(SYS_fork, 0, 0, 0);

    if (pid < 0)
        return -1;
    if (pid == 0)
    {
        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, (uint64)envp);
        syscall(SYS_exit, -1, 0, 0);
    }
    if ((int)syscall(SYS_wait, (uint64)&status, 0, 0) < 0)
        return -1;
    return status;
}

static void usage(void)
{
    write_str(STDERR_FD, "usage: myos-gcc [-c] [-o output] file...\n");
    syscall(SYS_exit, 1, 0, 0);
}

static int compile_c(char *input, char *output, char **c_flags, int c_flag_count, char **envp)
{
    char *argv[MAX_TOOL_ARGS];
    int argc = 0;

    if (append_arg(argv, &argc, "/bin/tcc") < 0 ||
        append_arg(argv, &argc, "-c") < 0)
        return -1;
    for (int i = 0; i < c_flag_count; i++)
        if (append_arg(argv, &argc, c_flags[i]) < 0)
            return -1;
    if (append_arg(argv, &argc, input) < 0 ||
        append_arg(argv, &argc, "-o") < 0 ||
        append_arg(argv, &argc, output) < 0)
        return -1;
    return run_program(argv, envp);
}

static int preprocess_asm(char *input, char *output, char **c_flags, int c_flag_count, char **envp)
{
    char *argv[MAX_TOOL_ARGS];
    int argc = 0;

    if (append_arg(argv, &argc, "/bin/tcc") < 0 ||
        append_arg(argv, &argc, "-E") < 0)
        return -1;
    for (int i = 0; i < c_flag_count; i++)
        if (append_arg(argv, &argc, c_flags[i]) < 0)
            return -1;
    if (append_arg(argv, &argc, input) < 0 ||
        append_arg(argv, &argc, "-o") < 0 ||
        append_arg(argv, &argc, output) < 0)
        return -1;
    return run_program(argv, envp);
}

static int run_as(char *input, char *output, char **envp)
{
    char *argv[MAX_TOOL_ARGS];
    int argc = 0;

    if (append_arg(argv, &argc, "/bin/myos-as") < 0 ||
        append_arg(argv, &argc, input) < 0 ||
        append_arg(argv, &argc, "-o") < 0 ||
        append_arg(argv, &argc, output) < 0)
        return -1;
    return run_program(argv, envp);
}

static int assemble_s(char *input, char *output, int index,
                      char **c_flags, int c_flag_count, char **envp)
{
    char temp_asm[16];
    int status;

    if (!has_suffix(input, ".S"))
        return run_as(input, output, envp);

    temp_asm_name(temp_asm, index);
    status = preprocess_asm(input, temp_asm, c_flags, c_flag_count, envp);
    if (status != 0)
    {
        syscall(SYS_unlink, (uint64)temp_asm, 0, 0);
        return status;
    }

    status = run_as(temp_asm, output, envp);
    syscall(SYS_unlink, (uint64)temp_asm, 0, 0);
    return status;
}

static int preprocess_inputs(char **inputs, int input_count, char *output,
                             char **c_flags, int c_flag_count, char **envp)
{
    char *argv[MAX_TOOL_ARGS];
    int argc = 0;

    if (append_arg(argv, &argc, "/bin/tcc") < 0 ||
        append_arg(argv, &argc, "-E") < 0)
        return -1;
    for (int i = 0; i < c_flag_count; i++)
        if (append_arg(argv, &argc, c_flags[i]) < 0)
            return -1;
    for (int i = 0; i < input_count; i++)
        if (append_arg(argv, &argc, inputs[i]) < 0)
            return -1;
    if (output)
    {
        if (append_arg(argv, &argc, "-o") < 0 ||
            append_arg(argv, &argc, output) < 0)
            return -1;
    }
    return run_program(argv, envp);
}

static int link_objects(char **link_args, int link_arg_count,
                        int nostartfiles, int nodefaultlibs, char *output, char **envp)
{
    char *argv[MAX_TOOL_ARGS];
    int argc = 0;

    if (append_arg(argv, &argc, "/bin/myos-ld") < 0)
        return -1;
    if (!nostartfiles)
    {
        if (append_arg(argv, &argc, "/lib/crt1.o") < 0 ||
            append_arg(argv, &argc, "/lib/crti.o") < 0)
            return -1;
    }
    for (int i = 0; i < link_arg_count; i++)
        if (append_arg(argv, &argc, link_args[i]) < 0)
            return -1;
    if (!nodefaultlibs)
    {
        if (append_arg(argv, &argc, "--start-group") < 0 ||
            append_arg(argv, &argc, "-lc") < 0 ||
            append_arg(argv, &argc, "-lgcc") < 0 ||
            append_arg(argv, &argc, "--end-group") < 0)
            return -1;
    }
    if (!nostartfiles)
    {
        if (append_arg(argv, &argc, "/lib/crtn.o") < 0)
            return -1;
    }
    if (append_arg(argv, &argc, "-o") < 0 ||
        append_arg(argv, &argc, output) < 0)
        return -1;
    return run_program(argv, envp);
}

void main(int argc, char *argv[], char *envp[])
{
    char *inputs[MAX_INPUTS];
    char *items[MAX_TOOL_ARGS];
    int item_kinds[MAX_TOOL_ARGS];
    char *link_args[MAX_TOOL_ARGS];
    char temp_names[MAX_INPUTS][16];
    char compile_outputs[MAX_INPUTS][64];
    int temp_count = 0;
    char *c_flags[MAX_EXTRAS];
    char *output = 0;
    int input_count = 0;
    int c_flag_count = 0;
    int item_count = 0;
    int link_arg_count = 0;
    int compile_only = 0;
    int preprocess_only = 0;
    int nostartfiles = 0;
    int nodefaultlibs = 0;
    int verbose = 0;
    int status;

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (str_eq(arg, "--version"))
        {
            write_str(STDOUT_FD, "myos-gcc: experimental driver using /bin/tcc, /bin/myos-as, /bin/myos-ld\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-v"))
        {
            write_str(STDERR_FD, "myos-gcc: experimental driver using /bin/tcc, /bin/myos-as, /bin/myos-ld\n");
            verbose = 1;
            continue;
        }
        if (str_eq(arg, "-dumpmachine"))
        {
            write_str(STDOUT_FD, "riscv64-unknown-elf\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-dumpversion") || str_eq(arg, "-dumpfullversion"))
        {
            write_str(STDOUT_FD, "13.2.0\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-sysroot"))
        {
            write_str(STDOUT_FD, "/\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-search-dirs"))
        {
            write_str(STDOUT_FD, "install: /\nprograms: =/bin\nlibraries: =/lib\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-multi-directory"))
        {
            write_str(STDOUT_FD, ".\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-multi-lib"))
        {
            write_str(STDOUT_FD, ".;\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-prog-name=as"))
        {
            write_str(STDOUT_FD, "/bin/myos-as\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-prog-name=ld"))
        {
            write_str(STDOUT_FD, "/bin/myos-ld\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-prog-name=cpp"))
        {
            write_str(STDOUT_FD, "/bin/cpp\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-prog-name=ar"))
        {
            write_str(STDOUT_FD, "/bin/myos-ar\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-prog-name=ranlib"))
        {
            write_str(STDOUT_FD, "/bin/myos-ranlib\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-libgcc-file-name") || str_eq(arg, "-print-file-name=libgcc.a"))
        {
            write_str(STDOUT_FD, "/lib/libgcc.a\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-file-name=libc.a"))
        {
            write_str(STDOUT_FD, "/lib/libc.a\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-file-name=crt1.o"))
        {
            write_str(STDOUT_FD, "/lib/crt1.o\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-file-name=crti.o"))
        {
            write_str(STDOUT_FD, "/lib/crti.o\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-file-name=crtn.o"))
        {
            write_str(STDOUT_FD, "/lib/crtn.o\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-print-file-name=include"))
        {
            write_str(STDOUT_FD, "/include\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(arg, "-c"))
        {
            compile_only = 1;
            continue;
        }
        if (str_eq(arg, "-E"))
        {
            preprocess_only = 1;
            continue;
        }
        if (str_eq(arg, "-nostdlib"))
        {
            nostartfiles = 1;
            nodefaultlibs = 1;
            continue;
        }
        if (str_eq(arg, "-nostartfiles"))
        {
            nostartfiles = 1;
            continue;
        }
        if (str_eq(arg, "-nodefaultlibs"))
        {
            nodefaultlibs = 1;
            continue;
        }
        if (str_eq(arg, "-r"))
        {
            nostartfiles = 1;
            nodefaultlibs = 1;
            if (append_item(items, item_kinds, &item_count, arg, ITEM_LINK_ARG) < 0)
                usage();
            continue;
        }
        if (str_eq(arg, "-o"))
        {
            if (i + 1 >= argc)
                usage();
            output = argv[++i];
            continue;
        }
        if (starts_with(arg, "-o") && arg[2])
        {
            output = arg + 2;
            continue;
        }
        if (str_eq(arg, "-I") || str_eq(arg, "-D") || str_eq(arg, "-U") ||
            str_eq(arg, "-include") || str_eq(arg, "-isystem"))
        {
            if (i + 1 >= argc || c_flag_count + 2 > MAX_EXTRAS)
                usage();
            c_flags[c_flag_count++] = arg;
            c_flags[c_flag_count++] = argv[++i];
            continue;
        }
        if (starts_with(arg, "-I") || starts_with(arg, "-D") || starts_with(arg, "-U") ||
            starts_with(arg, "-include") || starts_with(arg, "-isystem"))
        {
            if (c_flag_count >= MAX_EXTRAS)
                usage();
            c_flags[c_flag_count++] = arg;
            continue;
        }
        if (str_eq(arg, "-nostdinc"))
        {
            if (c_flag_count >= MAX_EXTRAS)
                usage();
            c_flags[c_flag_count++] = arg;
            continue;
        }
        if (str_eq(arg, "-L") || str_eq(arg, "-l"))
        {
            if (i + 1 >= argc ||
                append_item(items, item_kinds, &item_count, arg, ITEM_LINK_ARG) < 0 ||
                append_item(items, item_kinds, &item_count, argv[++i], ITEM_LINK_ARG) < 0)
                usage();
            continue;
        }
        if (starts_with(arg, "-L") || starts_with(arg, "-l"))
        {
            if (append_item(items, item_kinds, &item_count, arg, ITEM_LINK_ARG) < 0)
                usage();
            continue;
        }
        if (starts_with(arg, "-Wl,"))
        {
            if (append_wl_items(arg, items, item_kinds, &item_count) < 0)
                usage();
            continue;
        }
        if (str_eq(arg, "-Xlinker"))
        {
            if (i + 1 >= argc ||
                append_item(items, item_kinds, &item_count, argv[++i], ITEM_LINK_ARG) < 0)
                usage();
            continue;
        }
        if (arg[0] == '-')
        {
            if (str_eq(arg, "-g") || starts_with(arg, "-O") || starts_with(arg, "-W") ||
                starts_with(arg, "-f") || starts_with(arg, "-m") || starts_with(arg, "-std=") ||
                str_eq(arg, "-static"))
                continue;
            write_str(STDERR_FD, "myos-gcc: unsupported option ");
            write_str(STDERR_FD, arg);
            write_str(STDERR_FD, "\n");
            syscall(SYS_exit, 1, 0, 0);
        }
        if (input_count >= MAX_INPUTS)
            usage();
        inputs[input_count++] = arg;
        if (append_item(items, item_kinds, &item_count, arg, ITEM_INPUT) < 0)
            usage();
    }

    if (input_count == 0)
    {
        if (verbose)
            syscall(SYS_exit, 0, 0, 0);
        usage();
    }
    if (compile_only && output && input_count != 1)
    {
        write_str(STDERR_FD, "myos-gcc: cannot specify -o with multiple -c inputs\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    if (preprocess_only && output && input_count != 1)
    {
        write_str(STDERR_FD, "myos-gcc: cannot specify -o with multiple -E inputs\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    if (preprocess_only)
    {
        status = preprocess_inputs(inputs, input_count, output, c_flags, c_flag_count, envp);
        syscall(SYS_exit, status, 0, 0);
    }
    if (!output && !compile_only)
        output = "a.out";

    if (compile_only)
    {
        for (int i = 0; i < input_count; i++)
        {
            char *input = inputs[i];
            char *obj_output = output ? output : default_obj_name(compile_outputs[i], input);

            if (has_suffix(input, ".c"))
                status = compile_c(input, obj_output, c_flags, c_flag_count, envp);
            else if (has_suffix(input, ".s") || has_suffix(input, ".S"))
                status = assemble_s(input, obj_output, i, c_flags, c_flag_count, envp);
            else
            {
                write_str(STDERR_FD, "myos-gcc: unsupported -c input ");
                write_str(STDERR_FD, input);
                write_str(STDERR_FD, "\n");
                status = 1;
            }
            if (status != 0)
                syscall(SYS_exit, status, 0, 0);
        }
        syscall(SYS_exit, status, 0, 0);
    }

    for (int i = 0; i < item_count; i++)
    {
        char *input = items[i];
        if (item_kinds[i] == ITEM_LINK_ARG)
        {
            if (append_arg(link_args, &link_arg_count, input) < 0)
                usage();
            continue;
        }
        if (has_suffix(input, ".o") || has_suffix(input, ".a"))
        {
            if (append_arg(link_args, &link_arg_count, input) < 0)
                usage();
        }
        else if (has_suffix(input, ".c"))
        {
            temp_obj_name(temp_names[temp_count], temp_count);
            status = compile_c(input, temp_names[temp_count], c_flags, c_flag_count, envp);
            if (status != 0)
                syscall(SYS_exit, status, 0, 0);
            if (append_arg(link_args, &link_arg_count, temp_names[temp_count++]) < 0)
                usage();
        }
        else if (has_suffix(input, ".s") || has_suffix(input, ".S"))
        {
            temp_obj_name(temp_names[temp_count], temp_count);
            status = assemble_s(input, temp_names[temp_count], temp_count, c_flags, c_flag_count, envp);
            if (status != 0)
                syscall(SYS_exit, status, 0, 0);
            if (append_arg(link_args, &link_arg_count, temp_names[temp_count++]) < 0)
                usage();
        }
        else
        {
            write_str(STDERR_FD, "myos-gcc: unsupported input ");
            write_str(STDERR_FD, input);
            write_str(STDERR_FD, "\n");
            syscall(SYS_exit, 1, 0, 0);
        }
    }

    status = link_objects(link_args, link_arg_count, nostartfiles, nodefaultlibs, output, envp);
    for (int i = 0; i < temp_count; i++)
        syscall(SYS_unlink, (uint64)temp_names[i], 0, 0);
    syscall(SYS_exit, status, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
