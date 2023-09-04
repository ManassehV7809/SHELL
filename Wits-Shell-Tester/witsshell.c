#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

#define MAX_COMMANDS 100
#define MAX_PATHS 10

char *paths[MAX_PATHS] = {"/bin/"};  // Initial path
int path_count = 1;

bool is_builtin(char *cmd) {
    return strcmp(cmd, "exit") == 0 ||
           strcmp(cmd, "cd") == 0 ||
           strcmp(cmd, "path") == 0;
}

int main(int argc, char *argv[], char *envp[]) {
    pid_t child;
    char *command[MAX_COMMANDS], *token, *lineptr = NULL; 
    size_t n;
    int status;
    char *saveptr;
    size_t i;

    while (1) {
        printf("witsshell> ");
        if (getline(&lineptr, &n, stdin) == -1) {  // EOF (e.g., from Ctrl-D) encountered
            free(lineptr);
            exit(0);
        }
        
        i = 0;
        token = strtok_r(lineptr, " \t\n\r", &saveptr);
        while (i < MAX_COMMANDS - 1 && token != NULL) {  // Reserve one space for NULL termination
            command[i] = token;
            token = strtok_r(NULL, " \t\n\r", &saveptr);
            i++;
        }
        command[i] = NULL;  // NULL-terminate the command array

        if (is_builtin(command[0])) {
            if (strcmp(command[0], "exit") == 0) {
                if (command[1] != NULL) {
                    fprintf(stderr, "exit: Too many arguments\n");
                } else {
                    free(lineptr);
                    exit(0);
                }
            } else if (strcmp(command[0], "cd") == 0) {
                if (command[1] == NULL || command[2] != NULL) {
                    fprintf(stderr, "cd: Invalid arguments\n");
                } else {
                    if (chdir(command[1]) != 0) {
                        perror("cd");
                    }
                }
            } else if (strcmp(command[0], "path") == 0) {
                path_count = 0;
                for (int j = 1; j < MAX_COMMANDS && command[j] != NULL; j++) {
                    paths[path_count++] = command[j];
                }
            }
            continue;  // Skip the rest of the loop for built-in commands
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
            fprintf(stderr, "%s: Command not found\n", command[0]);
            continue;
        }

        child = fork();
        if (child == -1) {
            perror("fork");
            free(lineptr);
            exit(EXIT_FAILURE);
        }

        if (child == 0) {  // Child process
            if (execv(full_command, command) == -1) {
                perror("execv");
                exit(EXIT_FAILURE);
            }
        } else {  // Parent process
            wait(&status);
        }
    }

    // Shouldn't reach here but just in case
    free(lineptr);
    return 0;
}
