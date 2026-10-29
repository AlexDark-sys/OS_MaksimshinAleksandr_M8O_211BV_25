#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    double *src;
    double *dst;
    int rows;
    int cols;
    int start;
    int end;
    int dilation;
} Task;

static void filter_part(Task *t) {
    for (int i = t->start; i < t->end; ++i) {
        for (int j = 0; j < t->cols; ++j) {
            double v = t->dilation ? -1.0e300 : 1.0e300;
            for (int di = -1; di <= 1; ++di) {
                for (int dj = -1; dj <= 1; ++dj) {
                    int y = i + di, x = j + dj;
                    if (y < 0 || y >= t->rows || x < 0 || x >= t->cols)
                        continue;
                    double z = t->src[y * t->cols + x];
                    if (t->dilation ? z > v : z < v)
                        v = z;
                }
            }
            t->dst[i * t->cols + j] = v;
        }
    }
}

static void *worker(void *p) {
    filter_part((Task *)p);
    return NULL;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void run_filter(double *a, double *b, int rows, int cols, int k,
                       int dilation, int threads) {
    if (threads < 1) threads = 1;
    if (threads > rows) threads = rows;
    pthread_t *ids = malloc((size_t)threads * sizeof(*ids));
    Task *tasks = malloc((size_t)threads * sizeof(*tasks));
    if (!ids || !tasks) {
        fprintf(stderr, "Не удалось выделить память\n");
        free(ids);
        free(tasks);
        exit(1);
    }

    double *src = a, *dst = b;
    for (int iter = 0; iter < k; ++iter) {
        for (int t = 0; t < threads; ++t) {
            tasks[t].src = src;
            tasks[t].dst = dst;
            tasks[t].rows = rows;
            tasks[t].cols = cols;
            tasks[t].start = t * rows / threads;
            tasks[t].end = (t + 1) * rows / threads;
            tasks[t].dilation = dilation;
        }
        if (threads == 1) {
            filter_part(&tasks[0]);
        } else {
            for (int t = 0; t < threads; ++t) {
                if (pthread_create(&ids[t], NULL, worker, &tasks[t]) != 0) {
                    fprintf(stderr, "Ошибка создания потока\n");
                    exit(1);
                }
            }
            for (int t = 0; t < threads; ++t)
                pthread_join(ids[t], NULL);
        }
        double *tmp = src;
        src = dst;
        dst = tmp;
    }
    if (src != a) {
        for (size_t i = 0; i < (size_t)rows * cols; ++i)
            a[i] = src[i];
    }
    free(ids);
    free(tasks);
}

static int read_int(const char *s, int *v) {
    char *end;
    long x = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || x < 1 || x > 1000000) return 0;
    *v = (int)x;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Использование: %s rows cols K max_threads\n", argv[0]);
        return 1;
    }
    int rows, cols, k, max_threads;
    if (!read_int(argv[1], &rows) || !read_int(argv[2], &cols) ||
        !read_int(argv[3], &k) || !read_int(argv[4], &max_threads)) {
        fprintf(stderr, "Все параметры должны быть положительными целыми числами\n");
        return 1;
    }
    if ((size_t)rows > (size_t)-1 / (size_t)cols / sizeof(double)) {
        fprintf(stderr, "Размер матрицы слишком велик\n");
        return 1;
    }

    size_t n = (size_t)rows * cols;
    double *original = malloc(n * sizeof(double));
    double *a = malloc(n * sizeof(double));
    double *b = malloc(n * sizeof(double));
    if (!original || !a || !b) {
        fprintf(stderr, "Не удалось выделить память\n");
        free(original); free(a); free(b);
        return 1;
    }

    unsigned int seed = 12345;
    for (size_t i = 0; i < n; ++i) {
        seed = seed * 1103515245u + 12345u;
        original[i] = (double)(seed % 10000) / 100.0;
    }

    int cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 1;
    printf("Матрица: %d x %d, K = %d, логических ядер: %d\n",
           rows, cols, k, cpus);

    for (int op = 0; op < 2; ++op) {
        for (size_t i = 0; i < n; ++i) a[i] = original[i];
        double t0 = now_seconds();
        run_filter(a, b, rows, cols, k, op, 1);
        double sequential = now_seconds() - t0;
        printf("%s, последовательно: %.3f мс\n",
               op ? "Наращивание" : "Эрозия", sequential * 1000.0);

        int tests[] = {1, 2, 4, 8, 16, 128, 1024};
        int count = sizeof(tests) / sizeof(tests[0]);
        for (int q = 0; q < count; ++q) {
            int th = tests[q];
            if (th > max_threads) continue;
            if (th > rows) th = rows;
            if (th < 1) th = 1;
            for (size_t i = 0; i < n; ++i) a[i] = original[i];
            t0 = now_seconds();
            run_filter(a, b, rows, cols, k, op, th);
            double parallel = now_seconds() - t0;
            double speedup = parallel > 0 ? sequential / parallel : 0;
            double efficiency = th > 0 ? speedup / th : 0;
            printf("  потоков: %d, время: %.3f мс, ускорение: %.3f, эффективность: %.3f\n",
                   th, parallel * 1000.0, speedup, efficiency);
        }
    }

    free(original);
    free(a);
    free(b);
    return 0;
}