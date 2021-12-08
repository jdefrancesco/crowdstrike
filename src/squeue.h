/*
 *
 *
 */
#ifndef __SQUEUE_H
#define __SQUEUE_H

#include <pthread.h>
#include "cpcommon.h"


typedef struct sqnode_t {
    // For simplicity we will keep enough space in this
    // data structure, max sentence size isn't too big.
    char sentence[MAX_SENTENCE_LENGTH];
    struct sqnode_t *next;
}

// Thread safe sentence queue that will be used to pass
// worth from producers processing loop to workers in
// thread pool.
typedef struct squeue_t {
    sqnode_t *back;
    sqnode_t *front;

    size_t entry_count;
    bool stop;
} squeue_t;

// Return a new sentence node to store sentence and add to work queue.
snode_t * new_sqnode(void);

// Initilize our sentence queue.
squeue_t init_sentence_queue(void);

// Enqueue a sentence node.
void enqueue_sentence(squeue_t *q, const char *sentence_str);

// Dequeue a sentence, placing it in sentence_t variable first
// for placement in a shared memory buffer.
void dequeue_sentence(squeue_t *q, sentence_t *s);


#endif
