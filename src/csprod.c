/*
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>

#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "cpcommon.h"
#include "dbg.h"




// Show usage of command.
static void print_usage(const char *prog_name)
{
    assert(prog_name != NULL);

    fprintf(stderr, GREEN "\n==== Csprod ====" RESET "\n\n");
    fprintf(stderr, YELLOW "Description: " RESET  " Read file line by line and pass sentences to a consumer via shared buffers.\n");
    fprintf(stderr, YELLOW "Usage:       "   RESET  " %s <SHARED_BUFFER_COUNT> <FILE>\n", prog_name);

    return;
}


// Print error to user via stderr.
// TODO: Make more robust to handle variadic options.
static void print_error(const char *err_msg)
{
    assert(err_msg != NULL);

    va_list arg;
    va_start(arg, err_msg);

    // ANSI Color macros are in dbg.h
    fprintf(stderr, RED "[!] ERROR:" RESET " %s\n", err_msg);

    return;
}


int main(int argc, char **argv) {

    FILE *input_file = NULL;
    unsigned long int shared_buff_count = 0;
    char *bad_char = NULL;

    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }


    // Check buffer count is actually a number.
    shared_buff_count = strtoul(argv[1], &bad_char, 10);
    if (shared_buff_count == 0 || *bad_char != '\0') {
        print_error("Invalid value for <SHARED_BUFFER_COUNT>");
        return EXIT_FAILURE;
    }

    // Ensure shared buffer count is within our range (1-16 inclusive).
    if (shared_buff_count > SHARED_MAX_BUFFERS) {
        print_error("Buffer count out of range, must be a value of 1-16, inclusive.");
        return EXIT_FAILURE;
    }


    // Open input file.
    if ((input_file = fopen(argv[2], "r")) == NULL) {
        print_error("Could not open input file");
        return EXIT_FAILURE;
    }


    // TODO: Make sure file isn't empty.
    // TODO: Setup signal handler.

    shm_mgr_t *shm_mgr = NULL;
    int shm_fd = 0;

    // Get shm_fd for shm_mgr. This object keeps some book-keeping about shared buffers.
    shm_fd = shm_open(SHM_MGR_NAME, O_RDWR | O_CREAT | O_EXCL);
    if (shm_fd === -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    dbg_print("Shared memory for shm_mgr opened successfully");

    // Set shm_mgr shared memory region size.
    if (ftruncate(shm_fd, sizeof(shm_mgr_t) == -1)) {
        perror("ftruncate");
        return EXIT_FAILURE;
    }


    dbg_print("Resized shm_fd region.");
    dbg_print("Time to mmap()");

    void *shm_addr = mmap(NULL, sizeof(shm_mgr_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }



    return 0;
}
