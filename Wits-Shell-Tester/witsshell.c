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

char error_message[30] = "An error has occurred\n";
pid_t children[MAX_COMMANDS];
int child_count = 0;
typedef struct Node {
    char* path;
    struct Node* next;
} Node;

Node* createNode(const char* pathValue) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    if (!newNode) {
        exit(1);
    }
    newNode->path = strdup(pathValue); // Duplicate the string for storage
    newNode->next = NULL;
    return newNode;
}

Node* paths_head = NULL;


bool is_builtin(char *cmd) {
    return strcmp(cmd, "exit") == 0 ||
           strcmp(cmd, "cd") == 0 ||
           strcmp(cmd, "path") == 0;
}

void display_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void freePaths() {
    Node* current = paths_head;
    Node* next;
    while (current != NULL) {
        next = current->next;
        free(current->path);
        free(current);
        current = next;
    }
    paths_head = NULL;
}


void addPathWithSlash(char *path) {
    Node* newNode = malloc(sizeof(Node));
    size_t len = strlen(path);

    if (path[len - 1] == '/') {
        newNode->path = strdup(path);
    } else {
        newNode->path = malloc(len + 2);  
        strcpy(newNode->path, path);
        strcat(newNode->path, "/");
    }
    newNode->next = paths_head;
    paths_head = newNode;
}

// Modified tokenization to handle commands combined with '&'
void tokenize_commands(char* line, char*** commands, int* command_count) {
    char** result = (char**)malloc(MAX_COMMANDS * sizeof(char*));
    char* cmd_start = line;
    int cmd_idx = 0;
    
    for (char* ptr = line; ; ++ptr) {
        if (*ptr == '&' || *ptr == '\0' || *ptr == '\n') {
            if (ptr > cmd_start) {
                int length = ptr - cmd_start;
                char* command = (char*)malloc((length + 1) * sizeof(char));
                strncpy(command, cmd_start, length);
                command[length] = '\0';
                result[cmd_idx++] = command;
            }
            
            if (*ptr == '\0' || *ptr == '\n') {
                break;
            }
            cmd_start = ptr + 1; 
        }
    }
    
    result[cmd_idx] = NULL;  // NULL-terminate the list of commands
    *commands = result;
    *command_count = cmd_idx;
}

void free_commands(char*** commands) {
    for (int i = 0; (*commands)[i] != NULL; ++i) {
        free((*commands)[i]);
    }
    free(*commands);
    *commands = NULL;
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
                        Node* current_path = paths_head;
                        while (current_path) {
                            snprintf(full_command, sizeof(full_command), "%s%s", current_path->path, command[0]);
                            if (access(full_command, X_OK) == 0) {
                                command_found = true;
                                break;
                            }
                            current_path = current_path->next;
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
    if (!paths_head) {
        paths_head = createNode("/bin/");
    }

    char* lineptr = NULL;
    size_t n;
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

        char** commands = NULL;
        int command_count = 0;
        tokenize_commands(lineptr, &commands, &command_count);

        for (int i = 0; i < command_count; ++i) {
            char* command_tokens[MAX_COMMANDS];
            int cmd_idx = 0;
            char* token;
            char* saveptr;
            
            token = strtok_r(commands[i], " \t\n\r", &saveptr);
            while (token != NULL) {
                command_tokens[cmd_idx++] = token;
                token = strtok_r(NULL, " \t\n\r", &saveptr);
            }
            
            command_tokens[cmd_idx] = NULL;
            execute_command(command_tokens);
        }

        free_commands(&commands);

        while (child_count > 0) {
            wait(NULL);
            child_count--;
        }
    }

    free(lineptr);
    return 0;
}

