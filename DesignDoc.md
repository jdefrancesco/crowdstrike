# CrowdStrike Producer/Consumer Task Design Document

## Challenge Overview:

* Design a system that consists of two applications; one acts as a producer, the other a consumer.
* The two applications should communicate through N  _shared buffers_, where N can be configured by user.
* Shared buffers have a maximum size of 1024 bytes.
* The producer will read sentences from a file then place it in a shared buffer for the consumer to process.
* The consumer should be able to handle invalid data gracefully and continue processing.
* The producer begins execution first, the consumer second.

This is essentially the "Bounded Buffer Problem", better knowns as "Producer/Consumer Problem". The primary
challenge is for the two programs to efficiently process data and work in harmony. Since this
problem deals with concurrency, we must ensure safe communication between the producer and consumer.
We want to avoid things such as: deadlock situations, race conditions, and inefficient processing.

A primary decision that needs to be made is the method used for IPC. The requirements document specifies that we should optimize for
maximize throughput. We want a fast IPC mechanism that is well supported by most platforms. Since these two programs are built for POSIX
systems such as macOS/Linux, I chose POSIX Shared Memory for communication between producer and consumer. POSIX Shared Memory is an efficient
IPC mechanism as it allows us to "sidestep" the kernel when data exchanges take place. This is because we essentially use `mmap()` to create
page table entries in both processes that point to the memory region we wish to share. Although this solution targets POSIX systems, Windows NT
provides similar memory mapping and shared memory primitives; adding Windows support would not be too difficult.

## CSPROD (Producer)

### Short Description

csprod, the executable which acts as the producer, has the following responsibilities:

1. Open a file for input, sequentially read each line of text. The line read will correspond to one sentence.
2. The sentence read from the file should be formatted with the  sentence length prepended to sentence data.
> Ex:
> Let the example line read from a file be called L, L = "Hello, world!"
> We insert the following into the shared buffer: `[length(L)]["Hello, world!"]`
3. Populate shared buffer with **one or more** sentences; provided they fit into the shared buffer (size 1024).
4. On a new line, print out the sentence to the console.

csprod takes two command line arguments. The first is the file it wished to process, the second is the number
of shared buffers to create.

### ASSUMPTIONS:

#### A1

Each sentence corresponds to exactly one line read from the text file. So I will assume a file contain five
lines would be formatted in the following way:

*FileToRead.txt:*

```txt
Hello, my name is joey.
This is line two.
This is line three.
Is this real life?
Line five.
```
#### A2

The maximum length of a given sentence found in the input file is 256. I did some "back of the envelope" calculations with numbers
I grabbed from google which states your average sentence is about 75-100 characters. With shared buffer size being 1024, I figured
I would  be conservative and allow longer sentences. The length is still short enough to fit four sentences in a shared buffer if possible.

#### A3

A valid sentence will be delimited by standard punctuation:

* Period
* Question mark
* Exclamation

**NOTE: Producer will be started first.**

### Consumer

The consumer reads sentences from shared buffers. After doing so it should print
all the sentences which contain a substring S, where S is a specified command line argument given to the consumer.
**NOTE**: S may contains spaces!

**KEY POINTS:** The shared buffer data shouldn't be blindly trusted. We may assume an attack could be
attempting to alter the shared buffers. The consumer needs to safely process the input data. If the consumer
finds faulty data, we can halt processing and revert the buffer to a clean state for reuse int he future.

Invalid Data:

1. Header length doesn't match the length of the sentence appended.
2. The buffer doesn't contain ASCII printable characters.

**NOTE: Consumer will be started second.**

### Shared Buffer and Buffer Entry Details:


## POSIX SHMEM (IPC Mechanism)

Originally I had thought FIFO would be a good fit for this problem. It is simple to use and for the most part
we don't need to worry about synchronization/concurrency issues as the kernel helps us avoid race conditions,
deadlocks, etc. However, we want to maximize throughput and provide the fastest performance; POSIX Shared Memory
allows for this.

---------------

## SYNCHRONIZATION:

1. Safe data processing. Meaning:
    * No valid sentence is lost.
    * All shared buffers with valid sentences will be processed
2. Possible re-use of shared buffers (if needed)
3. Can use OS constructs/primitives

## Additional Comments From CS Document:

1. Design choices favor throughput
2. Consumer shouldn't crash after its initialization.
3. Producer starts first, exits when all sentences are processed by consumer.
4. Consumer starts second, doesn't terminate.

## MY QUESTIONS (ANSWERED):

1. "shared buffers" is whatever I interpret it to be. So FIFO or POSIX SHMEM
2. Assuming macOS/Linux is fine. Just note this in build instructions.
3. Make code robust as possible. Design in a way that that it works for 32bit and 64bit systems.
4. Constraints on number of buffers other resources is left to my discretion.
5. Invalid buffers could include: mismatch header size and sentence length, non-printable ASCII characters, etc..
6. IPC Mechanism I choose can be any of the POSIX mechanisms. Just state that in build as you do in question two.
7. Can I assume the producer is reading a text file where each line is one sentence? Do sentences span multiple lines?
8. It seems safe to assume each line has one sentence.
9. Maximum size of line isn't defined in the problem, this is left to my discretion.

