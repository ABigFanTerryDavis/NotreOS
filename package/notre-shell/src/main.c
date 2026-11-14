#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOTRE_SHELL_VERSION "0.0.1"

static void free_words(char **words)
{
    size_t i;
    if (words == NULL)
        return;
    for (i = 0; words[i] != NULL; ++i)
        free(words[i]);
    free(words);
}

static char **split_words(const char *line, size_t *count)
{
    size_t line_len = strlen(line);
    size_t word_cap = 8;
    size_t argc = 0;
    char **words = calloc(word_cap, sizeof(*words));
    char *word = malloc(line_len + 1);
    size_t used = 0;
    bool started = false;
    char quote = '\0';
    bool escaped = false;
    size_t i;

    if (words == NULL || word == NULL) {
        free(words);
        free(word);
        return NULL;
    }

    for (i = 0; i <= line_len; ++i) {
        char c = line[i];
        if (escaped) {
            if (c == '\0') {
                word[used++] = '\\';
                escaped = false;
                break;
            }
            word[used++] = c;
            started = true;
            escaped = false;
            continue;
        }
        if (c == '\\' && quote != '\'') {
            escaped = true;
            started = true;
            continue;
        }
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
                continue;
            }
            if (c == '\0') {
                fprintf(stderr, "notre-shell: unmatched quote\n");
                free(word);
                free_words(words);
                return NULL;
            }
            word[used++] = c;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            started = true;
            continue;
        }
        if (c == '#' && !started)
            break;
        if (c == '\0' || isspace((unsigned char)c)) {
            if (started) {
                word[used] = '\0';
                if (argc + 1 >= word_cap) {
                    size_t next_cap = word_cap * 2;
                    char **next = realloc(words, next_cap * sizeof(*words));
                    if (next == NULL) {
                        free(word);
                        free_words(words);
                        return NULL;
                    }
                    words = next;
                    word_cap = next_cap;
                }
                words[argc] = strdup(word);
                if (words[argc] == NULL) {
                    free(word);
                    free_words(words);
                    return NULL;
                }
                ++argc;
                words[argc] = NULL;
                used = 0;
                started = false;
            }
            if (c == '\0' || (c == '#' && !started))
                break;
            continue;
        }
        word[used++] = c;
        started = true;
    }

    free(word);
    *count = argc;
    return words;
}

static void print_help(void)
{
    puts("NotreShell " NOTRE_SHELL_VERSION " — NotreOS command shell");
    puts("Built-ins: cd, clear, echo, exit, help, pwd, version");
    puts("Run other programs by name through PATH, or by an explicit path.");
    puts("0.0.1 supports quoted arguments; pipes, redirection, variables,");
    puts("globbing and shell scripts are not implemented yet.");
}

static int run_builtin(char **argv, bool *handled, bool *should_exit)
{
    *handled = true;
    *should_exit = false;

    if (strcmp(argv[0], "help") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[0], "version") == 0) {
        puts("NotreShell " NOTRE_SHELL_VERSION);
        return 0;
    }
    if (strcmp(argv[0], "echo") == 0) {
        size_t i;
        for (i = 1; argv[i] != NULL; ++i)
            printf("%s%s", i == 1 ? "" : " ", argv[i]);
        putchar('\n');
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("pwd");
            return 1;
        }
        puts(cwd);
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *target;
        if (argv[1] != NULL && argv[2] != NULL) {
            fputs("cd: expected at most one path\n", stderr);
            return 2;
        }
        target = argv[1] != NULL ? argv[1] : getenv("HOME");
        if (target == NULL)
            target = "/";
        if (chdir(target) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[0], "clear") == 0) {
        fputs("\033[2J\033[H", stdout);
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        char *end = NULL;
        long status = 0;
        if (argv[1] != NULL) {
            errno = 0;
            status = strtol(argv[1], &end, 10);
            if (errno != 0 || end == argv[1] || *end != '\0') {
                fputs("exit: status must be an integer\n", stderr);
                return 2;
            }
            if (status < 0 || status > 255)
                status = 2;
        }
        *should_exit = true;
        return (int)status;
    }

    *handled = false;
    return 0;
}

static int run_external(char **argv)
{
    pid_t child = fork();
    int status;

    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execvp(argv[0], argv);
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

int main(void)
{
    char *line = NULL;
    size_t line_cap = 0;
    bool interactive = isatty(STDIN_FILENO) != 0;
    int last_status = 0;

    if (getenv("PATH") == NULL)
        setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
    if (getenv("HOME") == NULL)
        setenv("HOME", "/root", 1);

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    while (true) {
        size_t argc = 0;
        char **argv;
        bool handled = false;
        bool should_exit = false;
        ssize_t length;

        if (interactive) {
            fputs("notre> ", stdout);
            fflush(stdout);
        }
        length = getline(&line, &line_cap, stdin);
        if (length < 0)
            break;
        argv = split_words(line, &argc);
        if (argv == NULL) {
            last_status = 2;
            continue;
        }
        if (argc == 0) {
            free_words(argv);
            continue;
        }
        last_status = run_builtin(argv, &handled, &should_exit);
        if (!handled)
            last_status = run_external(argv);
        free_words(argv);
        if (should_exit)
            break;
    }

    free(line);
    return last_status;
}
