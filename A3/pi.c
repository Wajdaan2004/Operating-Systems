#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define MIN_DARTS   5000000LL
#define MIN_THREADS 2

typedef struct {
    long long darts;
    unsigned int seed;
} worker_arg_t;

static long long inside_count = 0;                  /* shared by all threads */
static pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;

static double random_double(unsigned int *seed)
{
    /* returns a value in [0, 1) using a per-thread seed */
    return (double)rand_r(seed) / ((double)RAND_MAX + 1.0);
}

static void *throw_darts(void *arg)
{
    worker_arg_t *w = (worker_arg_t *)arg;
    long long local_inside = 0;

    for (long long i = 0; i < w->darts; i++) {
        double x = random_double(&w->seed) * 2.0 - 1.0;   /* [-1, 1) */
        double y = random_double(&w->seed) * 2.0 - 1.0;
        if (x * x + y * y <= 1.0)
            local_inside++;
    }

    pthread_mutex_lock(&count_lock);
    inside_count += local_inside;
    pthread_mutex_unlock(&count_lock);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: pi <integer value for NUMBER_OF_DARTS> <integer value for NUMBER_OF_THREADS>\n");
        return 1;
    }

    long long darts = atoll(argv[1]);
    int num_threads = atoi(argv[2]);

    if (darts < MIN_DARTS) {
        printf("The number of darts must be >= 5000000\n");
        return 1;
    }
    if (num_threads < MIN_THREADS) {
        printf("The number of threads must be >= 2\n");
        return 1;
    }

    pthread_t *tids = malloc(num_threads * sizeof *tids);
    worker_arg_t *args = malloc(num_threads * sizeof *args);
    if (!tids || !args) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    /* Split the darts as evenly as possible; the first (darts % threads) threads get one extra */
    long long base = darts / num_threads;
    long long extra = darts % num_threads;
    unsigned int seed_base = (unsigned int)time(NULL);

    for (int i = 0; i < num_threads; i++) {
        args[i].darts = base + (i < extra ? 1 : 0);
        args[i].seed = seed_base + (unsigned int)i * 7919u;   /* different seed per thread */
        if (pthread_create(&tids[i], NULL, throw_darts, &args[i]) != 0) {
            fprintf(stderr, "failed to create thread %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(tids[i], NULL);

    double pi = 4.0 * (double)inside_count / (double)darts;
    printf("Pi = %f\n", pi);

    free(tids);
    free(args);
    pthread_mutex_destroy(&count_lock);
    return 0;
}