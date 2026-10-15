#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

int main(void) {
    unsigned char buf[4096];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "child1: ошибка чтения.\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return 1;
        }
        for (ssize_t i = 0; i < n; ++i)
            if (buf[i] >= 'a' && buf[i] <= 'z')
                buf[i] = (unsigned char)(buf[i] - 'a' + 'A');

        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(STDOUT_FILENO, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                const char msg[] = "child1: ошибка записи.\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                return 1;
            }
            if (w == 0) return 1;
            off += w;
        }
    }
}
