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

// Size of one individual shared buffer.
#define SHARED_BUFFER_SIZE 1024
// Handle to shmem. Entry will be listed in /dev/shmem.
#define SHARED_MEM_NAME    "/crowdstrike-shmem"
// Maximum number of shared buffers allowed (size of each buffer is 1KiB)
#define SHARED_MAX_BUFFERS 16
// Mutex for synchronizing producer/consumer buffer access.
#define SEM_MUTEX_NAME "/crowdstrike-sem"

// Doing some envelope calculations and research your average sentence is around
// 75-100 characters long. I chose a max sentence length of 256 which would allow us
// to fill one shared buffer with roughly 4 sentences.
#define MAX_SENTENCE_LENGTH 256

/* Holds a sentence which can be placed in a shared buffer. */
typedef struct sentence_t {
    bool processed;                // If set, the consumer has processed the sentence.
    uint16_t size_total;           // Total size of this structure including the flex array member.
    unsigned long sentence_length; // Length of sentence.

    char sentence[];               // Flex array member with actual sentence data. Max length is 512
                                   // bytes but we don't want to allocate what we don't need.
} sentence_t;

/* Structure to manage shared buffers. */
typedef struct shared_mem_t {
   uint32_t sh_buffer_count;          // The number of shared buffers (supplied by user).
   uint32_t buffer_idx;               // Buffer currently being accessed.
   sentence_t sh_sentence_buffers[];  // Stores our shared buffers.
} shared_mem_t;

#endif // __CPCOMMON_H
