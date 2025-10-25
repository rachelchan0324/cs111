#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "command.h"

bool shell_is_interactive = true;

static void setup_signal_handling(void) {
    if (shell_is_interactive) {
        // ignore these signals when interactive
        signal(SIGINT, SIG_IGN);    // Ctrl-C
        signal(SIGQUIT, SIG_IGN);   // Ctrl-'\'
        signal(SIGTERM, SIG_IGN);   // termination request
        signal(SIGTSTP, SIG_IGN);   // Ctrl-Z (suspend)
        signal(SIGCONT, SIG_IGN);   // continue after suspend
        signal(SIGTTIN, SIG_IGN);   // background read from terminal
        signal(SIGTTOU, SIG_IGN);   // background write to terminal
    }
}

static void print_usage(void) {
    printf(u8"\U0001F309 \U0001F30A \U00002600\U0000FE0F "
           u8"cash: The California Shell "
           u8"\U0001F334 \U0001F43B \U0001F3D4\U0000FE0F\n");
    printf("Usage: cash [script.sh]\n");
    printf("\n");
    printf("Built-in commands:\n");
    printf("help: Print out this usage information.\n");
    printf("exit <code>: Exit the shell with the specified exit code.\n");
    printf("cd <directory>: Change the current working directory.\n");
    printf("pwd: Print the current working directory.\n");
    printf("\n");
}

static void exit_command(const struct command *cmd) {
    int exit_code = 0;
    if (command_get_num_tokens(cmd) > 1) {
        exit_code = atoi(command_get_token_by_index(cmd, 1)); // parse exit code from argument
    }
    exit(exit_code); // exit with specified or default code
}

static void cd_command(const struct command *cmd) {
    // use provided directory or home directory if none given
    const char *dir = (command_get_num_tokens(cmd) > 1) ? 
                      command_get_token_by_index(cmd, 1) : getenv("HOME");
    if (chdir(dir) != 0) {
        perror("cd"); // print error if directory change fails
    }
}

static void pwd_command(const struct command *cmd) {
    char cwd[4096]; // reasonable buffer size for current directory
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd); // print current working directory
    }
}

// resolve program path and execute
static void run_program(char *prog, char **argv) {
    extern char **environ;
    
    // restore default signal handlers for child process
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    
    // if program has '/', use it directly
    if (strchr(prog, '/')) {
        execve(prog, argv, environ);
    } else {
        // search PATH
        char *PATH = getenv("PATH");
        if (PATH) {
            char prog_path[1000];
            char *path_dir = strtok(PATH, ":");
            while (path_dir) {
                sprintf(prog_path, "%s/%s", path_dir, prog);
                if (access(prog_path, X_OK) == 0) {
                    execve(prog_path, argv, environ);
                }
                path_dir = strtok(NULL, ":");
            }
        }
    }
    fprintf(stderr, "cash: %s: command not found\n", prog);
    exit(EXIT_FAILURE);
}

static void execute_external_command(const struct command *cmd) {
    extern char **environ;
    
    size_t num_tokens = command_get_num_tokens(cmd);
    char **argv = malloc((num_tokens + 1) * sizeof(char *));
    char *input_file = NULL;
    char *output_file = NULL;
    size_t argv_index = 0;
    
    // parse tokens to separate command from redirection
    for (size_t i = 0; i < num_tokens; i++) {
        const char *token = command_get_token_by_index(cmd, i);
        
        if (strcmp(token, "<") == 0) {
            // input redirection
            if (i + 1 < num_tokens) {
                input_file = (char *)command_get_token_by_index(cmd, i + 1);
                i++; // skip the filename
            }
        } else if (strcmp(token, ">") == 0) {
            // output redirection
            if (i + 1 < num_tokens) {
                output_file = (char *)command_get_token_by_index(cmd, i + 1);
                i++; // skip the filename
            }
        } else {
            // regular command argument
            argv[argv_index++] = (char *)token;
        }
    }
    argv[argv_index] = NULL;
    
    pid_t pid = fork();
    if (pid == 0) {
        // child process - handle redirection
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror(input_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO); // duplicates a file descriptor, makes standard input point to file we opened
            close(fd);
        }
        
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(output_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO); // makes standard output point to file we opened
            close(fd);
        }
        
        run_program(argv[0], argv);

    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork");
    }
    
    free(argv);
}

static bool handle_builtin_command(const struct command *cmd) {
    const char *first_token = command_get_token_by_index(cmd, 0);
    
    if (strcmp(first_token, "help") == 0) {
        print_usage();
        return true;
    } else if (strcmp(first_token, "exit") == 0) {
        exit_command(cmd);
        return true;
    } else if (strcmp(first_token, "cd") == 0) {
        cd_command(cmd);
        return true;
    } else if (strcmp(first_token, "pwd") == 0) {
        pwd_command(cmd);
        return true;
    } else {
        return false;
    }
}

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        print_usage();
        return EXIT_FAILURE;
    }

    /*
     * This code checks if the shell is being run interactively. Certain
     * aspects of the shell's functionality, like printing out the "cash"
     * prompt on each line, only apply when the shell is run interactively.
     */
    FILE *input_stream = stdin;
    FILE *output_stream = stdout;
    if (argc == 2) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
        shell_is_interactive = false; // script file provided, not interactive
    }
    if (!isatty(STDIN_FILENO)) {
        shell_is_interactive = false; // stdin is not a terminal (e.g., piped input, redirected)
    }
    if (!shell_is_interactive) {
        output_stream = NULL;
    }

    // set up signal handling
    setup_signal_handling();

    struct command cmd;
    while (prompt_and_read_command(output_stream, input_stream, &cmd)) {
        if (command_get_num_tokens(&cmd) > 0) {
            if (!handle_builtin_command(&cmd)) {
                execute_external_command(&cmd); // not a built-in command, try to execute as external program
            }
        }
        command_deallocate(&cmd);
    }
    command_deallocate(&cmd);

    fclose(input_stream);

    return EXIT_SUCCESS;
}
