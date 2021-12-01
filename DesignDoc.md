# CrowdStrike Producer/Consumer Task Design Document - PREDRAFT

## Design Problem:

* Design a system that consists of two applications; one acts as a producer, the other a consumer.
* The two applications should communicate through N  _shared buffers_, where N can be configured by user.
* Shared buffers have a maximum size of 1024 bytes.

### Producer

Producer will do the following:

1. Read an input file containing any number of lines of text.
2. Populate shared buffers with the sentences read from file. **NOTE:** Each sentence
corresponds to a line read from the text file.
3. Print each sentence to console after we add to buffer.

**NOTE: Producer will be started first.**

### Consumer

The consumer reads sentences from shared buffers. After doing so it should print
all the sentences which contain a substring S, where S is a specified parameter to the consumer.
**NOTE**: S may contains spaces!

**KEY POINTS:** The shared buffer data shouldn't be blindly trusted. We may assume an attack could be
attempting to alter the shared buffers. The consumer needs to safely process the input data. If the consumer
finds faulty data, we can halt processing and revert the buffer to a clean state for reuse int he future.

What does it mean for buffer entry data to be invalid?:

I am going to assume it means the entry header
containing the string length doesn't match the true size of the string data. Therefore its imperative we
don't blindly trust the `sent_len` header of an entry.

**NOTE: Consumer will be started second.**

### Shared Buffer and Buffer Entry Details:

There will be N *shared buffers*. The shared buffers could be one of several IPC
mechanisms. This Producer/Consumer lends itself nicely to FIFO buffers; because
there is one producer and one consumer. Costly synchronization mechanisms can be avoided
as well. This will help us get better performance.

```c
struct shared_buff {
 // FIFO buffs....
}

```


Each entry in the shared buffers should have the following format:


```c
struct buffer_entry {
    unsigned long sent_len;
    char sentence[];
}
```

* We want to fill the shared buffers with as many sequential entries as possible.
* Because the FIFO, if multiple messages are inside our shared buffer, we can skip a delimiter because we have
the sent_len prepended to the sentence.

------------

## DECISIONS TO PONDER:

1. What IPC mechanism is most suitable for this problem? We want to optimize for throughput according to docs.
2. What synchronization mechanisms would be most efficient? We could use a pattern where there is a monitor as mutual exclusion
is implicit and we don't need locks or anything to protect critical sections.
3. If consumer stumbled upon invalid data from a shared buffer, what actions should it take (clear for other entry and print
a warning message to user?)

## SYNCHRONIZATION:

1. Safe data processing. Meaning:
    * No valid sentence is lost.
    * All shared buffers with valid sentences will be processed
2. Possible re-use of shared buffers (if needed)
3. Can use OS constructs/primitives (What OS can I assume?)

## Additional Comments From CS Document:

1. Design choices favor throughput
2. Consumer shouldn't crash after its initialization.
3. Producer starts first, exits when all sentences are processed by consumer.
4. Consumer starts second, doesn't terminate (like a daemon)

## MY QUESTIONS:

1. "shared buffers" Can be buffers of whichever IPC mechanism I choose? Or do you want me to use shared memory?
2. What OS can assume this will run on? Is it safe to say it with be a POSIX compliant machine like macOS/Linux?
3. What C data model can I assume we are using? Will this be a 64bit system using LP64? Is it safe for me to assume
`unsigned long` will be 64 bits (uint64_t)?
4. The directions state to create two applications but I want to be clear. You want TWO independent applications written
and not simply one that forks or creates a thread to act as the consumer or producer correct? That is, my solution
will consist of two C/C++ programs: consumer.c and producer.c?
5. Is there a specific MAX number of shared buffers you would like to enforce? Am I free to dynamically choose and set limits
for the number of shared buffers? The OS has resource limits I will need to abide by for certain, but are there other limits
I might consider?
6. What exactly should I consider an "invalid" buffer entry? I will consider an entry invalid if the size listed in the header and
the sentence length don't match up. Are there any more cases I should consider? Are certain words/sentences not allowed?
If we are just printing the sentences out to STDOUT of a console, there shouldn't be too much to worry about aside perhaps
an attacker inserting VT100 escape sequences or other terminal patterns to mess up terminal output. Is that something
we care about?
7. Am I limited in choice to which IPC mechanism I choose? Different OS offer different ones; although they all usually have
the basic ones that can be used for a task like this. If the solution has to be platform independent, it narrows choices.
Can I assume this is running on Linux or MacOS?
8. Can I assume the producer is reading a text file where each line is one sentence? do sentences span multiple lines?
9. Is there a maximum line length I can assume for the producer when reading a file? Shared buffers are 1024 bytes, we use a header
to indicate size of the message that's uint64_t, can we have a sentence that long? If not, can I make the size header a uint16_t as
that is all we would need to represent the size of a sentence of max length < 1024?


