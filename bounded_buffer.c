#include "System.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>

// Definitions of the Semaphores
// 0: Main Buffer Mutex (protects char** buffer)
// 1: Empty-Counter Mutex
// 2: Empty-Counter Block
// 3: Full-Counter Mutex
// 4: Full-Counter Block

#define IDX_BUFF_MUTEX 0

#define IDX_EMPTY_MUTEX 1
#define IDX_EMPTY_BLOCK 2

#define IDX_FULL_MUTEX  3
#define IDX_FULL_BLOCK  4

// Initialise semaphore.
union semun {
    int val;               
    struct semid_ds *buf;  
    unsigned short *array; 
};

static void binary_down(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_flg = 0;
    sb.sem_op = -1;
    if (semop(semid, &sb, 1) == -1) {
        perror("binary_down failed");
        exit(1);
    }
}

static void binary_up(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_flg = 0;
    sb.sem_op = 1;
    if (semop(semid, &sb, 1) == -1) {
        perror("binary_up failed");
        exit(1);
    }
}

static void counting_wait(BoundedBuffer* bb, BinaryCountingSem* s) {
    // Lock the mutex protecting the integer 'value'
    binary_down(bb->semid, s->mutex_idx);

    // Decrement value
    s->value--;

    // Check if we need to block
    if (s->value < 0) {
        // Release mutex
        binary_up(bb->semid, s->mutex_idx);
        // Block
        binary_down(bb->semid, s->block_idx);
    } else {
        // Release the mutex
        binary_up(bb->semid, s->mutex_idx);
    }
}

static void counting_signal(BoundedBuffer* bb, BinaryCountingSem* s) {
    // Lock the mutex protecting the integer 'value'
    binary_down(bb->semid, s->mutex_idx);

    // Increment value
    s->value++;

    // If there are sleepers (value <= 0), wake one up
    if (s->value <= 0) {
        binary_up(bb->semid, s->block_idx);
    }

    // Release mutex
    binary_up(bb->semid, s->mutex_idx);
}

// Returns 1 if successful (decremented), 0 if it would block
static int counting_trywait(BoundedBuffer* bb, BinaryCountingSem* s) {
    binary_down(bb->semid, s->mutex_idx);
    
    if (s->value <= 0) {
        // It would block, so we abort
        binary_up(bb->semid, s->mutex_idx);
        return 0; 
    }
    
    // Safe to decrement
    s->value--;
    binary_up(bb->semid, s->mutex_idx);
    return 1;
}

BoundedBuffer* bb_create(int size) {

    // Allocate memory for the boumded buffer struct.
    BoundedBuffer* bb = (BoundedBuffer*)malloc(sizeof(BoundedBuffer));
    if (!bb) {
        perror("malloc for the struct");
        exit(1);
    }

    // Alocate memory for the buffer that holds the items and in the loop allocate for the items.
    char** buffer = (char**)malloc(sizeof(char*) * size);
    if (!buffer) {
        perror("malloc for the buffer");
        exit(1);
    }

    // Initialize pointers to NULL for safety
    bb->buffer = buffer; 
    for (int i = 0; i < size; i++) {
        bb->buffer[i] = NULL; 
    }

    bb->capacity = size;
    bb->head = 0;
    bb->tail = 0;
    
    // Create a new System V semaphore set with 5 semaphores,
    // accessible only by the owner (read/write permissions).
    int semid = semget(IPC_PRIVATE, 5, 0600);
    if (semid == -1) { 
        perror("semget failed");
        exit(1);
    }
    bb->semid = semid;

    union semun arg;
    unsigned short values[5];

    // 0: Buff Mutex  -> 1 (Unlocked)
    // 1: Empty Mutex -> 1 (Unlocked)
    // 2: Empty Block -> 0 (Locked)
    // 3: Full Mutex  -> 1 (Unlocked)
    // 4: Full Block  -> 0 (Locked)
    values[IDX_BUFF_MUTEX]  = 1;
    values[IDX_EMPTY_MUTEX] = 1; 
    values[IDX_EMPTY_BLOCK] = 0; 
    values[IDX_FULL_MUTEX]  = 1; 
    values[IDX_FULL_BLOCK]  = 0; 

    arg.array = values;
    if (semctl(semid, 0, SETALL, arg) == -1) { 
        perror("semctl init failed");
        exit(1); 
    }

    // Empty: starts at 'size'
    bb->empty.value = size;
    bb->empty.mutex_idx = IDX_EMPTY_MUTEX;
    bb->empty.block_idx = IDX_EMPTY_BLOCK;

    // Full: starts at 0
    bb->full.value = 0;
    bb->full.mutex_idx = IDX_FULL_MUTEX;
    bb->full.block_idx = IDX_FULL_BLOCK;

    return bb;
}

void bb_produce(BoundedBuffer* bb, char* item) {

    // Check if the buffer is full if so get BLOCKED. (wait for empty places)
    counting_wait(bb, &bb->empty);

    // Get the lock to prevent race conditions.
    binary_down(bb->semid, IDX_BUFF_MUTEX);

    // ---------- Start of CS -----------
    int len = strlen(item);
    char* copy = (char*)malloc(sizeof(char)* (len+1));
    if (!copy) {
        perror("malloc failed in creating a copy for item");
        exit(1);
    }
    strcpy(copy,item);
    bb->buffer[bb->tail] = copy; 
    bb->tail = (bb->tail + 1) % bb->capacity;
    // ----------  End of CS  -----------

    // Release the lock.
    binary_up(bb->semid, IDX_BUFF_MUTEX);

    // Signal that I produced an item.
    counting_signal(bb, &bb->full);
}

// this function for dispatcher only so if the buffer is empty he continue to go over other producer's buffers without getting blocked.
char* bb_consume_nonblocking(BoundedBuffer* bb) {

    // Check if the buffer is empty then return NULL (don't block).
    if (counting_trywait(bb, &bb->full) == 0) {
        return NULL;
    }

    // Get the lock to prevent race conditions.
    binary_down(bb->semid, IDX_BUFF_MUTEX);

    // ---------- Start of CS ----------- //
    char* item = bb->buffer[bb->head]; // Get the existing pointer
    bb->buffer[bb->head] = NULL;       // Clear the slot
    bb->head = (bb->head + 1) % bb->capacity;
    // ----------  End of CS  ----------- //

    // Release the lock.
    binary_up(bb->semid, IDX_BUFF_MUTEX);

    // Signal the empty semaphore that I consumed an item.
    counting_signal(bb, &bb->empty);

    return item;  
}

// Normal consume function get blocked when the buffer is empty
char* bb_consume(BoundedBuffer* bb) {

    // Check if the buffer is EMPTY if so get BLOCKED. (wait for items).
    counting_wait(bb, &bb->full);

    // Get the lock to prevent race conditions.
    binary_down(bb->semid, IDX_BUFF_MUTEX);

    // ---------- Start of CS ----------- //
    char* item = bb->buffer[bb->head]; // Get the existing pointer
    bb->buffer[bb->head] = NULL;       // Clear the slot
    bb->head = (bb->head + 1) % bb->capacity;
    // ----------  End of CS  ----------- //

    // Release the lock.
    binary_up(bb->semid, IDX_BUFF_MUTEX);

    // Signal the empty semaphore that I consumed an item.
    counting_signal(bb, &bb->empty);

    return item;
}

void bb_destroy(BoundedBuffer* bb) {
    if (semctl(bb->semid, 0, IPC_RMID) == -1) {
        perror("fail to delete the semaphore");
    }

    // Free any strings left in the buffer (unsent messages)
    for (int i = 0; i < bb->capacity; i++) {
        if (bb->buffer[i] != NULL) {
            free(bb->buffer[i]);
        }
    }
    free(bb->buffer);
    free(bb);
}