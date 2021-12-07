/*
 * File       : cpcommon.h
 * Description: Header for definitions common to producer.c and consumer.c
 * Author     : J. DeFrancesco
 */

#ifndef __CPCOMMON_H
#define __CPCOMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <unistd.h>
#include <pthread.h>

// Size of one individual shared buffer.
#define SHARED_BUFFER_SIZE 1024
// Maximum number of shared buffers allowed (size of each buffer is 1KiB)
#define SHARED_MAX_BUFFERS 16
// Mutex for synchronizing producer/consumer buffer access.
#define SEM_MUTEX_NAME "/crowdstrike-sem"


// Doing a few "back of the envelope" calculations and research, your average sentence is around
// 75-100 characters long. I chose 247 as the max length of a sentence. This was chosen so the
// total size of a sentence_t will be 256 bytes maximum given sentence_length header
// is going to be 8 bytes: 256 (max size of struct) - 8 (sentence length preceding data) - 1 (nul)
// byte that delimits the string. This means one shared buffer can hold 4 sentences that are
// max length.
#define MAX_SENTENCE_LENGTH 247

/* Holds a sentence which can be placed in a shared buffer. */
typedef struct sentence_t {
    unsigned long sentence_length; // Length of sentence (Maximum size of 256)

    char sentence[];               // Flex array member with actual sentence data. Max length is 255.
                                   // bytes but we don't want to allocate what we don't need.
} sentence_t;


// Name for shmem_mgr_t shm needed
#define SHM_MGR_NAME "/cs-shmgr"
// Name we will use for sem mutex.
#define SHM_MGR_MTX "/cs-shmgr-mtx"

/* Structure to manage shared buffers. */
typedef struct shm_mgr_t {
   uint32_t sb_count;          // The number of shared buffers (supplied by user).
   uint32_t buffer_idx;               // Buffer currently being accessed.
   sentence_t *sh_sentence_buffers[SHARED_MAX_BUFFERS];  // Stores our shared buffers.
} shm_mgr_t;


/* Enum for shared memory permissions. */
typedef enum {
    READ_MEMORY,
    WRITE_MEMORY,
    WR_MEMORY,
} access_t;





/* Enum representing state a thread may be in. */
typedef enum {
    TS_ALIVE,
    TS_TERMINATED, // Thread terminated but not yet joined.
    TS_FINISHED,   // Thread that has terminated, and has joined.
} thread_state_t;

/* Thread pool object to manage worker threads */
typedef struct thread_pool_t {
    pthread_t tid;
    thread_state_t state;
} thread_pool_t;


// Creates a shared memory buffer via call to mmap identified
// by our POSIX shm file descriptor, our IPC mechanism of choice.
void * create_shared_buffer(int shm_fd, size_t buff_size, access_t mem_type);

#endif // __CPCOMMON_H
