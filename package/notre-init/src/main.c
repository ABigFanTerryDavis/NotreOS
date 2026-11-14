#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/reboot.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NOTRE_INIT_VERSION "0.0.1"
#define CMDLINE_CAPACITY 4096

static void log_message(const char *message)
{
    dprintf(STDERR_FILENO, "NotreInit: %s\n", message);
}

static bool has_token(const char *line, const char *wanted)
{
    size_t wanted_len = strlen(wanted);
    const char *p = line;
    while (*p != '\0') {
        const char *start;
        size_t len;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            ++p;
        start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n')
            ++p;
        len = (size_t)(p - start);
        if (len == wanted_len && strncmp(start, wanted, len) == 0)
            return true;
    }
    return false;
}

static bool get_cmdline_value(const char *line, const char *key,
                              char *value, size_t value_capacity)
{
    size_t key_len = strlen(key);
    const char *p = line;
    while (*p != '\0') {
        const char *start;
        size_t len;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            ++p;
        start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n')
            ++p;
        len = (size_t)(p - start);
        if (len > key_len && strncmp(start, key, key_len) == 0) {
            size_t value_len = len - key_len;
            if (value_len >= value_capacity)
                value_len = value_capacity - 1;
            memcpy(value, start + key_len, value_len);
            value[value_len] = '\0';
            return true;
        }
    }
    return false;
}

static void read_cmdline(char *buffer, size_t capacity)
{
    FILE *file = fopen("/proc/cmdline", "r");
    if (file == NULL) {
        buffer[0] = '\0';
        return;
    }
    if (fgets(buffer, (int)capacity, file) == NULL)
        buffer[0] = '\0';
    fclose(file);
}

static void make_dir(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        dprintf(STDERR_FILENO, "NotreInit: mkdir %s: %s\n", path, strerror(errno));
}

static void mount_if_needed(const char *source, const char *target,
                            const char *filesystem, unsigned long flags,
                            const char *data)
{
    if (mount(source, target, filesystem, flags, data) != 0 && errno != EBUSY)
        dprintf(STDERR_FILENO, "NotreInit: mount %s on %s: %s\n",
                filesystem, target, strerror(errno));
}

static int run_and_wait(const char *path, char *const argv[])
{
    pid_t child = fork();
    int status;
    if (child < 0)
        return -1;
    if (child == 0) {
        execv(path, argv);
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static pid_t start_background(const char *path, char *const argv[])
{
    pid_t child = fork();
    if (child < 0) {
        dprintf(STDERR_FILENO, "NotreInit: fork for %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (child == 0) {
        setsid();
        execv(path, argv);
        dprintf(STDERR_FILENO, "NotreInit: cannot start %s: %s\n", path, strerror(errno));
        _exit(127);
    }
    return child;
}

static void start_udev(void)
{
    char *udevd_argv[] = { (char *)"udevd", (char *)"--daemon", NULL };
    char *trigger_argv[] = { (char *)"udevadm", (char *)"trigger",
                             (char *)"--action=add", NULL };
    char *settle_argv[] = { (char *)"udevadm", (char *)"settle",
                            (char *)"--timeout=10", NULL };
    if (access("/sbin/udevd", X_OK) != 0)
        return;
    (void)run_and_wait("/sbin/udevd", udevd_argv);
    (void)run_and_wait("/sbin/udevadm", trigger_argv);
    (void)run_and_wait("/sbin/udevadm", settle_argv);
}

static bool resolve_root_device(const char *root, char *resolved, size_t capacity)
{
    if (strncmp(root, "LABEL=", 6) == 0) {
        char label_path[PATH_MAX];
        const char *label = root + 6;
        char *actual;
        if (snprintf(label_path, sizeof(label_path), "/dev/disk/by-label/%s", label)
            >= (int)sizeof(label_path))
            return false;
        actual = realpath(label_path, NULL);
        if (actual == NULL)
            return false;
        if (strlen(actual) >= capacity) {
            free(actual);
            return false;
        }
        strcpy(resolved, actual);
        free(actual);
        return true;
    }
    if (strlen(root) >= capacity)
        return false;
    strcpy(resolved, root);
    return true;
}

static void emergency_reboot(int signal_number)
{
    (void)signal_number;
    sync();
    reboot(RB_AUTOBOOT);
}

static bool pivot_to_installed_root(const char *root_arg)
{
    char device[PATH_MAX];
    struct timespec pause = { .tv_sec = 0, .tv_nsec = 500000000L };
    unsigned int attempt;

    start_udev();
    if (!resolve_root_device(root_arg, device, sizeof(device))) {
        log_message("the requested root device is not available; staying in live mode");
        return false;
    }
    make_dir("/newroot");
    for (attempt = 0; attempt < 120; ++attempt) {
        if (mount(device, "/newroot", "ext4", MS_RELATIME, NULL) == 0)
            break;
        nanosleep(&pause, NULL);
    }
    if (attempt == 120) {
        dprintf(STDERR_FILENO, "NotreInit: cannot mount %s as ext4; staying in live mode\n",
                device);
        return false;
    }

    make_dir("/newroot/oldroot");
    make_dir("/newroot/dev");
    make_dir("/newroot/proc");
    make_dir("/newroot/sys");
    make_dir("/newroot/run");
    (void)mount(NULL, "/dev", "/newroot/dev", MS_MOVE, NULL);
    (void)mount(NULL, "/proc", "/newroot/proc", MS_MOVE, NULL);
    (void)mount(NULL, "/sys", "/newroot/sys", MS_MOVE, NULL);
    (void)mount(NULL, "/run", "/newroot/run", MS_MOVE, NULL);

    if (chdir("/newroot") != 0 || syscall(SYS_pivot_root, ".", "oldroot") != 0) {
        dprintf(STDERR_FILENO, "NotreInit: pivot_root failed: %s\n", strerror(errno));
        return false;
    }
    if (chdir("/") != 0) {
        dprintf(STDERR_FILENO, "NotreInit: cannot enter the installed root: %s\n",
                strerror(errno));
        return false;
    }
    (void)umount2("/oldroot", MNT_DETACH);
    (void)rmdir("/oldroot");
    log_message("entered the installed NotreOS root filesystem");
    {
        char *argv[] = { (char *)"notre-init", (char *)"--installed", NULL };
        execv("/sbin/notre-init", argv);
    }
    log_message("cannot start NotreInit from the installed root");
    return false;
}

static pid_t spawn_on_tty(const char *path, char *const argv[], const char *tty)
{
    pid_t child = fork();
    if (child < 0) {
        dprintf(STDERR_FILENO, "NotreInit: fork for %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (child == 0) {
        int fd;
        (void)setsid();
        fd = open(tty, O_RDWR);
        if (fd < 0) {
            dprintf(STDERR_FILENO, "NotreInit: open %s: %s\n", tty, strerror(errno));
            _exit(126);
        }
        (void)ioctl(fd, TIOCSCTTY, 0);
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
        if (chdir("/root") != 0)
            dprintf(STDERR_FILENO, "NotreInit: cannot enter /root: %s\n", strerror(errno));
        setenv("HOME", "/root", 1);
        setenv("USER", "root", 1);
        setenv("SHELL", "/usr/bin/notre-shell", 1);
        setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
        execv(path, argv);
        dprintf(STDERR_FILENO, "NotreInit: cannot start %s: %s\n", path, strerror(errno));
        _exit(127);
    }
    return child;
}

static pid_t start_shell(const char *tty)
{
    char *argv[] = { (char *)"notre-shell", NULL };
    return spawn_on_tty("/usr/bin/notre-shell", argv, tty);
}

static pid_t start_x_server(void)
{
    char *argv[] = { (char *)"Xorg", (char *)":0", (char *)"-keeptty",
                     (char *)"vt1", (char *)"-nolisten", (char *)"tcp",
                     (char *)"-noreset", NULL };
    setenv("DISPLAY", ":0", 1);
    setenv("HOME", "/root", 1);
    setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
    return spawn_on_tty("/usr/bin/Xorg", argv, "/dev/tty1");
}

static pid_t start_dillo(void)
{
    char *argv[] = { (char *)"dillo",
                     (char *)"file:///usr/share/notreos/welcome.html", NULL };
    int null_fd;
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        (void)setsid();
        null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDIN_FILENO);
            (void)dup2(null_fd, STDOUT_FILENO);
            (void)dup2(null_fd, STDERR_FILENO);
            if (null_fd > STDERR_FILENO)
                close(null_fd);
        }
        if (chdir("/root") != 0)
            _exit(126);
        setenv("HOME", "/root", 1);
        setenv("DISPLAY", ":0", 1);
        setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
        execv("/usr/bin/dillo", argv);
        _exit(127);
    }
    return child;
}

static void start_network(void)
{
    char *argv[] = { (char *)"dhcpcd", (char *)"-B", (char *)"-q", NULL };
    if (access("/sbin/dhcpcd", X_OK) == 0)
        (void)start_background("/sbin/dhcpcd", argv);
}

static void start_system(bool no_gui, bool serial_console)
{
    pid_t shell_tty2;
    pid_t shell_serial = -1;
    pid_t x_server = -1;
    pid_t browser = -1;
    struct timespec gui_delay = { .tv_sec = 3, .tv_nsec = 0 };
    int status;
    pid_t dead;

    shell_tty2 = start_shell("/dev/tty2");
    if (serial_console && access("/dev/ttyS0", F_OK) == 0)
        shell_serial = start_shell("/dev/ttyS0");
    if (!no_gui) {
        x_server = start_x_server();
        nanosleep(&gui_delay, NULL);
        browser = start_dillo();
    }

    for (;;) {
        dead = waitpid(-1, &status, 0);
        if (dead < 0) {
            if (errno == EINTR)
                continue;
            if (errno == ECHILD) {
                pause();
                continue;
            }
            continue;
        }
        if (dead == shell_tty2)
            shell_tty2 = start_shell("/dev/tty2");
        else if (dead == shell_serial)
            shell_serial = start_shell("/dev/ttyS0");
        else if (dead == x_server && !no_gui) {
            x_server = start_x_server();
            nanosleep(&gui_delay, NULL);
            browser = start_dillo();
        } else if (dead == browser && !no_gui) {
            nanosleep(&gui_delay, NULL);
            browser = start_dillo();
        }
    }
}

int main(int argc, char **argv)
{
    char cmdline[CMDLINE_CAPACITY];
    char root_arg[PATH_MAX];
    bool installed_restart = argc > 1 && strcmp(argv[1], "--installed") == 0;
    bool no_gui;
    bool serial_console;
    char *udevd_argv[] = { (char *)"udevd", (char *)"--daemon", NULL };
    char *trigger_argv[] = { (char *)"udevadm", (char *)"trigger",
                             (char *)"--action=add", NULL };
    char *settle_argv[] = { (char *)"udevadm", (char *)"settle",
                            (char *)"--timeout=10", NULL };

    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        puts("NotreInit " NOTRE_INIT_VERSION);
        return 0;
    }
    if (getpid() != 1 && !installed_restart) {
        fputs("NotreInit: refusing to start outside PID 1\n", stderr);
        return 1;
    }

    signal(SIGINT, emergency_reboot);
    signal(SIGTERM, emergency_reboot);
    signal(SIGCHLD, SIG_DFL);
    (void)umask(022);
    setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
    setenv("HOME", "/root", 1);

    make_dir("/proc");
    make_dir("/sys");
    make_dir("/dev");
    make_dir("/run");
    make_dir("/tmp");
    make_dir("/newroot");
    mount_if_needed("proc", "/proc", "proc", 0, NULL);
    mount_if_needed("sysfs", "/sys", "sysfs", 0, NULL);
    mount_if_needed("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    make_dir("/dev/pts");
    make_dir("/dev/shm");
    mount_if_needed("devpts", "/dev/pts", "devpts", 0, "mode=0620,ptmxmode=000");
    mount_if_needed("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
    mount_if_needed("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
    read_cmdline(cmdline, sizeof(cmdline));
    no_gui = has_token(cmdline, "notreos.nogui=1");
    serial_console = strstr(cmdline, "ttyS0") != NULL;

    if (!installed_restart && get_cmdline_value(cmdline, "root=", root_arg,
                                                 sizeof(root_arg))) {
        log_message("trying the root device requested by the kernel command line");
        (void)pivot_to_installed_root(root_arg);
    }

    if (access("/sbin/udevd", X_OK) == 0) {
        log_message("starting device discovery");
        (void)run_and_wait("/sbin/udevd", udevd_argv);
        (void)run_and_wait("/sbin/udevadm", trigger_argv);
        (void)run_and_wait("/sbin/udevadm", settle_argv);
    }
    start_network();

    log_message("NotreOS Ancient 0.0.1 is ready");
    log_message("NotreShell is on VT2; P4DE (Platform 4 Desktop Environment) uses VT1");
    start_system(no_gui, serial_console);
    return 0;
}
