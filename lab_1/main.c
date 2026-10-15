#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int write_all(int fd, const void *data, size_t size) {
    const char *p = (const char *)data;
    while (size > 0) {
        ssize_t n = write(fd, p, size);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += n;
        size -= (size_t)n;
    }
    return 0;
}

static void fail(const char *message) {
    write_all(STDERR_FILENO, message, strlen(message));
    _exit(1);
}

int main(void) {
    int p12[2];
    int p01[2];
    int p2p[2];

    if (pipe(p01) == -1 || pipe(p12) == -1 || pipe(p2p) == -1)
        fail("Ошибка: не удалось создать канал pipe().\n");

    pid_t c1 = fork();
    if (c1 == -1) fail("Ошибка: fork() для child1 завершился неудачно.\n");
    if (c1 == 0) {
        if (dup2(p01[0], STDIN_FILENO) == -1 ||
            dup2(p12[1], STDOUT_FILENO) == -1)
            fail("Ошибка: dup2() в child1.\n");
        close(p01[0]); close(p01[1]);
        close(p12[0]); close(p12[1]);
        close(p2p[0]); close(p2p[1]);
        execl("./child1", "child1", (char *)NULL);
        fail("Ошибка: не удалось запустить ./child1.\n");
    }

    pid_t c2 = fork();
    if (c2 == -1) {
        close(p01[1]); close(p12[0]); close(p2p[0]);
        kill(c1, SIGTERM);
        waitpid(c1, NULL, 0);
        fail("Ошибка: fork() для child2 завершился неудачно.\n");
    }
    if (c2 == 0) {
        if (dup2(p12[0], STDIN_FILENO) == -1 ||
            dup2(p2p[1], STDOUT_FILENO) == -1)
            fail("Ошибка: dup2() в child2.\n");
        close(p01[0]); close(p01[1]);
        close(p12[0]); close(p12[1]);
        close(p2p[0]); close(p2p[1]);
        execl("./child2", "child2", (char *)NULL);
        fail("Ошибка: не удалось запустить ./child2.\n");
    }

    close(p01[0]);
    close(p12[0]); close(p12[1]);
    close(p2p[1]);

    char buf[4096];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            close(p01[1]); close(p2p[0]);
            waitpid(c1, NULL, 0); waitpid(c2, NULL, 0);
            fail("Ошибка: чтение стандартного ввода.\n");
        }
        if (write_all(p01[1], buf, (size_t)n) == -1) {
            close(p01[1]); close(p2p[0]);
            waitpid(c1, NULL, 0); waitpid(c2, NULL, 0);
            fail("Ошибка: запись в канал к child1.\n");
        }
    }
    close(p01[1]);

    for (;;) {
        ssize_t n = read(p2p[0], buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            close(p2p[0]);
            waitpid(c1, NULL, 0); waitpid(c2, NULL, 0);
            fail("Ошибка: чтение результата от child2.\n");
        }
        if (write_all(STDOUT_FILENO, buf, (size_t)n) == -1) {
            close(p2p[0]);
            waitpid(c1, NULL, 0); waitpid(c2, NULL, 0);
            fail("Ошибка: вывод результата.\n");
        }
    }
    close(p2p[0]);

    int s1, s2;
    if (waitpid(c1, &s1, 0) == -1 || waitpid(c2, &s2, 0) == -1)
        fail("Ошибка: waitpid().\n");
    if (!WIFEXITED(s1) || WEXITSTATUS(s1) != 0 ||
        !WIFEXITED(s2) || WEXITSTATUS(s2) != 0)
        fail("Ошибка: один из дочерних процессов завершился с ошибкой.\n");
    return 0;
}
