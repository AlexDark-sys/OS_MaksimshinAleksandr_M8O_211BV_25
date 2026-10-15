#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <errno.h>

int main(void) {
    unsigned char in[4096], out[4096];
    int previous_was_space = 0;

    for (;;) {
        ssize_t n = read(STDIN_FILENO, in, sizeof(in));
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "child2: ошибка чтения.\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return 1;
        }

        size_t used = 0;
        for (ssize_t i = 0; i < n; ++i) {
            if (in[i] == ' ') {
                if (!previous_was_space) out[used++] = in[i];
                previous_was_space = 1;
            } else {
                out[used++] = in[i];
                previous_was_space = 0;
            }
        }

        size_t off = 0;
        while (off < used) {
            ssize_t w = write(STDOUT_FILENO, out + off, used - off);
            if (w < 0) {
                if (errno == EINTR) continue;
                const char msg[] = "child2: ошибка записи.\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                return 1;
            }
            if (w == 0) return 1;
            off += (size_t)w;
        }
    }
}
