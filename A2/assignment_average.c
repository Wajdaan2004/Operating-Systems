#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_STUDENTS 10
#define MAX_COLS 256
#define ASSIGNMENTS_PER_CHAPTER 2

/* TA process: computes and prints the average of one assignment. */
static void ta_process(int assignment_no, const double *grades, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += grades[i];
    }
    printf("Assignment %d - Average = %f\n", assignment_no, sum / count);
    fflush(stdout);
}

/* GradTA process: creates one TA per assignment, one at a time, so output is ordered. */
static void grad_ta_process(int chapter, const double grades[][ASSIGNMENTS_PER_CHAPTER],
                            int students)
{
    for (int a = 0; a < ASSIGNMENTS_PER_CHAPTER; a++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            double column[MAX_STUDENTS];
            for (int s = 0; s < students; s++) {
                column[s] = grades[s][a];
            }
            int assignment_no = chapter * ASSIGNMENTS_PER_CHAPTER + a + 1;
            ta_process(assignment_no, column, students);
            exit(EXIT_SUCCESS);
        }
        /* Wait for this TA to finish before creating the next one. */
        wait(NULL);
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <grades_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    double matrix[MAX_STUDENTS][MAX_COLS];
    int students = 0;
    int total_cols = 0;
    char line[1024];

    while (students < MAX_STUDENTS && fgets(line, sizeof(line), fp) != NULL) {
        int cols = 0;
        char *p = line;
        char *end = NULL;
        while (cols < MAX_COLS) {
            double val = strtod(p, &end);
            if (end == p) {
                break;
            }
            matrix[students][cols++] = val;
            p = end;
        }
        if (cols == 0) {
            continue;
        }
        if (total_cols == 0) {
            total_cols = cols;
        }
        students++;
    }
    fclose(fp);

    int chapters = total_cols / ASSIGNMENTS_PER_CHAPTER;

    /* Teacher creates one GradTA per chapter, one at a time, so chapters print in order. */
    for (int ch = 0; ch < chapters; ch++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return EXIT_FAILURE;
        }
        if (pid == 0) {
            double local[MAX_STUDENTS][ASSIGNMENTS_PER_CHAPTER];
            for (int s = 0; s < students; s++) {
                for (int a = 0; a < ASSIGNMENTS_PER_CHAPTER; a++) {
                    local[s][a] = matrix[s][ch * ASSIGNMENTS_PER_CHAPTER + a];
                }
            }
            grad_ta_process(ch, local, students);
        }
        /* Wait for this chapter's GradTA to finish before starting the next. */
        wait(NULL);
    }

    return EXIT_SUCCESS;
}