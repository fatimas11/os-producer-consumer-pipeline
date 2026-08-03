#include "System.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

CoEditor* ce_create(BoundedBuffer* bb, BoundedBuffer* all){
    // Alllocate memory for the co-editor struct
    CoEditor* ce = (CoEditor*)malloc(sizeof(CoEditor));
    if(!ce) {
        perror("Co-Editor malloc failed");
        exit(1);
    } 
    ce->allBB = all;
    ce->ceBB = bb;
    ce->done = 0;
    return ce;
}

void ce_job(CoEditor* ce) {
    char* message;
    // While the co-editor didnt get DONE read messages.
    while (ce->done == 0) {
        message = bb_consume(ce->ceBB);
        if (strcmp(message, "DONE") == 0) {
            ce->done = 1;
            free(message);
            continue;
        } else {
            bb_produce(ce->allBB, message);
            free(message);
            if(usleep(100000) == -1) {
                perror("usleep");
                exit(1);
            }
        }
    }
    bb_produce(ce->allBB, "DONE");
}