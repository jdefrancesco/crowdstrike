/*
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>

#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "cpcommon.h"
#include "dbg.h"



// Show usage of command.
static void
print_usage(const char *prog_name)
{
    assert(prog_name != NULL);

    fprintf(stderr, GREEN "\n==== Csprod ====" RESET "\n\n");
    fprintf(stderr, YELLOW "Description: "   RESET  " Read file line by line and pass sentences to a consumer via shared buffers.\n");
    fprintf(stderr, YELLOW "Usage:       "   RESET  " %s <SHARED_BUFFER_COUNT> <FILE>\n", prog_name);

    return;
}


// Print error to user via stderr.
// TODO: Make more robust to handle variadic options.
static void
print_error(const char *err_msg)
{
    assert(err_msg != NULL);
    //
    // ANSI Color macros are in dbg.h
    fprintf(stderr, RED "[!] ERROR:" RESET " %s\n", err_msg);

    return;
}


static void *
shm_worker_thread(void *arg) {
    size_t i = (size_t) arg;

    printf("Hello from %zu\n", i);

    return 0;
}


int main(int argc, char **argv) {

    FILE *input_file = NULL;
    unsigned long int shared_buff_count = 0;
    char *bad_char = NULL;


    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }


    // Check buffer count is actually a number.
    shared_buff_count = strtoul(argv[1], &bad_char, 10);
    if (shared_buff_count == 0 || *bad_char != '\0') {
        print_error("Invalid value for <SHARED_BUFFER_COUNT>");
        return EXIT_FAILURE;
    }

    // Ensure shared buffer count is within our range (1-16 inclusive).
    if (shared_buff_count > SHARED_MAX_BUFFERS) {
        print_error("Buffer count out of range, must be a value of 1-16, inclusive.");
        return EXIT_FAILURE;
    }


    // Open input file.
    if ((input_file = fopen(argv[2], "r")) == NULL) {
        print_error("Could not open input file");
        return EXIT_FAILURE;
    }


    // TODO: Make sure file isn't empty.
    // TODO: Setup signal handler.

    int shm_fd = 0;

    // Get shm_fd for shm_mgr. This object keeps some book-keeping about shared buffers.
Shm_Retry:
    errno = 0;
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR | O_CREAT | O_EXCL, 0770);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("File already exists. Cleaning it up to try again.");
            shm_unlink(SHM_MGR_NAME);
            goto Shm_Retry;
        }

        perror("shm_open");
        return EXIT_FAILURE;
    }

    dbg_print("Shared memory for shm_mgr opened successfully");

    // Set shm_mgr shared memory region size.
    if (ftruncate(shm_fd, sizeof(struct shm_mgr_t)) == -1) {
        fprintf(stderr, "sizeof(shm_mgr_t): %zu\n", sizeof(struct shm_mgr_t));
        perror("ftruncate");
        return EXIT_FAILURE;
    }


    dbg_print("Resized shm_fd region.");
    dbg_print("Time to mmap()");

    void *shm_addr = mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    close(shm_fd);

    // Initialize shared memory manager.
    shm_mgr_t *sm = (shm_mgr_t *) shm_addr;
    sm->sb_count = shared_buff_count;
    sm->buffer_idx = 0; // Currently unused.

    // Create thread pool. One thread per shared buffer.
    /* thread_pool_t *tp = NULL; */
    /* tp = calloc(shm_mgr->sh_buff_count, sizeof(struct thread_pool_t)); */
    pthread_t *tp = NULL;
    tp = calloc(sm->sb_count, sizeof(pthread_t));
    for (size_t i = 0; i < sm->sb_count; i++) {
        printf("starting thread %zu\n", i);
        int ret = pthread_create(&tp[i], NULL, shm_worker_thread, (void *)i);
        if (ret != 0) {
            print_error("Problem creating a thread.");
        }
    }


    dbg_print("All threads created");

    dbg_print("process file data");


    // Join all created threads.
    for (size_t i = 0; i < sm->sb_count; i++) {
        pthread_join(tp[i], NULL);
    }

    dbg_print("threads all finished");


    dbg_print("cleaning up");

    // Clean up.
    shm_unlink(SHM_MGR_NAME);
    fclose(input_file);
    free(tp);

    if (munmap(sm, sizeof(shm_mgr_t)) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
