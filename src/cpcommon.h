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
// Name template for creating semaphore used between threads
// from different processes.
#define SEM_MTX_THREAD "/cs-sem-"

// Doing a few "back of the envelope" calculations and research, your average sentence is around
// 75-100 characters long. I chose 247 as the max length of a sentence. This was chosen so the
// total size of a sentence_t will be 256 bytes maximum given sentence_length header
// is going to be 8 bytes: 256 (max size of struct) - 8 (sentence length preceding data) - 1 (nul)
// byte that delimits the string. This means one shared buffer can hold 4 sentences that are
// max length. Note, we keep null delimiter as it will still act as a sentence seperator;
// this is so we can call our strlen() and make sure what we compute matches the header.
#define MAX_SENTENCE_LENGTH 247

// Maximum size of a line we could potentially read out of a file.
// This is less than what the sentence length can be.
#define MAX_LINE_SIZE 256

/* Holds a sentence which can be placed in a shared buffer. */
typedef struct sentence_t {
    unsigned long sentence_length; // Length of sentence (Maximum size of 247)

    char sentence[];               // Flex array member with actual sentence data. Max length is 247.
                                   // bytes but we don't want to allocate what we don't need.
} sentence_t;


// Name for shmem_mgr_t shm needed
#define SHM_MGR_NAME "/cs-shmgr"
// Name we will use for sem mutex.
#define SHM_MGR_MTX "/cs-shmgr-mtx"

/* Structure to manage shared buffers. */
typedef struct shm_mgr_t {
   size_t sb_count;          // The number of shared buffers (supplied by user).
   size_t buffer_idx;        // Buffer currently being accessed.
} shm_mgr_t;


/* Enum for shared memory permissions. */
typedef enum {
    READ_MEMORY,
    WRITE_MEMORY,
    WR_MEMORY,
} access_t;



#define SHM_THREAD_NAME "/cs-thrd-"



// Creates a shared memory buffer via call to mmap identified
// by our POSIX shm file descriptor, our IPC mechanism of choice.
void * create_shared_buffer(int shm_fd, size_t buff_size);

// Hex dump a buffer of data. Will use this to debug if I need to see
// contents of shared buffer.
void hex_dump(const uint8_t *data, size_t size);

#endif // __CPCOMMON_H
