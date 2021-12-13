#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>

#include "cpcommon.h"
#include "dbg.h"

// Used for debugging.
void
print_error(const char *err_msg)
{
    assert(err_msg != NULL);
    // ANSI Color macros are in dbg.h
    fprintf(stderr, RED "[!] ERROR:" RESET " %s\n", err_msg);
    return;
}

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


// Hex dump a buffer of data. Will use this to debug if I need to see
// contents of shared buffer.
void hex_dump(const uint8_t *data, size_t size)
{
    const size_t kBytesPerLine = 16;
    printf(GREEN "===================== HEXDUMP =====================\n" RESET);
    for (size_t i = 0; i < size; i += kBytesPerLine) {
        printf(YELLOW "%03zX: " RESET, i);
        for (size_t j = 0; j < kBytesPerLine; j++) {
            printf("%02X ", data[i + j]);
        }
        puts("");
    }
    printf(GREEN "======================= END =======================\n" RESET);
}
