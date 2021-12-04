/*
 * File       : cpcommon.h
 * Description: Header for definitions common to producer.c and consumer.c
 * Author     : J. DeFrancesco
 */

#ifndef __CPCOMMON_H
#define __CPCOMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

// Size of one individual shared buffer.
#define SHARED_BUFFER_SIZE 1024
// Handle to shmem. Entry will be listed in /dev/shmem.
#define SHARED_MEM_NAME    "/crowdstrike-shmem"
// Maximum number of shared buffers allowed (size of each buffer is 1KiB)
#define SHARED_MAX_BUFFERS 16
// Mutex for synchronizing producer/consumer buffer access.
#define SEM_MUTEX_NAME "/crowdstrike-sem"

// Doing a few "back of the envelope" calculations and research, your average sentence is around
// 75-100 characters long. I chose a max sentence length of 256 which would allow us
// to fill one shared buffer with roughly 4 sentences.
#define MAX_SENTENCE_LENGTH 256

/* Holds a sentence which can be placed in a shared buffer. */
typedef struct sentence_t {
    bool processed;                // If set, the consumer has processed the sentence.

    uint16_t size_total;           // Total size of this structure including the flex array member.
                                   // This should not exceed (MAX_SENTENCE_LENGTH + sizeof(bool) + sizeof(uint16_t)
                                   // + sizeof(unsigned long))

    unsigned long sentence_length; // Length of sentence (Maximum size of 256)

    char sentence[];               // Flex array member with actual sentence data. Max length is 256.
                                   // bytes but we don't want to allocate what we don't need.
} sentence_t;

/* Structure to manage shared buffers. */
typedef struct shared_mem_t {
   uint32_t sh_buffer_count;          // The number of shared buffers (supplied by user).
   uint32_t buffer_idx;               // Buffer currently being accessed.
   sentence_t sh_sentence_buffers[];  // Stores our shared buffers.
} shared_mem_t;


/* Enum for shared memory permissions. */
typedef enum {
    READ_MEMORY,
    WRITE_MEMORY,
    WR_MEMORY,
} access_t;

// Function:    create_shared_buffer
// Descriptiom: Creates a shared memory buffer via call to mmap identified
//              by our POSIX shm file descriptor, our IPC mechanism of choice.
void * create_shared_buffer(int shm_fd, size_t buff_size, access_t mem_type);

#endif // __CPCOMMON_H
