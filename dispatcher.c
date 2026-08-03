#include "System.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Dispatcher* d_create(int producers_num, Producer** producers, BoundedBuffer* Sbb, BoundedBuffer* Nbb, BoundedBuffer* Wbb) {

    // Allocate place for the dispatcher buffer.
    Dispatcher* d = (Dispatcher*)malloc(sizeof(Dispatcher));
    if (!d) {
        perror("malloc dispatcher failed");
        exit(1);
    }

    // Allocate place for the producers bufferes (pointer to array of pointers to each buffer).
    d->producers_buffers = (BoundedBuffer**)malloc(sizeof(BoundedBuffer*) * producers_num);
    if(!d->producers_buffers) {
        perror("malloc failed");
        exit(1);
    }
    
    // Get each buffer from the Producers array.
    for (int i = 0; i < producers_num; i++) {
        d->producers_buffers[i] = producers[i]->pbb;
    }

    d->dones_counter = 0;
    d->news = Nbb;
    d->sports = Sbb;
    d->weather = Wbb;
    d->producers_num = producers_num;
    d->producers = producers;

    return d;
}

void d_sort(Dispatcher* d) {
    char* message;
    char id_str[10], type[10], count_str[10];

    // read for each producers its messages using round robin.
    while (d->dones_counter < d->producers_num) {
        BoundedBuffer** bb = d->producers_buffers;
        for (int i = 0; i < d->producers_num; i++) {
            message = bb_consume_nonblocking(bb[i]);
            if (!message) {
                continue;
            } else if (strcmp(message, "DONE") == 0) {
                d->dones_counter++;
                free(message);
                continue;
            }
            // skip the word PRODUCER.
            char* ptr = message + 8; 

            // "Do ++ for pointer until get to letter"
            // This loop skips the spaces AND the number <j> in one go.
            // It stops only when it hits the first letter of <type>.
            while (*ptr && !( (*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') )) {
                    ptr++;
                }
            
            // Capture the word
            char* start = ptr;
            
            // Move ptr to the end of the word (find the next space or newline or \0)
            while (*ptr && *ptr != ' ' && *ptr != '\n') {
                ptr++;
            }

            // Allocate and copy
            int len = ptr - start;
            char* type = (char*)malloc(len + 1);
            if (!type) {
                perror("malloc failed");
                exit(1);
            }
            
            strncpy(type, start, len);
            type[len] = '\0';
            if (strcmp(type, "SPORTS") == 0) {
                bb_produce(d->sports, message);
            } else if (strcmp(type, "NEWS") == 0) {
                bb_produce(d->news, message);
            } else if (strcmp(type, "WEATHER") == 0) {
                bb_produce(d->weather, message);
            }
            free(type);
            free(message);
        }
    }
    // Write done to each buffer.
    bb_produce(d->sports, "DONE");
    bb_produce(d->news, "DONE");
    bb_produce(d->weather, "DONE");

    for (int i = 0 ; i < d->producers_num ; i++) {
        bb_destroy(d->producers_buffers[i]);
        free(d->producers[i]);
    }
    free(d->producers_buffers);
    free(d);
}