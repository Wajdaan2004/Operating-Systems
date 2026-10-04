#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Insufficient parameters passed.\n");
        exit(EXIT_FAILURE);
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    int in_fd = open(input_file, O_RDONLY);
    if (in_fd == -1) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        perror("Error creating output file");
        close(in_fd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(in_fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t total_written = 0;
        while (total_written < bytes_read) {
            ssize_t bytes_written = write(out_fd, buffer + total_written,
                                          bytes_read - total_written);
            if (bytes_written == -1) {
                perror("Error writing to output file");
                close(in_fd);
                close(out_fd);
                exit(EXIT_FAILURE);
            }
            total_written += bytes_written;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading input file");
        close(in_fd);
        close(out_fd);
        exit(EXIT_FAILURE);
    }

    if (close(in_fd) == -1) {
        perror("Error closing input file");
        exit(EXIT_FAILURE);
    }
    if (close(out_fd) == -1) {
        perror("Error closing output file");
        exit(EXIT_FAILURE);
    }

    printf("The contents of file %s have been successfully copied into the %s file.\n",
           input_file, output_file);
    return 0;
}