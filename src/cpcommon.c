#include <assert.h>
#include <sys/mman.h>

#include "cpcommon.h"


// Creates a shared memory buffer via call to mmap identified
// by our POSIX shm file descriptor, our IPC mechanism of choice.
void * create_shared_buffer(int shm_fd, size_t buff_size, access_t mem_type)
{
    assert(shm_fd != -1);

    // Specification states shared buffer should have fixed size of 1024.
    if (buff_size != SHARED_BUFFER_SIZE) {
        fprintf(stderr, "[!] Maximum buffer size allowed is %zu. Correcting size and resuming.",
                SHARED_BUFFER_SIZE);
        buff_size = SHARED_BUFFER_SIZE;
    }

    // Set memory protection. Producer will have read/write access, consumer only needs to read.
    int mem_protection = (mem_type == WR_MEMORY) ? (PROT_WRITE | PROT_WRITE) : PROT_READ;
    return mmap(NULL, buff_size, mem_protection, MAP_SHARED, shm_fd, 0);
}


