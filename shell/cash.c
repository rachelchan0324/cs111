#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command.h"

bool shell_is_interactive = true;

static void print_usage(void) {
    printf(u8"\U0001F309 \U0001F30A \U00002600\U0000FE0F "
           u8"cash: The California Shell "
           u8"\U0001F334 \U0001F43B \U0001F3D4\U0000FE0F\n");
    printf("Usage: cash [script.sh]\n");
    printf("\n");
    printf("Built-in commands:\n");
    printf("help: Print out this usage information.\n");
    /* If you wish, you can add a summary of each built-in you implement. */
    printf("\n");
}

static bool handle_builtin_command(const struct command *cmd) {
    const char *first_token = command_get_token_by_index(cmd, 0);
    if (strcmp(first_token, "help") == 0) {
        print_usage();
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
        shell_is_interactive = false;
    }
    if (!isatty(STDIN_FILENO)) {
        shell_is_interactive = false;
    }
    if (!shell_is_interactive) {
        output_stream = NULL;
    }

    struct command cmd;
    while (prompt_and_read_command(output_stream, input_stream, &cmd)) {
        if (command_get_num_tokens(&cmd) > 0) {
            handle_builtin_command(&cmd);
        }
        command_deallocate(&cmd);
    }
    command_deallocate(&cmd);

    fclose(input_stream);

    return EXIT_SUCCESS;
}
