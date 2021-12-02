#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {

    // ================= SETTING UP SHARED MEMORY ======================

    // Check arguments are valid
    //   i. N - Number of shared buffers
    //   2. file name - name of file to read
    //

    // Make sure file we need to read exists.
    // Also ensure it contains data.

    // Init IPC SHMEM constructs we need.
    //  1. make semaphore
    //  2. call shm_open to get shared mem
    //  3. ftruncate to set size of shared memory needed
    //

    // Call mmap with RW and SHARED
    //

    // Init shared mememory

    // setup buffer counting sem that shows num of available buffs. Initial value = MAX_BUFFS
    //

    // init complete set mutex to 1 indicated mem segs is available.

    // ================= END INIT OF SHARED MEM IPC =====================


    // ======= FILE PROCESSING ========
    // while read sentence from file:
    //
    //      put sentence in a shared buff if the space exists.
    //      if space is left over, see if next sentence can fit to fill buffer
    //      before allowing it to be read.
    //
    //      Print sentence to term

    printf("producer!\n");
    return 0;
}
