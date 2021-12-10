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
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "cpcommon.h"
#include "dbg.h"
#include "squeue.h"



// Mutex to used to to make sentence queue thread safe.
// This a lazy solution because of time constraints.
// Normally, I would make a full thread-safe queue interface.
/* static pthread_mutex_t sq_mtx = PTHREAD_MUTEX_INITIALIZER; */
/* static pthread_cond_t sq_cond = PTHREAD_COND_INITIALIZER; */

// Our sentence queue.
static squeue_t *sq = NULL;

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

// TODO: Add signal handler!
void
signal_handler(int sig)
{
    (void) sig;
    // Signal handler.
}

static void *
shm_worker_thread(void *arg) {

    size_t i = (size_t) arg;
    int shm_fd = 0;
    char shm_name[256] = {0};
    void *shm_addr = NULL;

    // Semaphore mutex name used between two processes.
    char sem_mtx_name[256] = {0};
    sem_t *shbuff_sem_mtx = NULL;

    // We dequeue a line from the queue into this temp buffer.
    char temp_line[MAX_LINE_SIZE] = {0};


    // If fail to dequeue 10 times, break loop
    size_t queue_fail = 0;

    // Construct sem mutex name.
    snprintf(sem_mtx_name, (sizeof(sem_mtx_name)-1), SEM_MUTEX_NAME "%zu", i);
    // Construct shm name. Note, this won't show in the file system
    snprintf(shm_name, (sizeof(shm_name)-1), SHM_THREAD_NAME "%zu", i);
    printf("Hello, my name is %s\n", shm_name);

    // Create sem mtx we use for sync. between different processes.
    /* if ((shbuff_sem_mtx = sem_open(sem_mtx_name, O_CREAT, 0660, 0)) */
    /*         == SEM_FAILED){ */
    /*     perror("sem_open"); */
    /*     goto Exit; */
    /* } */

    errno = 0;
    if(shm_unlink(shm_name) == -1) {
        // That is fine, we don't want
        if (errno == ENOENT) {
            dbg_print("no entry for the shm_name");
        }
    }

    // Thread sets up a shm buffer for sentences consumer will take.
    errno = 0;
    shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0660);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("Shm file object already exists. Cleaning it up to retry.");
            shm_unlink(shm_name);
        }

        perror("shm_open");
        goto Exit;
    }


    // Resize our shared memory region.
    dbg_print("Calling ftruncate from thread");
    if(ftruncate(shm_fd, (off_t)SHARED_BUFFER_SIZE) == -1) {
        if (errno == EBADF || errno == EINVAL) {
            print_error("THREAD: bad fd not open for writing");
        }
        perror("ftruncate");
        goto Exit;
    }

    dbg_print("ftruncate from thread success...");

    dbg_print("Calling create_shared_buffer");
    shm_addr = create_shared_buffer(shm_fd, SHARED_BUFFER_SIZE);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        goto Exit;
    }

    dbg_print("create_shared_buffer called success.");

    for (;;) {

        if (!squeue_dequeue(sq, temp_line)) {
            printf("[+] Queue is currently empty.\n");
            queue_fail++;
            if (queue_fail == 10) {
                printf("thread - enqueue failed ten times. breaking\n");
                break;
            }
        }
    //
    //  if (!holding sem_mtx)
    //      sem_wait()
    //
    //  pack shared buffer with sentences.
    //
    //  if buff remaining space < 256
    //      sem_post()
    }


    if (munmap(shm_addr, SHARED_BUFFER_SIZE) == -1) {
        perror("munmap");
        goto Exit;
    }

Exit:

    if (shm_addr) munmap(shm_addr, SHARED_BUFFER_SIZE);
    shm_unlink(shm_name);
    if (shm_fd) close(shm_fd);
    return NULL;
}


int main(int argc, char **argv) {

    FILE *input_file = NULL;
    unsigned long int shared_buff_count = 0;
    char *bad_char = NULL;
    int shm_fd = 0;
    void *shm_addr = NULL;

    // Signal handler.
    struct sigaction sa = {
        .sa_handler = signal_handler,
        .sa_flags = SA_RESTART,
    };

    // Mutex semaphore for sharing the shm_mgr_t struct betweeen processes.
    /* sem_t *ms_shm_mgr = NULL; */

    // tp references the little thread pool we create.
    pthread_t *tp = NULL;

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


    // Setup signal handler.
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        goto ExitFail;
    }


    // TODO: Add Mtx Semaphore;

    // Get shm_fd for shm_mgr. This object keeps some book-keeping about shared buffers.
    // TODO: Explain MORE
    errno = 0;
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR | O_CREAT | O_EXCL, 0660);
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
    errno = 0;
    if (ftruncate(shm_fd, sizeof(struct shm_mgr_t)) == -1) {
        if ((errno  == EINVAL) || (errno == EBADF)) {
            fprintf(stderr, "[!] ftruncate fd not open for writing\n");
        }
        perror("ftruncate");
        goto ExitFail;
    }


    dbg_print("Resized shm_fd region.");
    dbg_print("Time to mmap()");

    // The consumer will map this as well to know how many buffers will be shared.
    shm_addr = mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE,
            MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        goto ExitFail;
    }
    close(shm_fd);

    // TODO: Add lock semaphore here
    // TODO: OR USE A FIFO!
    // Initialize shared memory manager.
    shm_mgr_t *sm = (shm_mgr_t *) shm_addr;
    sm->sb_count = shared_buff_count;
    sm->buffer_idx = 0; // Currently unused.
    // TODO: Unlock sem

    // Allocate space for thread pool.
    tp = calloc(sm->sb_count, sizeof(pthread_t));
    if (tp == NULL) {
        perror("calloc");
        goto ExitFail;
    }

    // Create thread pool. One thread per shared buffer.
    for (size_t i = 0; i < sm->sb_count; i++) {
        int ret = pthread_create(&tp[i], NULL, shm_worker_thread, (void *)i);
        if (ret != 0) {
            print_error("Problem creating a thread.");
            goto ExitFail;
        }
    }

    dbg_print("All threads created");
    dbg_print("Initialize squeue");
    // Initilize our sentence queue.
    sq = squeue_init();
    if (sq == NULL) {
        fprintf(stderr, "[!] Could not create sentence queue!\n");
        goto ExitFail;
    }


    // Process input file one line at a time.
    while(fgets(line, sizeof(line), input_file) != NULL) {
        // Strip off new line
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        printf("%s\n", line);


        if(!squeue_enqueue(sq, line)) {
            fprintf(stderr, "[!] Failed to add line to queue!\n");
        }

        // Clear line buffer for new sentence.
        memset(line, '\0', sizeof(line));

    }

    printf("[!] Done processing file!\n");

    // Check for any errors while processing file stream.
    if (ferror(input_file)) {
        print_error("Error on input_file.");
    }

    if (!feof(input_file)) {
        print_error("Unable to process entire file.");
    }
    clearerr(input_file);


    // Join all created threads.
    for (size_t i = 0; i < sm->sb_count; i++) {
        pthread_join(tp[i], NULL);
    }
    dbg_print("threads all finished");

    free(tp);
    tp = NULL;

    squeue_destroy(sq);
    sq = NULL;

    shm_unlink(SHM_MGR_NAME);
    fclose(input_file);


    if (munmap(shm_addr, sizeof(shm_mgr_t)) == -1) {
        perror("munmap");
        shm_addr = 0;
        goto ExitFail;
    }

    return EXIT_SUCCESS;

ExitFail:
    if (sq) squeue_destroy(sq);
    if (shm_fd) shm_unlink(SHM_MGR_NAME);
    if (input_file) fclose(input_file);
    if (tp) free(tp);
    if (shm_addr) munmap(shm_addr, sizeof(shm_mgr_t));

    return EXIT_FAILURE;


}
