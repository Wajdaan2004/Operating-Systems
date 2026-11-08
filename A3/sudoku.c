#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define SIZE 9
#define NUM_THREADS 27   /* 9 columns + 9 rows + 9 sub-grids */

/* structure for passing data to threads */
typedef struct
{
    int row;
    int column;
} parameters;

int grid[SIZE][SIZE];
int results[NUM_THREADS];   /* results[i] == 1 means unit i is valid */

/* Marks v as seen. Returns 0 if v is out of range 1..9 or already seen. */
static int mark(int v, int seen[])
{
    if (v < 1 || v > SIZE || seen[v])
        return 0;
    seen[v] = 1;
    return 1;
}

/* Checks column data->column. Result index: 0..8 */
static void *check_column(void *arg)
{
    parameters *data = (parameters *) arg;
    int seen[SIZE + 1] = {0};
    int valid = 1;

    for (int r = 0; r < SIZE; r++)
        if (!mark(grid[r][data->column], seen))
            valid = 0;

    results[data->column] = valid;
    return NULL;
}

/* Checks row data->row. Result index: 9..17 */
static void *check_row(void *arg)
{
    parameters *data = (parameters *) arg;
    int seen[SIZE + 1] = {0};
    int valid = 1;

    for (int c = 0; c < SIZE; c++)
        if (!mark(grid[data->row][c], seen))
            valid = 0;

    results[SIZE + data->row] = valid;
    return NULL;
}

/* Checks the 3x3 sub-grid whose top-left corner is (row, column). Result index: 18..26 */
static void *check_subgrid(void *arg)
{
    parameters *data = (parameters *) arg;
    int seen[SIZE + 1] = {0};
    int valid = 1;

    for (int r = data->row; r < data->row + 3; r++)
        for (int c = data->column; c < data->column + 3; c++)
            if (!mark(grid[r][c], seen))
                valid = 0;

    results[2 * SIZE + (data->row / 3) * 3 + data->column / 3] = valid;
    return NULL;
}

/* Reads 81 digits from the file, ignoring spaces, tabs and newlines */
static int read_grid(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open the file\n");
        return 0;
    }

    int count = 0;
    int ch;
    while (count < SIZE * SIZE && (ch = fgetc(fp)) != EOF) {
        if (ch >= '0' && ch <= '9') {
            grid[count / SIZE][count % SIZE] = ch - '0';
            count++;
        }
    }
    fclose(fp);

    if (count != SIZE * SIZE) {
        fprintf(stderr, "File does not contain a 9x9 grid (found %d digits)\n", count);
        return 0;
    }
    return 1;
}

static void print_grid(void)
{
    printf("Sudoku Puzzle Solution is:\n");
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++)
            printf("%d%s", grid[r][c], c < SIZE - 1 ? " " : "");
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sudoku <sudoku grid>\n");
        return 1;
    }

    if (!read_grid(argv[1]))
        return 1;

    print_grid();

    pthread_t tids[NUM_THREADS];
    parameters *params[NUM_THREADS];
    int created = 0;
    int failed = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        void *(*worker)(void *);

        params[i] = malloc(sizeof(parameters));
        if (params[i] == NULL) {
            perror("malloc");
            failed = 1;
            break;
        }

        if (i < SIZE) {                       /* columns */
            params[i]->row = 0;
            params[i]->column = i;
            worker = check_column;
        } else if (i < 2 * SIZE) {            /* rows */
            params[i]->row = i - SIZE;
            params[i]->column = 0;
            worker = check_row;
        } else {                              /* 3x3 sub-grids */
            params[i]->row = ((i - 2 * SIZE) / 3) * 3;
            params[i]->column = ((i - 2 * SIZE) % 3) * 3;
            worker = check_subgrid;
        }

        if (pthread_create(&tids[i], NULL, worker, params[i]) != 0) {
            perror("pthread_create");
            free(params[i]);
            failed = 1;
            break;
        }
        created++;
    }

    /* Wait for every thread that was started, then release its parameters */
    for (int i = 0; i < created; i++) {
        pthread_join(tids[i], NULL);
        free(params[i]);
    }

    if (failed)
        return 1;

    int valid = 1;
    for (int i = 0; i < NUM_THREADS; i++)
        if (!results[i])
            valid = 0;

    printf("\nSudoku puzzle is %s\n", valid ? "valid" : "invalid");
    return valid ? 0 : 1;
}