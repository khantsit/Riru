#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <syscall.h>
#include <cstdio>
#include <sys/xattr.h>
#include <plt.h>

static int setsockcreatecon_builtin_impl(const char *ctx) {
    int fd = open("/proc/thread-self/attr/sockcreate", O_RDWR | O_CLOEXEC);
    if (fd == -1 && errno == ENOENT) {
        char path[PATH_MAX];
        pid_t tid;

        // bionic gettid sometimes does not return correct tid, https://twitter.com/HaruueIcymoon/status/1059365098265882624
        tid = syscall(__NR_gettid);
        snprintf(path, PATH_MAX, "/proc/self/task/%d/attr/sockcreate", tid);
        fd = open(path, O_RDWR | O_CLOEXEC);
    }
    if (fd < 0) return -1;
    ssize_t rc;
    if (ctx) {
        do {
            rc = write(fd, ctx, strlen(ctx) + 1);
        } while (rc < 0 && errno == EINTR);
    } else {
        do {
            rc = write(fd, nullptr, 0);
        } while (rc < 0 && errno == EINTR);
    }
    close(fd);
    return rc == -1 ? -1 : 0;
}

static int setfilecon_builtin_impl(const char *path, const char *ctx) {
    return syscall(__NR_setxattr, path, XATTR_NAME_SELINUX, ctx, strlen(ctx) + 1, 0);
}

static int stub(const char *) {
    return 0;
}

static int stub(const char *, const char *) {
    return 0;
}

static int stub(const char *scon, const char *tcon, const char *tclass, const char *perm, void *auditdata) {
    return 0;
}

int (*setsockcreatecon)(const char *con) = stub;

int (*setfilecon)(const char *, const char *) = stub;

int (*selinux_check_access)(const char *, const char *, const char *, const char *, void *) = stub;

void selinux_builtin_impl() {
    setsockcreatecon = setsockcreatecon_builtin_impl;
    setfilecon = setfilecon_builtin_impl;
}

void dload_selinux() {
    if (access("/system/lib/libselinux.so", F_OK) != 0 && access("/system/lib64/libselinux.so", F_OK) != 0) {
        return;
    }

    selinux_builtin_impl();
    auto _selinux_check_access = (plt_dlsym("selinux_check_access", nullptr));
    if (_selinux_check_access)
        selinux_check_access = (int (*)(const char *, const char *, const char *, const char *, void *)) _selinux_check_access;
}