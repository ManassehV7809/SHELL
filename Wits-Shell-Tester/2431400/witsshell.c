#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_COMMANDS 1000

char error_message[30] = "An error has occurred\n";
pid_t children[MAX_COMMANDS];
int child_count = 0;


void redirect_fd(int old_fd, int new_fd) {
    if (old_fd != new_fd) {
        dup2(old_fd, new_fd);
        close(old_fd);
    }
}

typedef struct Node {
    char* path;
    struct Node* next;
} Node;

Node* make_Path_Node(const char* pathValue) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    if (!newNode) {
        exit(1);
    }
    
    size_t len = strlen(pathValue);
    if (pathValue[len - 1] == '/') {
        newNode->path = strdup(pathValue); // Duplicate the string for storage
    } else {
        // Allocate space for the existing path, the additional slash, and the null terminator
        newNode->path = malloc(len + 2);  
        strcpy(newNode->path, pathValue);
        strcat(newNode->path, "/");
    }

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
void cleanPath() {
    Node* current = paths_head;
    Node* nextNode = NULL;

    while (current != NULL) {
        nextNode = current->next; 
        free(current->path);       
        free(current);             
        current = nextNode;        
    }

    paths_head = NULL;  
}


void tokenize_commands(char* line, char*** commands, int* command_count) {
    char** result = (char**)malloc(MAX_COMMANDS * sizeof(char*));
    int cmd_idx = 0;
    
    char* saveptr;  
    char* command = strtok_r(line, "&\n", &saveptr);  
    
    while (command) {
        result[cmd_idx++] = strdup(command);  
        command = strtok_r(NULL, "&\n", &saveptr);  
    }

    result[cmd_idx] = NULL;  
    *commands = result;
    *command_count = cmd_idx;
}


void clean_commands(char*** commands) {
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

        // Check for redirection symbol as first char of the first command or multiple redirection symbols
        if ((i == 0 && redirection == command[i]) || redirect_count > 1) {
            display_error();
            return;
        }

        // Check if filename follows the > symbol in the same token
        if (*(redirection + 1)) {
            if (redirect_file) {
                display_error();
                return;
            }
            redirect_file = redirection + 1;
            redirect_file_index = i;
        } 
        // Check if filename is in the next token
        else if (command[i + 1] != NULL) {
            if (redirect_file) {
                display_error();
                return;
            }
            redirect_file = command[i + 1];
            redirect_file_index = i + 1;
            i++; // Increment to skip the file in the loop
        } 
        // Missing filename
        else {
            display_error();
            return;
        }
    }
}

// Ensure there's nothing after the redirection file
if (redirect_count == 1 && command[redirect_file_index + 1] != NULL) {
    display_error(); 
    return;
}

// Handle file redirection
if (redirect_count == 1) {
    output_fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (output_fd == -1) {
        display_error();
        return;
    }
    error_fd = output_fd; 
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
            cleanPath();
            for (int j = 1; command[j] != NULL; j++) {
                Node* newNode = make_Path_Node(command[j]);
                newNode->next = paths_head;
                paths_head = newNode;
            }
        }
        //Handle external commands
    }  else {
        pid_t child_pid = fork();

        if (child_pid == 0) {
    // Redirect standard output if necessary
    redirect_fd(output_fd, STDOUT_FILENO);
    // Redirect standard error if necessary
    redirect_fd(error_fd, STDERR_FILENO);

            char full_command[512];
            bool command_found = false;
            Node* current_path = paths_head;


            while (current_path) {
    // Construct the full command path
    char full_command[512];
    strcpy(full_command, current_path->path);
    strcat(full_command, command[0]);

    // Check if the command is accessible and executable
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
            display_error(); 
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
        paths_head = make_Path_Node("/bin/");
    }

    char* input_line = NULL;
    size_t input_line_length;
    FILE *input_stream = stdin;
    bool is_batch_mode = false;

    if (argc == 2) {
        is_batch_mode = true;
        input_stream = fopen(argv[1], "r");
        if (!input_stream) {
            display_error();
            exit(1);
        }
    } else if (argc > 2) {
        display_error();
        exit(1);
    }

    while (1) {
        if (!is_batch_mode) {
            printf("witsshell> ");
        }
        if (getline(&input_line, &input_line_length, input_stream) == -1) {
            if (is_batch_mode) {
                fclose(input_stream);
            }
            free(input_line);
            exit(0);
        }

        char** command_list = NULL;
        int num_of_commands = 0;
        tokenize_commands(input_line, &command_list, &num_of_commands);

        for (int i = 0; i < num_of_commands; ++i) {
            char* individual_tokens[MAX_COMMANDS];
            int token_count = 0;  
            char* single_token;
            char* strtok_context;  // Rename to reflect the context used by strtok_r
            
            single_token = strtok_r(command_list[i], " \t\n\r", &strtok_context);
            while (single_token != NULL) {
                individual_tokens[token_count++] = single_token;
                single_token = strtok_r(NULL, " \t\n\r", &strtok_context);
            }
            
            individual_tokens[token_count] = NULL;
            execute_command(individual_tokens);
        }

        clean_commands(&command_list);

        while (child_count > 0) {
            wait(NULL);
            child_count--;
        }
    }

    free(input_line);
    return 0;
}


