#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

void directory_summary(const char *dir_name);
int find_entry(const char *dir_name, const char *entry_name);
void create_subdirectory(const char *dir_name, const char *subdir_name, mode_t mode);
int remove_subdirectory(const char *dir_name, const char *subdir_name);
int count_processes(void);

int main(void) {
    int choice;
    char dir_name[256];
    char entry_name[256];
    char subdir_name[256];

    while (1) {
        printf("\n----- Directory Analyzer -----\n");
        printf("1: Display directory summary\n");
        printf("2: Find an entry\n");
        printf("3: Create a subdirectory\n");
        printf("4: Remove an empty subdirectory\n");
        printf("5: Count running processes\n");
        printf("99: Quit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) break;

        if (choice == 99) {
            printf("Exiting program.\n");
            break;
        }

        switch (choice) {
            case 1:
                printf("Enter directory name: ");
                if (scanf("%255s", dir_name) != 1) break;
                directory_summary(dir_name);
                break;

            case 2: {
                printf("Enter directory name: ");
                if (scanf("%255s", dir_name) != 1) break;
                printf("Enter entry name to search: ");
                if (scanf("%255s", entry_name) != 1) break;

                int result = find_entry(dir_name, entry_name);
                if (result == 1) {
                    printf("Entry found.\n");
                } else if (result == 0) {
                    printf("Entry not found.\n");
                }
                break;
            }

            case 3: {
                unsigned int mode;
                printf("Enter parent directory name: ");
                if (scanf("%255s", dir_name) != 1) break;
                printf("Enter new subdirectory name: ");
                if (scanf("%255s", subdir_name) != 1) break;
                printf("Enter directory mode (e.g., 0755): ");
                if (scanf("%o", &mode) != 1) break;

                create_subdirectory(dir_name, subdir_name, (mode_t)mode);
                break;
            }

            case 4:
                printf("Enter parent directory name: ");
                if (scanf("%255s", dir_name) != 1) break;
                printf("Enter subdirectory name to remove: ");
                if (scanf("%255s", subdir_name) != 1) break;

                if (remove_subdirectory(dir_name, subdir_name) == 0) {
                    printf("Directory removed successfully.\n");
                }
                break;

            case 5: {
                int procs = count_processes();
                if (procs != -1) {
                    printf("Number of running processes: %d\n", procs);
                }
                break;
            }

            default:
                printf("Invalid choice.\n");
        }
    }
    return 0;
}

void directory_summary(const char *dir_name) {
    DIR *dp = opendir(dir_name);
    if (dp == NULL) {
        perror("Error opening directory");
        return;
    }

    int file_count = 0;
    int dir_count = 0;
    int hidden_count = 0;

    struct dirent *entry;
    struct stat st;
    char path[512];

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (entry->d_name[0] == '.') {
            hidden_count++;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);
        if (stat(path, &st) == -1) {
            perror("Error getting file info");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            file_count++;
        } else if (S_ISDIR(st.st_mode)) {
            dir_count++;
        }
    }

    closedir(dp);

    printf("Directory: %s\n", dir_name);
    printf("Regular files: %d\n", file_count);
    printf("Subdirectories: %d\n", dir_count);
    printf("Hidden entries: %d\n", hidden_count);
}

int find_entry(const char *dir_name, const char *entry_name) {
    DIR *dp = opendir(dir_name);
    if (dp == NULL) {
        perror("Error opening directory");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, entry_name) == 0) {
            closedir(dp);
            return 1;
        }
    }

    closedir(dp);
    return 0;
}

void create_subdirectory(const char *dir_name, const char *subdir_name, mode_t mode) {
    int exists = find_entry(dir_name, subdir_name);
    if (exists == 1) {
        printf("Error: an entry named %s already exists.\n", subdir_name);
        return;
    }
    if (exists == -1) {
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir_name, subdir_name);

    if (mkdir(path, mode) == -1) {
        perror("Error creating directory");
        return;
    }

    printf("Directory %s created successfully.\n", subdir_name);
}

int remove_subdirectory(const char *dir_name, const char *subdir_name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir_name, subdir_name);

    if (rmdir(path) == -1) {
        perror("Error removing directory");
        return -1;
    }

    return 0;
}

static int is_all_digits(const char *name) {
    if (name[0] == '\0') {
        return 0;
    }
    for (int i = 0; name[i] != '\0'; i++) {
        if (!isdigit((unsigned char)name[i])) {
            return 0;
        }
    }
    return 1;
}

int count_processes(void) {
    DIR *dp = opendir("/proc");
    if (dp == NULL) {
        perror("Error opening /proc");
        return -1;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (is_all_digits(entry->d_name)) {
            count++;
        }
    }

    closedir(dp);
    return count;
}