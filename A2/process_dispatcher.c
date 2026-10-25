#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_PREFIX "/dispatcher_shm_"
#define OUTPUT_FILE "output.txt"
#define MAX_JOBS 256
#define MAX_CMDS 2
#define MAX_ARGS 64

typedef struct {
    char *text;                     
    char *buf;                          
    int ncmds;                         
    char *argv[MAX_CMDS][MAX_ARGS + 1]; 
} Job;

static int writeOutput(const char *command, const char *output)
{
    FILE *fp = fopen("output.txt", "a");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }
    fprintf(fp, "The output of: %s : is\n", command);
    fprintf(fp, ">>>>>>>>>>>>>>>>\n%s<<<<<<<<<<<<<<<<\n", output);  
    fclose(fp);
    return 0;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') {
        s++;
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
    return s;
}

static int tokenize(char *s, char **argv, int max)
{
    int n = 0;
    char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n >= max) {
            return -1;
        }
        argv[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    argv[n] = NULL;
    return n;
}

static void free_job(Job *job)
{
    free(job->text);
    free(job->buf);
    job->text = NULL;
    job->buf = NULL;
}

static int parse_job(const char *line, Job *job)
{
    job->ncmds = 0;
    job->text = strdup(line);
    job->buf = strdup(line);
    if (job->text == NULL || job->buf == NULL) {
        free_job(job);
        return -1;
    }

    char *seg = job->buf;
    while (1) {
        char *bar = strchr(seg, '|');
        if (bar != NULL) {
            *bar = '\0';
        }
        if (job->ncmds >= MAX_CMDS) {
            free_job(job);
            return -1;
        }
        if (tokenize(seg, job->argv[job->ncmds], MAX_ARGS) <= 0) {
            free_job(job);
            return -1;
        }
        job->ncmds++;
        if (bar == NULL) {
            break;
        }
        seg = bar + 1;
    }
    return 0;
}

static char *read_all(int fd)
{
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (buf == NULL) {
        return NULL;
    }
    for (;;) {
        if (len + 1024 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (nb == NULL) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, cap - len - 1);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (r == 0) {
            break;
        }
        len += (size_t)r;
    }
    buf[len] = '\0';
    return buf;
}

static void write_all(int fd, const char *s, size_t n)
{
    while (n > 0) {
        ssize_t w = write(fd, s, n);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        s += w;
        n -= (size_t)w;
    }
}

static int wait_for(pid_t pid)
{
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
}

static void reader_process(const char *filename, const char *shm_name)
{
    int in = open(filename, O_RDONLY);
    if (in < 0) {
        perror("open input file");
        _exit(EXIT_FAILURE);
    }

    struct stat st;
    if (fstat(in, &st) < 0) {
        perror("fstat");
        _exit(EXIT_FAILURE);
    }

    int shm = shm_open(shm_name, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (shm < 0) {
        perror("shm_open");
        _exit(EXIT_FAILURE);
    }
    if (ftruncate(shm, st.st_size) < 0) {
        perror("ftruncate");
        _exit(EXIT_FAILURE);
    }

    if (st.st_size > 0) {
        size_t size = (size_t)st.st_size;
        char *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
        if (map == MAP_FAILED) {
            perror("mmap");
            _exit(EXIT_FAILURE);
        }
        size_t total = 0;
        while (total < size) {
            ssize_t r = read(in, map + total, size - total);
            if (r < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("read");
                _exit(EXIT_FAILURE);
            }
            if (r == 0) {
                break;
            }
            total += (size_t)r;
        }
        munmap(map, size);
    }

    close(shm);
    close(in);
    _exit(EXIT_SUCCESS);
}

static void manager_process(const Job *job, int result_fd)
{
    char *output = NULL;
    int codes[MAX_CMDS] = {0, 0};

    if (job->ncmds == 1) {
        int opipe[2];
        if (pipe(opipe) < 0) {
            perror("pipe");
            _exit(EXIT_FAILURE);
        }
        pid_t c = fork();
        if (c < 0) {
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        if (c == 0) {
            close(opipe[0]);
            dup2(opipe[1], STDOUT_FILENO);
            close(opipe[1]);
            execvp(job->argv[0][0], job->argv[0]);
            perror("execvp");
            _exit(127);
        }
        close(opipe[1]);
        output = read_all(opipe[0]);
        close(opipe[0]);
        codes[0] = wait_for(c);
    } else {
        int cmd_pipe[2];
        int out_pipe[2];
        if (pipe(cmd_pipe) < 0 || pipe(out_pipe) < 0) {
            perror("pipe");
            _exit(EXIT_FAILURE);
        }

        pid_t c1 = fork();
        if (c1 < 0) {
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        if (c1 == 0) {
            close(cmd_pipe[0]);
            close(out_pipe[0]);
            close(out_pipe[1]);
            dup2(cmd_pipe[1], STDOUT_FILENO);
            close(cmd_pipe[1]);
            execvp(job->argv[0][0], job->argv[0]);
            perror("execvp");
            _exit(127);
        }

        pid_t c2 = fork();
        if (c2 < 0) {
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        if (c2 == 0) {
            close(cmd_pipe[1]);
            close(out_pipe[0]);
            dup2(cmd_pipe[0], STDIN_FILENO);
            close(cmd_pipe[0]);
            dup2(out_pipe[1], STDOUT_FILENO);
            close(out_pipe[1]);
            execvp(job->argv[1][0], job->argv[1]);
            perror("execvp");
            _exit(127);
        }

        close(cmd_pipe[0]);
        close(cmd_pipe[1]);
        close(out_pipe[1]);
        output = read_all(out_pipe[0]);
        close(out_pipe[0]);
        codes[0] = wait_for(c1);
        codes[1] = wait_for(c2);
    }

    const char *text = (output != NULL) ? output : "";
    write_all(result_fd, text, strlen(text));
    dprintf(result_fd, "[Process status]\n");
    for (int i = 0; i < job->ncmds; i++) {
        dprintf(result_fd, "Stage %d (%s): exit %d\n", i + 1, job->argv[i][0], codes[i]);
    }

    free(output);
    close(result_fd);
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    remove(OUTPUT_FILE);

    char shm_name[64];
    snprintf(shm_name, sizeof shm_name, "%s%d", SHM_PREFIX, (int)getpid());

    pid_t reader = fork();
    if (reader < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (reader == 0) {
        reader_process(argv[1], shm_name);
    }
    if (wait_for(reader) != 0) {
        fprintf(stderr, "Reader process failed\n");
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }

    int fd = shm_open(shm_name, O_RDONLY, 0);
    if (fd < 0) {
        perror("shm_open");
        return EXIT_FAILURE;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }
    size_t size = (size_t)st.st_size;
    char *text = malloc(size + 1);
    if (text == NULL) {
        perror("malloc");
        close(fd);
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }
    if (size > 0) {
        void *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            perror("mmap");
            free(text);
            close(fd);
            shm_unlink(shm_name);
            return EXIT_FAILURE;
        }
        memcpy(text, map, size);
        munmap(map, size);
    }
    text[size] = '\0';
    close(fd);
    shm_unlink(shm_name);

    Job *jobs = calloc(MAX_JOBS, sizeof *jobs);
    if (jobs == NULL) {
        perror("calloc");
        free(text);
        return EXIT_FAILURE;
    }
    int njobs = 0;
    char *cursor = text;
    while (*cursor != '\0') {
        char *eol = strchr(cursor, '\n');
        size_t len = (eol != NULL) ? (size_t)(eol - cursor) : strlen(cursor);
        char *raw = malloc(len + 1);
        if (raw == NULL) {
            perror("malloc");
            break;
        }
        memcpy(raw, cursor, len);
        raw[len] = '\0';
        cursor += len;
        if (*cursor == '\n') {
            cursor++;
        }

        char *line = trim(raw);
        if (*line != '\0' && njobs < MAX_JOBS) {
            if (parse_job(line, &jobs[njobs]) == 0) {
                njobs++;
            } else {
                fprintf(stderr, "Skipping invalid job: %s\n", line);
            }
        }
        free(raw);
    }
    free(text);

    for (int j = 0; j < njobs; j++) {
        int pfd[2];
        if (pipe(pfd) < 0) {
            perror("pipe");
            break;
        }

        pid_t mgr = fork();
        if (mgr < 0) {
            perror("fork");
            close(pfd[0]);
            close(pfd[1]);
            fprintf(stderr, "Could not start manager for job %d; stopping\n", j + 1);
            break;
        }
        if (mgr == 0) {
            close(pfd[0]);
            manager_process(&jobs[j], pfd[1]);
        }

        close(pfd[1]);
        char *result = read_all(pfd[0]);
        close(pfd[0]);
        wait_for(mgr);

        if (writeOutput(jobs[j].text, result != NULL ? result : "") < 0) {
            fprintf(stderr, "Could not write job %d to %s\n", j + 1, OUTPUT_FILE);
        }

        free(result);
        free_job(&jobs[j]);
    }

    free(jobs);
    return EXIT_SUCCESS;
}