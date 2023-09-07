#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_COMMANDS 100
#define MAX_PATHS 100

char *paths[MAX_PATHS] = {"/bin/"};  
char error_message[30] = "An error has occurred\n";
int path_count = 1;

pid_t children[MAX_COMMANDS];
int child_count = 0;

bool is_builtin(char *cmd) {
    return strcmp(cmd, "exit") == 0 ||
           strcmp(cmd, "cd") == 0 ||
           strcmp(cmd, "path") == 0;
}

void display_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void freePaths() {
    for (int i = 0; i < path_count; i++) {
        free(paths[i]);
    }
    path_count = 0;
}

void addPathWithSlash(char *path) {
    size_t len = strlen(path);
    if (path[len - 1] == '/') {
        paths[path_count++] = strdup(path);
    } else {
        char *newPath = malloc(len + 2);  
        strcpy(newPath, path);
        strcat(newPath, "/");
        paths[path_count++] = newPath;
    }
}
void execute_command(char **command) {
    if (!command[0]) return;

    int output_fd = STDOUT_FILENO;
    int error_fd = STDERR_FILENO;

    int redirect_count = 0;
    char *redirect_file = NULL;
    int redirect_file_index = -1;

    for (int i = 0; command[i] != NULL; i++) {
        char *redirection = strchr(command[i], '>'); // Find the > symbol in the token
        if (redirection) {
            *redirection = '\0'; // Split the token at the > symbol
            redirect_count++;

            // Check the part after the > symbol
            if (*(redirection + 1)) {
                if (redirect_file) { // We've already found a file to redirect to
                    display_error();
                    return;
                }
                redirect_file = redirection + 1;
                redirect_file_index = i;
            } else if (command[i + 1] != NULL) {
                if (redirect_file) { // We've already found a file to redirect to
                    display_error();
                    return;
                }
                redirect_file = command[i + 1];
                redirect_file_index = i + 1;
                i++; // Increment to skip the file in the loop
            } else {
                display_error();
                return;
            }
        }
    }

    if (redirect_count > 1 || (redirect_count == 1 && command[redirect_file_index + 1] != NULL)) {
        display_error(); 
        return;
    }

    if (redirect_count == 1) {
        output_fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (output_fd == -1) {
            display_error();
            return;
        }
        error_fd = output_fd; // For the described behavior, we route stderr to the same file
    }

    if (is_builtin(command[0])) {
        // Handle the 'exit' command
        if (strcmp(command[0], "exit") == 0) {
            if (command[1] != NULL) {
                display_error();
            } else {
                exit(0);
            }
        }
        // Handle the 'cd' command
        else if (strcmp(command[0], "cd") == 0) {
            if (command[1] == NULL || command[2] != NULL) {
                display_error();
            } else {
                if (chdir(command[1]) != 0) {
                    display_error();
                }
            }
        }
        // Handle the 'path' command
        else if (strcmp(command[0], "path") == 0) {
            freePaths();
            for (int j = 1; command[j] != NULL; j++) {
                addPathWithSlash(command[j]);
            }
        }
    } else {
        pid_t child_pid = fork();

        if (child_pid == 0) {
           

            // Redirect standard output if necessary
            if (output_fd != STDOUT_FILENO) {
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            // Redirect standard error if necessary
            if (error_fd != STDERR_FILENO) {
                dup2(error_fd, STDERR_FILENO);
                if (output_fd != error_fd) {
                    close(error_fd);
                }
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
                display_error();
                exit(1);
            }
            execv(full_command, command);
            display_error();  // execv only returns if there's an error
            exit(1);
        } else if (child_pid > 0) {
            children[child_count++] = child_pid;
        } else {
            display_error();
            exit(1);
        }
    }
}



int main(int argc, char *argv[]) {
    char *command[MAX_COMMANDS], *token, *lineptr = NULL;
    size_t n;
    char *saveptr;

    FILE *input_source = stdin;
    bool batch_mode = false;

    if (argc == 2) {
        batch_mode = true;
        input_source = fopen(argv[1], "r");
        if (!input_source) {
            display_error();
            exit(1);
        }
    } else if (argc > 2) {
        display_error();
        exit(1);
    }

   while (1) {
    if (!batch_mode) {
        printf("witsshell> ");
    }
    if (getline(&lineptr, &n, input_source) == -1) {
        if (batch_mode) {
            fclose(input_source);
        }
        free(lineptr);
        exit(0);
    }

    int cmd_idx = 0;
    token = strtok_r(lineptr, " \t\n\r", &saveptr);
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            command[cmd_idx] = NULL;
            execute_command(command);
            cmd_idx = 0;
        } else {
            command[cmd_idx++] = token;
        }
        token = strtok_r(NULL, " \t\n\r", &saveptr);
    }
    command[cmd_idx] = NULL;

    if (cmd_idx > 0) {  // If there's any command left after the last `&`
        execute_command(command);
    }

    while (child_count > 0) {
        wait(NULL);
        child_count--;
    }
}


    free(lineptr);
    return 0;
}
