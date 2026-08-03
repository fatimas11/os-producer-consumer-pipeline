#ifndef BOUNDED_BUFFER_H
#define BOUNDED_BUFFER_H

#include <sys/types.h>
// --------------------- bounded buffer ----------------------//

// Internal struct for the "Class Algorithm" Counting Semaphore
typedef struct {
    int value;      // The integer counter
    int mutex_idx;  // Index of the binary mutex in the semaphore set
    int block_idx;  // Index of the binary block in the semaphore set
} BinaryCountingSem;

// bounded buffers struct.
typedef struct {
    char** buffer;     // The buffer that have the items (strings).
    int capacity;    // The buffers size.
    int head;        // The head index for bb_consume function.
    int tail;        // The tail index for bb_produce function.
    int semid;       // System V Semaphore Set ID.

    BinaryCountingSem empty; // "Empty slots" counter
    BinaryCountingSem full;  // "Full slots" counter
} BoundedBuffer;

// Creates the buffer and the Semaphore.
BoundedBuffer* bb_create(int size);

// Add product to the buffer.
void bb_produce(BoundedBuffer* bb, char* item);

// Remove product from the buffer and get BLOCKE if its empty.
char* bb_consume(BoundedBuffer* bb);

// Remove product from the buffer and if its empty return NULL.
char* bb_consume_nonblocking(BoundedBuffer* bb);

// Clean and free all.
void bb_destroy(BoundedBuffer* bb);

// --------------------- Producer ----------------------//

// Each producer struct.
typedef struct {
    int producerId;
    int massegesNum;
    BoundedBuffer* pbb;
    int NEWS_counter;
    int WEATHER_counter;
    int SPORTS_counter;
} Producer;

// Create the producer and its private bounded buffer.
Producer* p_create(int id, int num);

// produce all the masseges and send it to its private buffer.
void p_produce(Producer* p);

// --------------------- Dispatcher ----------------------//

// the dispatcher struct.
typedef struct {
    BoundedBuffer** producers_buffers;
    Producer** producers;
    BoundedBuffer* sports;
    BoundedBuffer* news;
    BoundedBuffer* weather;

    int dones_counter;
    int producers_num;
} Dispatcher;

// Create the dispatcher
Dispatcher* d_create(int producers_num, Producer** producers, BoundedBuffer* Sbb, BoundedBuffer* Nbb, BoundedBuffer* Wbb);

// Sort each message using a round robin algorithm.
void d_sort(Dispatcher* d);

// --------------------- Co-Editor ----------------------//

// struct for each Co-Editor
typedef struct {
    BoundedBuffer* ceBB;
    BoundedBuffer* allBB;
    int done;
} CoEditor;

// Create a Co-Editor.
CoEditor* ce_create(BoundedBuffer* bb, BoundedBuffer* all);

// Co-Editor job.
void ce_job(CoEditor* ce);

// --------------------- Screen Manager ----------------------//

typedef struct {
    int dones_counter;
    BoundedBuffer* allBB;
} ScreenManager;

// Create the screen manager.
ScreenManager* sm_create(BoundedBuffer* all);

// Print all the messages to the screen.
void print_all(ScreenManager* sm);

#endif

