/*
 *
 *
 */
#ifndef __SQUEUE_H
#define __SQUEUE_H

#include <stdbool.h>
#include <pthread.h>

#include "cpcommon.h"


// sqnode_t are primary node that is added or removed
// from the queue.
typedef struct sqnode_t {
    // For simplicity a node will contain the sentence
    // data inline.
    char sentence[MAX_SENTENCE_LENGTH+1];
    struct sqnode_t *next;
} sqnode_t;


// squeue_t is a simple concurrency safe FIFO queue with rather
// coarse grain locking. This can be improved upon.
typedef struct squeue_t {
    sqnode_t *back;
    sqnode_t *front;

    // Number of entries currently in queue.
    size_t entry_count;

    bool is_empty;

    // This flag, when set by the producer, indicate that
    // processing is finished and threads should clean up.
    bool finished;

    // Out mtx to make the queue concurrency safe.
    pthread_mutex_t *lock;
} squeue_t;



// Return a new sentence node to store sentence and add to work queue.
sqnode_t * new_sqnode(char *sentence_str);

// Initilize our sentence queue.
squeue_t * squeue_init(void);

// Enqueue a sentence node.
bool squeue_enqueue(squeue_t *q, char *sentence_str);

// Dequeue a sentence, placing it in sentence_t variable first
// for placement in a shared memory buffer.
bool squeue_dequeue(squeue_t *q, char *sentence_buff);

// Return number of elements on the queue.
size_t squeue_count(const squeue_t *q);

// Remove squeue and free associated memory.
void squeue_destroy(squeue_t *q);

// Set finished field to signal to consumers no more
// entries will be added to the queue.
void squeue_setfinished(squeue_t *q);

// Returns true if the finished flag is set. This means no
// more data will be placed on queue by main thread.
bool squeue_done(const squeue_t *q);

#endif
