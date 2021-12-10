#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>

#include "cpcommon.h"


// Creates a shared memory buffer via call to mmap identified
// by our POSIX shm file descriptor, our IPC mechanism of choice.
void * create_shared_buffer(int shm_fd, size_t buff_size)
{
    assert(shm_fd != -1);

    // Specification states shared buffer should have fixed size of 1024.
    if (buff_size != SHARED_BUFFER_SIZE) {
        fprintf(stderr, "[!] Maximum buffer size allowed is %zu. Correcting size and resuming.",
                (size_t) SHARED_BUFFER_SIZE);
        buff_size = SHARED_BUFFER_SIZE;
    }

    // Set memory protection. Producer will have read/write access, consumer only needs to read.
    return mmap(NULL, buff_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, shm_fd, 0);
}


