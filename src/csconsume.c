/*
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "cpcommon.h"
#include "dbg.h"

static void * shm_worker_thread(void *arg);
static bool process_buffer(uint8_t *buff);
static bool valid_ascii(uint8_t *buff);


int main(int argc, char **argv) {

    unsigned long int shared_buff_count = 0;
    sem_t *sem_mtx = NULL;
    int shm_fd = 0;
    char *bad_char = 0;
    void *shm_addr = NULL;
    shm_mgr_t *sm = NULL;

    // tp references the little thread pool we create.
    pthread_t *tp = NULL;

    if (argc < 3) {
        fprintf(stderr, "Usage: ./csconsumer <SHARED_BUFFER_COUNT> <SUBSTRING_TO_SEARCH>\n");
        return EXIT_FAILURE;
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


    // Mtx we obtain to sync with producer.
    if ((sem_mtx = sem_open(SHM_MGR_MTX, O_CREAT, 0666, 1))
            == SEM_FAILED) {
        perror("sem_open");
        goto ExitFail;
    }

    // Get access to shared manager data struct.
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("Shm file object already exists. Cleaning it up. Please rerun the program.");
            shm_unlink(SHM_MGR_NAME);
        }
        perror("shm_open");
        goto ExitFail;
    }

    // Map in shm_mgr_t....
    sm = (shm_mgr_t *) mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE,
            MAP_SHARED, shm_fd, 0);
    if (sm == MAP_FAILED) {
        dbg_print("mmmap error");
        perror("mmap");
        goto ExitFail;
    }


    // Let producer know we are ready, they can fill shm_mgr_t struct.
    if (sem_post(sem_mtx) == -1) {
        perror("sem_post");
        goto ExitFail;
    }

    printf("[+] Activating producer..\n");

    if (sem_wait(sem_mtx) == -1) {
        perror("sem_wait");
        goto ExitFail;
    }

    dbg_print("consumer ready to validate producers shm_mgr_t data");

    sm = (shm_mgr_t *) shm_addr;
    // Make sure the producer process and consumer process use the same number
    // of shared buffers for information exchange.
    if (sm->sb_count != shared_buff_count) {
        fprintf(stderr, "Producer/Consumer processes were not given the same "
                "shared buffer count value.\n Producer: %zu, Consumer: %zu\n",
                sm->sb_count,
                shared_buff_count);
        goto ExitFail;
    }
    printf("[+] Producer and consumer processes agreed on the same buffer count\n");

    if (sem_post(sem_mtx) == -1) {
        perror("sem_post");
        goto ExitFail;
    }


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


    printf("[+] Waiting for threads to finish...\n");

    for (size_t i = 0; i < shared_buff_count; i++) {
        pthread_join(tp[i], NULL);
    }

    printf("[+] Finished....\n");


    shm_unlink(SHM_MGR_NAME);

    free(tp);
    tp = NULL;

    close(shm_fd);

    if (munmap(shm_addr, sizeof(shm_mgr_t)) == -1) {
        perror("munmap");
        shm_addr = 0;
        goto ExitFail;
    }


    return EXIT_SUCCESS;

ExitFail:
    if (shm_fd) shm_unlink(SHM_MGR_NAME);
    if (shm_addr) munmap(shm_addr, sizeof(shm_mgr_t));
    return EXIT_FAILURE;
}



// Worker thread that consumes what the corresponding thread of
// csprod creates. Additionally it must validate the shared buffer contents
// and determine if it contains the sub-string we are looking for.
static void *
shm_worker_thread(void *arg) {

    size_t i = (size_t) arg;
    int shm_fd = 0;
    char shm_name[256] = {0};

    void *shm_addr = NULL;
    uint8_t * shm_buff = NULL;
    size_t shm_bytes_avail = SHARED_BUFFER_SIZE;

    // Semaphore mutex name used between two corresponding thread workers.
    char sem_mtx_name[256] = {0};
    sem_t *sem_mtx = NULL;
    bool holding_sem_mtx = false;

    sentence_t *s = NULL;

    // We copy contents from shared buffer here before we start doing work.
    // This lets us relinquish the semaphore so the producer can keep going.
    uint8_t active_buffer[SHARED_BUFFER_SIZE] = {0};

    // Construct sem mutex and shm names for communicating between processes.
    snprintf(sem_mtx_name, (sizeof(sem_mtx_name)-1), SEM_MTX_THREAD "%zu", i);
    snprintf(shm_name, (sizeof(shm_name)-1), SHM_THREAD_NAME "%zu", i);

    // The producer thread will have sem_mtx when we start  off.
    if ((sem_mtx = sem_open(sem_mtx_name, O_CREAT, 0666, 0))
            == SEM_FAILED) {
        perror("sem_open");
        goto ExitErr;
    }

    // Acquire the shared memory buffer which will contain data we
    // pass back and forth.
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        goto ExitErr;
    }

    shm_addr = create_shared_buffer(shm_fd, SHARED_BUFFER_SIZE);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        goto ExitErr;
    }
    shm_buff = (uint8_t *) shm_addr;


    while (true) {
        if (!holding_sem_mtx) {
            if (sem_wait(sem_mtx) == -1) {
                perror("sem_wait");
                break;
            }
            holding_sem_mtx = true;
        }

        // Get data in our processing buffer so we can relinquish the semaphore.
        memcpy(active_buffer, shm_buff, SHARED_BUFFER_SIZE);
        // Clearing the entire buffer indicates we have "processed" the data.
        memset(shm_buff, 0x0, SHARED_BUFFER_SIZE);

        // Give control back to producer while we validate things.
        if (sem_post(sem_mtx) == -1) {
            perror("sem_post");
            goto ExitErr;
        }

        // Buffer should be filled, lets copy it out so
        // we can give access back to producer

    }


    return NULL;

ExitErr:

    return NULL;

}
