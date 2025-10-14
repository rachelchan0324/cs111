/*
 * Word count application with one process per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "word_count.h"
#include "word_helpers.h"

/*
 * Read stream of counts and accumulate globally.
 */
void merge_counts(word_count_list_t *wclist, FILE *count_stream) {
    char *word;
    int count;
    int rv;
    while ((rv = fscanf(count_stream, "%8d\t%ms\n", &count, &word)) == 2) {
        add_word_with_count(wclist, word, count);
    }
    if ((rv == EOF) && (feof(count_stream) == 0)) {
        perror("could not read counts");
    } else if (rv != EOF) {
        fprintf(stderr, "read ill-formed count (matched %d)\n", rv);
    }
}

/*
 * main - handle command line, spawning one process per file.
 */
int main(int argc, char *argv[]) {
    /* Create the empty data structure. */
    word_count_list_t word_counts;
    init_words(&word_counts);

    if (argc <= 1) {
        /* Process stdin in a single process. */
        count_words(&word_counts, stdin);
    } 
    else {
        for (int i = 1; i < argc; i++) {
            // create a pipe for communication between parent and child.
            // pipefd[0] is the read end, pipefd[1] is the write end.
            // the child will write word counts to the pipe, and the parent will read them back

            // each child gets its own pipe
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            
            // fork a new child process
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            
            if (pid == 0) {
                // child process: counts words in one file
                // close the read end of the pipe (child only writes)
                close(pipefd[0]);
                
                // open the file assigned to this child
                FILE *fp = fopen(argv[i], "r");
                if (fp == NULL) {
                    perror("could not open file");
                    exit(EXIT_FAILURE);
                }
                
                // count words in this file
                word_count_list_t child_counts;
                init_words(&child_counts);
                count_words(&child_counts, fp);
                fclose(fp);
                
                // write the word counts to the pipe
                FILE *pipe_stream = fdopen(pipefd[1], "w");
                fprint_words(&child_counts, pipe_stream);
                fclose(pipe_stream); // also closes pipefd[1]
                
                // exit child process
                exit(EXIT_SUCCESS);
                
            } else {
                // parent process: collects results from child
                
                // close the write end of the pipe
                close(pipefd[1]);
                
                // convert the pipe's read end into a FILE* stream.
                FILE *pipe_stream = fdopen(pipefd[0], "r");
                merge_counts(&word_counts, pipe_stream);
                fclose(pipe_stream);  // also closes pipefd[0]
                
                // wait for child to finish, don't care about exit status
                wait(NULL);
            }
        }
    }

    /* Output final result of all process' work. */
    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
