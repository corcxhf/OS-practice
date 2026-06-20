#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_CAP 32
#define COMMAND_CAP 512

struct buffer {
    char **line;
    int len;
    int cap;
};

static char *dup_range(const char *s, int n) {
    char *out = malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, s, (size_t)n);
    out[n] = '\0';
    return out;
}

static char *dup_string(const char *s) {
    return dup_range(s, (int)strlen(s));
}

static void strip_newline(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static int has_text(const char *s) {
    return s && *s != '\0';
}

static int ensure_cap(struct buffer *b, int need) {
    char **next;
    int cap = b->cap ? b->cap : INITIAL_CAP;

    while (cap < need)
        cap *= 2;
    if (cap == b->cap)
        return 0;

    next = realloc(b->line, (size_t)cap * sizeof(char *));
    if (!next)
        return -1;
    b->line = next;
    b->cap = cap;
    return 0;
}

static int append_line(struct buffer *b, const char *s) {
    if (ensure_cap(b, b->len + 1) < 0)
        return -1;
    b->line[b->len] = dup_string(s);
    if (!b->line[b->len])
        return -1;
    b->len++;
    return 0;
}

static int insert_line(struct buffer *b, int at, const char *s) {
    int i;

    if (at < 0) at = 0;
    if (at > b->len) at = b->len;
    if (ensure_cap(b, b->len + 1) < 0)
        return -1;
    for (i = b->len; i > at; i--)
        b->line[i] = b->line[i - 1];
    b->line[at] = dup_string(s);
    if (!b->line[at])
        return -1;
    b->len++;
    return 0;
}

static void delete_line(struct buffer *b, int at) {
    int i;

    if (at < 0 || at >= b->len)
        return;
    free(b->line[at]);
    for (i = at; i + 1 < b->len; i++)
        b->line[i] = b->line[i + 1];
    b->len--;
}

static void free_buffer(struct buffer *b) {
    int i;

    for (i = 0; i < b->len; i++)
        free(b->line[i]);
    free(b->line);
}

static int read_file(struct buffer *b, const char *path) {
    FILE *f = fopen(path, "r");
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    if (!f)
        return 0;

    while ((n = getline(&line, &cap, f)) >= 0) {
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (append_line(b, line) < 0) {
            free(line);
            fclose(f);
            return -1;
        }
    }
    free(line);
    fclose(f);
    return 0;
}

static int write_file(struct buffer *b, const char *path) {
    FILE *f = fopen(path, "w");
    int i;

    if (!f)
        return -1;
    for (i = 0; i < b->len; i++) {
        fputs(b->line[i], f);
        fputc('\n', f);
    }
    return fclose(f);
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  p              print file with line numbers\n");
    printf("  p N            print line N\n");
    printf("  a TEXT         append TEXT after current line\n");
    printf("  a              append lines until a single . line\n");
    printf("  i N TEXT       insert TEXT before line N\n");
    printf("  i N            insert lines before line N until .\n");
    printf("  r N TEXT       replace line N with TEXT\n");
    printf("  d N            delete line N\n");
    printf("  w              save file\n");
    printf("  q              quit\n");
    printf("  q!             quit without saving\n");
    printf("  h              help\n");
}

static int read_command(char *buf, int cap) {
    int len = 0;

    for (;;) {
        char c;
        if (read(0, &c, 1) != 1)
            continue;
        if (c == '\r')
            c = '\n';
        if (c == '\n') {
            buf[len] = '\0';
            write(1, "\n", 1);
            return len;
        }
        if (c == 3)
            return -1;
        if (c == 127 || c == '\b') {
            if (len > 0) {
                len--;
                write(1, "\b \b", 3);
            }
            continue;
        }
        if (c >= 32 && c < 127 && len < cap - 1) {
            buf[len++] = c;
            write(1, &c, 1);
        }
    }
}

static int read_text_block(struct buffer *b, int at, int *current) {
    char cmd[COMMAND_CAP];
    int inserted = 0;

    if (at < 0) at = 0;
    if (at > b->len) at = b->len;
    printf("Enter text. End with a single . line.\n");
    for (;;) {
        int nread;

        printf(". ");
        nread = read_command(cmd, sizeof(cmd));
        if (nread < 0)
            return inserted ? inserted : -1;
        if (strcmp(cmd, ".") == 0)
            return inserted;
        if (insert_line(b, at + inserted, cmd) < 0) {
            printf("Out of memory\n");
            return inserted ? inserted : -1;
        }
        *current = at + inserted;
        inserted++;
    }
}

static int parse_number(const char **p) {
    int n = 0;

    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        (*p)++;
    }
    while (**p == ' ')
        (*p)++;
    return n;
}

static void print_lines(struct buffer *b) {
    int i;

    if (b->len == 0) {
        printf("(empty)\n");
        return;
    }
    for (i = 0; i < b->len; i++)
        printf("%d\t%s\n", i + 1, b->line[i]);
}

static void print_one(struct buffer *b, int n) {
    if (n < 1 || n > b->len) {
        printf("No such line\n");
        return;
    }
    printf("%d\t%s\n", n, b->line[n - 1]);
}

int main(int argc, char **argv) {
    struct buffer b = {0};
    char cmd[COMMAND_CAP];
    const char *path;
    int current = 0;
    int dirty = 0;

    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        printf("This MyOS editor is line based. Run h inside for help.\n");
        return 1;
    }
    path = argv[1];

    if (read_file(&b, path) < 0) {
        printf("Could not load file\n");
        return 1;
    }
    if (b.len > 0)
        current = b.len - 1;

    printf("%s: %d lines\n", path, b.len);
    printf("Type h for help. Use w to save, q to quit.\n");

    for (;;) {
        const char *p;
        int nread;

        printf(":%d> ", current + 1);
        nread = read_command(cmd, sizeof(cmd));
        if (nread < 0)
            break;
        p = cmd + 1;

        if (cmd[0] == '\0')
            continue;
        if (strcmp(cmd, "h") == 0) {
            print_help();
        } else if (strcmp(cmd, "p") == 0) {
            print_lines(&b);
        } else if (cmd[0] == 'p' && cmd[1] == ' ') {
            int line = parse_number(&p);
            print_one(&b, line);
            if (line >= 1 && line <= b.len)
                current = line - 1;
        } else if (strcmp(cmd, "a") == 0) {
            int at = b.len == 0 ? 0 : current + 1;
            int added = read_text_block(&b, at, &current);
            if (added > 0)
                dirty = 1;
        } else if (cmd[0] == 'a' && cmd[1] == ' ') {
            int at = b.len == 0 ? 0 : current + 1;
            if (insert_line(&b, at, cmd + 2) < 0) {
                printf("Out of memory\n");
                continue;
            }
            current = at;
            dirty = 1;
        } else if (cmd[0] == 'i' && cmd[1] == ' ') {
            int line = parse_number(&p);
            if (line < 1) line = 1;
            if (line > b.len + 1) line = b.len + 1;
            if (has_text(p)) {
                if (insert_line(&b, line - 1, p) < 0) {
                    printf("Out of memory\n");
                    continue;
                }
                current = line - 1;
                dirty = 1;
            } else {
                int added = read_text_block(&b, line - 1, &current);
                if (added > 0)
                    dirty = 1;
            }
        } else if (cmd[0] == 'r' && cmd[1] == ' ') {
            int line = parse_number(&p);
            char *next;
            if (line < 1 || line > b.len) {
                printf("No such line\n");
                continue;
            }
            next = dup_string(p);
            if (!next) {
                printf("Out of memory\n");
                continue;
            }
            free(b.line[line - 1]);
            b.line[line - 1] = next;
            current = line - 1;
            dirty = 1;
        } else if (cmd[0] == 'd' && cmd[1] == ' ') {
            int line = parse_number(&p);
            if (line < 1 || line > b.len) {
                printf("No such line\n");
                continue;
            }
            delete_line(&b, line - 1);
            if (current >= b.len)
                current = b.len - 1;
            if (current < 0)
                current = 0;
            dirty = 1;
        } else if (strcmp(cmd, "w") == 0) {
            if (write_file(&b, path) < 0)
                printf("Could not save file\n");
            else {
                dirty = 0;
                printf("%d lines written\n", b.len);
            }
        } else if (strcmp(cmd, "q") == 0) {
            if (dirty) {
                printf("Unsaved changes. Use w to save or q! to discard.\n");
                continue;
            }
            break;
        } else if (strcmp(cmd, "q!") == 0) {
            break;
        } else {
            printf("Unknown command. Type h for help.\n");
        }
    }

    free_buffer(&b);
    return 0;
}
