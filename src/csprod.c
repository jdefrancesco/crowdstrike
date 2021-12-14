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



// Our sentence queue.
static squeue_t *sq = NULL;


// Prototypes
void signal_handler(int sig);
static void print_usage(const char *prog_name);
static void *shm_worker_thread(void *arg);


int main(int argc, char **argv) {

    FILE *input_file = NULL;
    unsigned long int shared_buff_count = 0;
    char *bad_char = NULL;
    int shm_fd = 0;
    void *shm_addr = NULL;

    // tp references the little thread pool we create.
    pthread_t *tp = NULL;

    // Will store the line we read from the file.
    char line[MAX_LINE_SIZE] = {0};

    // Mutex semaphore for sharing the shm_mgr_t struct betweeen processes.
    sem_t *sem_mtx = NULL;

    // Make sure stdio is line buffered only up to one line.
    setvbuf(stdout, NULL, _IOLBF, 0);


    // Signal handler.
    struct sigaction sa = {
        .sa_handler = signal_handler,
        .sa_flags = SA_RESTART,
    };

    // Setup signal handler.
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        goto ExitFail;
    }


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


    // Create smeaphore mutex, we will not hold this at the start.
    // We will wait for the consumer.
    if ((sem_mtx = sem_open(SHM_MGR_MTX, O_CREAT,
                    0666, 0)) == SEM_FAILED) {
        print_error("Error opening semaphore mutex SHM_MGR_MTX");
        goto ExitFail;
    }

    // Get shm_fd for shm_mgr. This object keeps some book-keeping about shared buffers.
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            // TODO: We need root to cleanup shm. Check uid and let user know.
            print_error("Shm file object already exists. Cleaning it up to retry.");
            shm_unlink(SHM_MGR_NAME);
        }

        perror("shm_open");
        goto ExitFail;
    }

    // Set shm_mgr shared memory region size.
    if (ftruncate(shm_fd, sizeof(struct shm_mgr_t)) == -1) {
        if ((errno  == EINVAL) || (errno == EBADF)) {
            fprintf(stderr, "[!] ftruncate fd not open for writing\n");
        }
        perror("ftruncate");
        goto ExitFail;
    }

    // The consumer will map this as well to know how many buffers will be shared.
    shm_mgr_t *sm = (shm_mgr_t *)mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE,
            MAP_SHARED, shm_fd, 0);
    if (sm == MAP_FAILED) {
        perror("mmap");
        goto ExitFail;
    }


    dbg_print("waiting for semaphore");
    // Initialize semaphore as process shared, value 0.
    if (sem_wait(sem_mtx) == -1) {
        perror("sem_init");
        goto ExitFail;
    }
    dbg_print("done waiting csprod...");

    sm->sb_count = shared_buff_count;
    sm->buffer_idx = 0; // Currently unused.
    sm->consumer_proc_ready = false;

    // Let corresponding process know this data can be safely read.
    if (sem_post(sem_mtx) == -1) {
        perror("sem_post");
        goto ExitFail;
    }

    dbg_print("sem posted...");


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


    // Initilize our sentence queue.
    sq = squeue_init();
    if (sq == NULL) {
        fprintf(stderr, "[!] Could not create sentence queue!\n");
        goto ExitFail;
    }

    // Process input file one line at a time.
    while(fgets(line, sizeof(line), input_file) != NULL) {
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        printf(YELLOW "%s\n" RESET, line);

        if(!squeue_enqueue(sq, line)) {
            fprintf(stderr, "[!] Failed to add line to queue!\n");
        }

        // Clear line buffer for new sentence.
        memset(line, '\0', sizeof(line));
    }

    // Set finished flag for consumer threads to check.
    squeue_setfinished(sq);
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

    free(tp);
    tp = NULL;

    squeue_destroy(sq);
    sq = NULL;

    shm_unlink(SHM_MGR_NAME);
    fclose(input_file);

    close(shm_fd);
    if (munmap(shm_addr, sizeof(shm_mgr_t)) == -1) {
        perror("munmap");
        shm_addr = 0;
        goto ExitFail;
    }

    puts("csprod goodbye :-)\n");
    return EXIT_SUCCESS;

ExitFail:
    if (sq) squeue_destroy(sq);
    if (shm_fd) shm_unlink(SHM_MGR_NAME);
    if (input_file) fclose(input_file);
    if (tp) free(tp);
    if (shm_addr) munmap(shm_addr, sizeof(shm_mgr_t));

    return EXIT_FAILURE;


}

static void *
shm_worker_thread(void *arg) {

    size_t i = (size_t) arg;
    int shm_fd = 0;
    char shm_name[256] = {0};

    void *shm_addr = NULL;
    uint8_t * shm_buff = NULL;
    size_t shm_bytes_avail = SHARED_BUFFER_SIZE;

    // Semaphore mutex name used between two processes.
    char sem_mtx_name[256] = {0};
    sem_t *sem_mtx = NULL;
    bool holding_sem_mtx = false;

    // We dequeue a line from the queue into this temp buffer.
    char temp_line[MAX_LINE_SIZE] = {0};

    sentence_t *s = NULL;

    // Construct sem mutex name.
    snprintf(sem_mtx_name, (sizeof(sem_mtx_name)-1), SEM_MTX_THREAD "%zu", i);
    // Construct shm name. Note, this won't show in the file system
    snprintf(shm_name, (sizeof(shm_name)-1), SHM_THREAD_NAME "%zu", i);

    // Create sem mtx we use for sync. between different processes.
    if ((sem_mtx = sem_open(sem_mtx_name, O_CREAT, 0666, 1))
            == SEM_FAILED){
        perror("sem_open");
        goto Exit;
    }

    // We start off holding the sem.
    holding_sem_mtx = true;
    if(shm_unlink(shm_name) == -1) {
        // That is fine, we don't want an entry.
        if (errno == ENOENT) {
           fprintf(stderr, "[!] No shm entry, creating a new one.\n");
        }
    }

    shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("Shm file object already exists. Cleaning it up to retry.");
            shm_unlink(shm_name);
        }

        perror("shm_open");
        goto Exit;
    }

    // Resize our shared memory region.
    if(ftruncate(shm_fd, (off_t)SHARED_BUFFER_SIZE) == -1) {
        perror("ftruncate");
        goto Exit;
    }


    // mmap() our shared buffer.
    shm_addr = create_shared_buffer(shm_fd, SHARED_BUFFER_SIZE);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        goto Exit;
    }

    shm_buff = (uint8_t *) shm_addr;
    memset(shm_buff, 0x0, SHARED_BUFFER_SIZE);

    // This loop takes strings off the queue, and attempts to place them
    // into a finite size buffer of size 1024, the strings may be of variable
    // length. We can view see this problem as a special case of bin packing.
    // Bin-packing is NP-Complete so heuristics are our best tool for obtaining
    // effciency. This is complicated by the fact we are implementing an "on-line"
    // solution. We don't have a global view of everything before hand. If we did,
    // we could use some variant of "first-fit decreasing" heuristic. This isn't
    // the case for us.
    while (true) {
        // NOTE: We enter loop holding the semaphore

        // Try to dequeue a sentence/line from main thread.
        if (!squeue_dequeue(sq, temp_line)) {
            if (squeue_done(sq)) {
                break;
            }
            continue;
        }


        // If we aren't currently holding the semaphore, we wait on other process to finish
        // up the work it needs to do on shared buffer before we have control again.
        if (!holding_sem_mtx) {
            dbg_print("calling sem_wait() to get buffer sem.");
            if(sem_wait(sem_mtx) == - 1) {
                perror("sem_wait");
                break;
            }
            dbg_print("(csprod) producer thread gained access to buffer again\n");
            holding_sem_mtx = true;
            // Rewind to beginning of buffer.
            /* shm_buff = (uint8_t *)shm_addr; */
            // Means we have control of buffer.
            if (*shm_buff == '\0') {
                // First byte of buffer is nul, we consider the buffer processed.
                printf("[+] Consumer process has unpacked and the sentence buffer.\n");
            } else {
                // First byte isn't nul
                printf("[+] Consumer process may not have fully handled the sentence buffer\n");
            }
            // Clear buffer to start clean and reset shared buffer bytes available to max (1024).
            shm_bytes_avail = SHARED_BUFFER_SIZE;
            memset(shm_buff, 0x0, SHARED_BUFFER_SIZE);
        }


        if ((strlen(temp_line) > MAX_SENTENCE_LENGTH) ||
                (strlen(temp_line) == 0)) {
            fprintf(stderr, "[+] Line from queue exceeds maximum "
                    "sentence length or is zero. Dropping. length = %zu\n", strlen(temp_line));
            memset(temp_line, 0x0, sizeof(temp_line));
            continue;
        } else {
            // Lets allocate new sentence_t structure to add to shared buffer.
            s = calloc(1, sizeof(sentence_t) + (strlen(temp_line)+1));
            if (s == NULL) {
                fprintf(stderr, "[!] Error allocating sentence_t space.\n");
                break;
            }
        }

        // sentence_length does NOT include the null terminator.
        s->sentence_length = strlen(temp_line);
        strncpy(s->sentence, temp_line, s->sentence_length);
        memset(temp_line, 0x0, sizeof(temp_line));

        // Ensure nul termination.
        s->sentence[s->sentence_length] = '\0';
        // Total number of bytes for sentence_t (+1 for null char)
        size_t s_tb = (sizeof(sentence_t) + s->sentence_length + 1);
        memcpy(shm_buff, (uint8_t *)s, s_tb);
        free(s);

        shm_buff += (uintptr_t) s_tb;
        shm_bytes_avail -= s_tb;


        // If we have less than 256 bytes less. Just release mutex
        // for consumer to process.
        if (shm_bytes_avail < MAX_LINE_SIZE) {
            // For debugging...
            shm_buff = (uint8_t *) shm_addr;
            hex_dump((uint8_t *)shm_buff, SHARED_BUFFER_SIZE);
            dbg_print("release sem");
            if(sem_post(sem_mtx) == -1) {
                perror("sem_post");
                break;
            }
            holding_sem_mtx = false;
        }

    }

    // Debug, check out contents in the shared buffer.
    hex_dump((uint8_t *)shm_addr, SHARED_BUFFER_SIZE);
    // Clean up.
    if (holding_sem_mtx) {
        if(sem_post(sem_mtx) == -1) {
            perror("sem_post");
        }
        holding_sem_mtx = false;
    }

    if (munmap(shm_addr, SHARED_BUFFER_SIZE) == -1) {
        perror("munmap");
        goto Exit;
    }

    return NULL;

Exit:
    if (shm_addr) munmap(shm_addr, SHARED_BUFFER_SIZE);
    shm_unlink(shm_name);
    if (shm_fd) close(shm_fd);
    return NULL;
}

void
signal_handler(int sig)
{
    (void) sig;
    const char * const s = "SIGINT CAUGHT! EXITING!\n";
    write(1, s, strlen(s));

    // Gracefully stop all threads and quit.
}

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

