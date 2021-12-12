#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "squeue.h"
#include "cpcommon.h"
#include "dbg.h"

// Make queue thread-safe.
static pthread_mutex_t queue_lock;



// Create new sentence/line node.
sqnode_t * new_sqnode(char *sentence_str)
{
    size_t s_len = strlen(sentence_str);
    if (s_len > MAX_SENTENCE_LENGTH) {
        fprintf(stderr, "[!] String of size %zu exceeds maximum.\n",
                strlen(sentence_str));
        return NULL;
    }

    fprintf(stderr, CYAN "[new_sqnode] s_len = %zu\n" RESET, s_len);
    /* if (sentence_str[s_len] != '\0') { */
    /*     sentence_str[s_len] = '\0'; */
    /*     dbg_print("Added missing nul delimiter in sentence_str\n"); */
    /* } */

    sqnode_t *node = calloc(1, sizeof(sqnode_t));
    strncpy(node->sentence, sentence_str, MAX_SENTENCE_LENGTH);
    // Make sure we add our null delimiter.
    node->sentence[MAX_SENTENCE_LENGTH] = '\0';
    return node;
}



// Create squeue, our sentence queue.
squeue_t * squeue_init(void)
{
    squeue_t *q = calloc(1, sizeof(squeue_t));
    if (!q) {
        fprintf(stderr, "[!] Error initializing queue.\n");
        return NULL;
    }
    // Initialize mutex that will guard out queue.
    pthread_mutex_init(&queue_lock, NULL);
    q->lock = &queue_lock;

    // Set other queue fields.
    q->front = NULL;
    q->back = NULL;
    q->is_empty = true;
    q->entry_count = 0;
    q->finished = false;

    return q;
}



// Add sentence node to the back of the queue.
bool squeue_enqueue(squeue_t *q, char *sentence_str)
{
    sqnode_t *tmp_node = new_sqnode(sentence_str);
    if (!tmp_node) {
        fprintf(stderr, "[!] Error creating new sentence node.\n");
        return false;
    }

    /* strncpy(tmp_node->sentence, sentence_str, MAX_SENTENCE_LENGTH-1); */
    /* // Append null delimiter if one is not found. */
    /* if (tmp_node->sentence[MAX_SENTENCE_LENGTH] != '\0') { */
    /*     tmp_node->sentence[MAX_SENTENCE_LENGTH] = '\0'; */
    /* } */

    // Entering critical section.
    pthread_mutex_lock(q->lock);
    // Check is queue is empty. Both front/back pointers now point to it.
    if (q->back == NULL) {
        q->front = q->back = tmp_node;
        q->is_empty = false;
        q->entry_count++;
    } else {
        assert(q->is_empty != true);
        // Add new item to back of the queue.
        q->back->next = tmp_node;
        q->back = tmp_node;
        q->entry_count++;
    }

    fprintf(stderr, BLUE "[SQUEUE ENQUEUE TMP NODE DEBUG]" RESET);
    fprintf(stderr, "[squeue_enqueue] tmp->sentence = %s\n", tmp_node->sentence);
    fprintf(stderr, "[squeue_enqueue] tmp->next = %p\n", tmp_node->next);
    pthread_mutex_unlock(q->lock);
    // End critical section.

    return true;
}



// Remove an element from the queue and place it in sentence_buff.
bool squeue_dequeue(squeue_t *q, char *sentence_buff)
{
    // Enter critical section.
    pthread_mutex_lock(q->lock);
    if (q->front == NULL) {
        assert(q->is_empty == true);
        // Unlock before we return.
        pthread_mutex_unlock(q->lock);
        goto ExitQueueEmpty;
    }

    sqnode_t *tmp_node = q->front;

    q->front = q->front->next;
    if (q->front == NULL) {
        q->back = NULL;
    }
    q->entry_count--;
    if (q->entry_count == 0) {
        q->is_empty = true;
    }
    pthread_mutex_unlock(q->lock);
    // End of critical section.

    // Fill sentence_t with the data.
    tmp_node->next = NULL;
    size_t len = strlen(tmp_node->sentence);

    fprintf(stderr, RED "[squeue_dequeue] line: %s, length = %zu\n" RESET, tmp_node->sentence, len);

    if (len > MAX_SENTENCE_LENGTH) {
        fprintf(stderr, RED "[WARNING]" RESET " Sentence length too large. Truncating.\n");
        len = MAX_SENTENCE_LENGTH;
    } else {
        fprintf(stderr, GREEN "[WARNING] Copying sentence to sentence_buff\n" RESET);
        strncpy(sentence_buff, tmp_node->sentence, MAX_LINE_SIZE);

        fprintf(stderr, "tmp_node->sentence = %s\n", tmp_node->sentence);
        fprintf(stderr, YELLOW "sentence_buff: %s\n" RESET, sentence_buff);
        /* sentence_buff[strlen(sentence_buff)] = '\0'; */
    }

    free(tmp_node);
    return true;

ExitQueueEmpty:
    return false;
}



// Return number of elements in the queue.
size_t squeue_count(const squeue_t *q)
{
    pthread_mutex_lock(q->lock);
    size_t t_entry_count = q->entry_count;
    pthread_mutex_unlock(q->lock);

    return t_entry_count;
}



// Set finished queue field member as a signal to consuming
// threads that nothing more will go onto queue.
void squeue_setfinished(squeue_t *q)
{
    pthread_mutex_lock(q->lock);
    q->finished = true;
    pthread_mutex_unlock(q->lock);
}



// Check if the queue has retired.
bool squeue_done(const squeue_t *q)
{
    bool finished = false;
    pthread_mutex_lock(q->lock);
    finished = q->finished;
    pthread_mutex_unlock(q->lock);

    return finished;
}



// Destructor for queue.
void squeue_destroy(squeue_t *q)
{
    pthread_mutex_lock(q->lock);
    if (q->front != NULL || q->back != NULL) {
        fprintf(stderr, RED "[FATAL]:" RESET " Queue still currently holds data!\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(q->lock);

    // Free any other allocated memory.
    pthread_mutex_destroy(q->lock);
    free(q->front);
    free(q->back);
    free(q);
    return;

}
