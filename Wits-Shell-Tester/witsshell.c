#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

#define MAX_COMMANDS 100
#define MAX_PATHS 10

char *paths[MAX_PATHS] = {"/bin/"};  // Initial path
char error_message[30] = "An error has occurred\n";
int path_count = 1;

bool is_builtin(char *cmd) {
    return strcmp(cmd, "exit") == 0 ||
           strcmp(cmd, "cd") == 0 ||
           strcmp(cmd, "path") == 0;
}

int main(int argc, char *argv[]) {
    pid_t child;
    char *command[MAX_COMMANDS], *token, *lineptr = NULL;
    size_t n;
    int status;
    char *saveptr;
    size_t i;

    FILE *input_source = stdin;  // Default to stdin
    bool batch_mode = false;

    if (argc == 2) {
        batch_mode = true;
        input_source = fopen(argv[1], "r");
        if (!input_source) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    } else if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    while (1) {
        if (getline(&lineptr, &n, input_source) == -1) {  
            if (batch_mode) {
                fclose(input_source);  // Close batch file if in batch mode
            }
            free(lineptr);
            exit(0);
        }

        i = 0;
        token = strtok_r(lineptr, " \t\n\r", &saveptr);
        while (i < MAX_COMMANDS - 1 && token != NULL) {
            command[i] = token;
            token = strtok_r(NULL, " \t\n\r", &saveptr);
            i++;
        }
        command[i] = NULL;

        if (is_builtin(command[0])) {
            if (strcmp(command[0], "exit") == 0) {
                if (command[1] != NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                } else {
                    free(lineptr);
                    exit(0);
                }
            } else if (strcmp(command[0], "cd") == 0) {
                if (command[1] == NULL || command[2] != NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                } else {
                    if (chdir(command[1]) != 0) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                    }
                }
            } else if (strcmp(command[0], "path") == 0) {
                path_count = 0;
                for (int j = 1; j < MAX_COMMANDS && command[j] != NULL; j++) {
                    paths[path_count++] = command[j];
                }
            }
            continue;
        }

        char full_command[512];
        bool command_found = false;

        for (int j = 0; j < path_count; j++) {
            snprintf(full_command, sizeof(full_command), "%s%s", paths[j], command[0]);
            if (access(full_command, X_OK) == 0) {
                command_found = true;
                break;
            }
        }

        if (!command_found) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        child = fork();
        if (child == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(lineptr);
            exit(EXIT_FAILURE);
        }

        if (child == 0) {
            if (execv(full_command, command) == -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(EXIT_FAILURE);
            }
        } else {
            wait(&status);
        }
    }

    free(lineptr);
    return 0;
}