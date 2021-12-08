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
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "cpcommon.h"
#include "dbg.h"

#define MAX_LINE_SIZE 256


// Mutex to used to to make sentence queue thread safe.
// This a lazy solution because of time constraints.
// Normally, I would make a full thread-safe queue interface.
static pthread_mutex_t sq_mtx = PTHREAD_MUTEX_INITIALIZER;


// Show usage of command.
static void
print_usage(const char *prog_name)
{
    assert(prog_name != NULL);
    fprintf(stderr, GREEN "\n==== Csprod ====" RESET "\n\n");
    fprintf(stderr, YELLOW "Description: "   RESET  " Read file line by line and pass "
            "sentences to a consumer via shared buffers.\n");
    fprintf(stderr, YELLOW "Usage:       "   RESET  " %s <SHARED_BUFFER_COUNT> <FILE>\n", prog_name);
    return;
}


// Print error to user via stderr.
// TODO: Make more robust to handle variadic options.
static void
print_error(const char *err_msg)
{
    assert(err_msg != NULL);
    // ANSI Color macros are in dbg.h
    fprintf(stderr, RED "[!] ERROR:" RESET " %s\n", err_msg);
    return;
}


static void *
shm_worker_thread(void *arg) {
    size_t i = (size_t) arg;
    int shm_fd = 0;
    char shm_name[256] = {0};
    void *shm_addr = NULL;

    // Construct shm name. Note, this won't show in the file system
    snprintf(shm_name, (sizeof(shm_name)-1), SHM_THREAD_NAME "%zu", i);
    printf("Hello, my name is %s\n", shm_name);

    // Thread sets up a shm buffer for sentences consumer will take.
    errno = 0;
    shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, 0660);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("Shm file object already exists. Cleaning it up to retry.");]
            shm_unlink(shm_name);
        }

        perror("shm_open");
        goto ThreadFail;
    }

    if(ftruncate(shm_fd, SHARED_BUFFER_SIZE)) {

    }

    void *shm_addr = mmap(NULL, SHARED_BUFFER_SIZE, PROT_READ | PROT_WRITE,
            MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    // Processing happens in here...




    if (munmap(shm_addr, SHARED_BUFFER_SIZE) == -1) {
        perror("munmap");
        return EXIT_FAILURE;

    }

    return EXIT_SUCCESS;

ThreadFail;
    if (shm_fd) close(shm_fd);

    return EXIT_FAILURE;

}


int main(int argc, char **argv) {

    FILE *input_file = NULL;
    unsigned long int shared_buff_count = 0;
    char *bad_char = NULL;
    int shm_fd = 0;

    // Will store the line we read from the file.
    char line[MAX_LINE_SIZE] = {0};

    if (argc != 3) {
        print_usage(argv[0]);
        goto ExitFail;
    }


    // Check buffer count is actually a number.
    shared_buff_count = strtoul(argv[1], &bad_char, 10);
    if (shared_buff_count == 0 || *bad_char != '\0') {
        print_error("Invalid value for <SHARED_BUFFER_COUNT>");
        goto ExitFail;
    }

    // Ensure shared buffer count is within our range (1-16 inclusive).
    if (shared_buff_count > SHARED_MAX_BUFFERS) {
        print_error("Buffer count out of range, must be a value of 1-16, inclusive.");
        goto ExitFail;
    }


    // Open input file.
    if ((input_file = fopen(argv[2], "r")) == NULL) {
        print_error("Could not open input file");
        goto ExitFail;
    }


    // TODO: Make sure file isn't empty.
    // TODO: Setup signal handler.


    // Get shm_fd for shm_mgr. This object keeps some book-keeping about shared buffers.
    // TODO: Explain MORE
    errno = 0;
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR | O_CREAT, 0660);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            // TODO: We need root to cleanup shm. Check uid and let user know.
            print_error("Shm file object already exists. Cleaning it up to retry.");
            shm_unlink(SHM_MGR_NAME);
        }

        perror("shm_open");
        goto ExitFail;
    }


    dbg_print("Shared memory for shm_mgr opened successfully");

    // Set shm_mgr shared memory region size.
    if (ftruncate(shm_fd, sizeof(struct shm_mgr_t)) == -1) {
        perror("ftruncate");
        return EXIT_FAILURE;
    }


    dbg_print("Resized shm_fd region.");
    dbg_print("Time to mmap()");

    // The consumer will map this as well to know how many buffers will be shared.
    void *shm_addr = mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    // No longer needed.
    close(shm_fd);

    // TODO: Add lock semaphore here
    // TODO: OR USE A FIFO!
    // Initialize shared memory manager.
    shm_mgr_t *sm = (shm_mgr_t *) shm_addr;
    sm->sb_count = shared_buff_count;
    sm->buffer_idx = 0; // Currently unused.
    // TODO: Unlock sem

    // Create thread pool. One thread per shared buffer.
    pthread_t *tp = NULL;
    tp = calloc(sm->sb_count, sizeof(pthread_t));
    for (size_t i = 0; i < sm->sb_count; i++) {
        printf("starting thread %zu\n", i);
        int ret = pthread_create(&tp[i], NULL, shm_worker_thread, (void *)i);
        if (ret != 0) {
            print_error("Problem creating a thread.");
            goto ExitFail;
        }
    }

    dbg_print("All threads created");

    dbg_print("process file data");

    // Will use fgets() as opposed to getline() because we are setting strict requirements
    // for line size.
    while(fgets(line, sizeof(line), input_file) != NULL) {
        // Strip off new line
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }

        // 1. Create new sqnode
        // 2. Acquire MTX
        // 3. enqueue_sentence(sqnode)
        // 4. Unlock MTX
        //

        // Output line to console.
        printf("%s\n", line);

        // Clear line buffer for new sentence.
        memset(line, sizeof(line), '\0');

    }


    // Check for any errors while processing file stream.
    if (ferror(input_file)) {
        print_error("Error on input_file");
    }

    if (!feof(input_file)) {
        print_error("Unable to process entire file");
    }
    clearerr(input_file);

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

    if (munmap(shm_addr, sizeof(shm_mgr_t)) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;

ExitFail:
    if (shm_fd) shm_unlink(SHM_MGR_NAME);
    if (input_file) fclose(input_file);
    if (tp) free(tp);
    if (shm_addr) munmap(shm_addr, sizeof(shm_mgr_t));

    return EXIT_FAILURE;


}
