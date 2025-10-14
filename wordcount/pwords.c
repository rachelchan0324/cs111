/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright (C) 2019 University of California, Berkeley
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
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "word_count.h"
#include "word_helpers.h"

struct thread_args {
    char *filename;
    word_count_list_t *wclist;
};

/* thread function to process one file */
void *process_file(void *args) {
    struct thread_args *ta = (struct thread_args *)args; // cast argument back to thread_args
    FILE *fp = fopen(ta->filename, "r");
    if (fp == NULL) {
        perror("Could not open file");
        return NULL;
    }
    count_words(ta->wclist, fp);
    fclose(fp);
    return NULL;
}

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char *argv[]) {
    /* Create the empty data structure. */
    word_count_list_t word_counts;
    init_words(&word_counts);

    if (argc <= 1) {
        /* Process stdin in a single thread. */
        count_words(&word_counts, stdin);
    } else {
        int num_threads = argc - 1;
        pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
        struct thread_args *args = malloc(num_threads * sizeof(struct thread_args));
        
        // create threads to process each file
        for (int i = 0; i < num_threads; i++) {
            args[i].filename = argv[i + 1];
            args[i].wclist = &word_counts; // pass pointer to shared word count list
            
            if (pthread_create(&threads[i], NULL, process_file, &args[i]) != 0) {
                perror("ERROR creating thread");
                exit(1);
            }
        }
        
        // wait for all threads to complete
        for (int i = 0; i < num_threads; i++)
            pthread_join(threads[i], NULL);

        free(args);
        free(threads);
    }

    /* Output final result of all threads' work. */
    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
