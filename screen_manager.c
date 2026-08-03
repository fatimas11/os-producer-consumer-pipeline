#include "System.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ScreenManager* sm_create(BoundedBuffer* all) {

    // Allocate memory for the Screen manager struct.
    ScreenManager* sm = (ScreenManager*)malloc(sizeof(ScreenManager));
    if (!sm) {
        perror("malloc in screen manager");
        exit(1);
    }
    sm->allBB=all;
    sm->dones_counter=0;
    return sm;
}

void print_all(ScreenManager* sm) {

    // while the screen manager didnt get all three dones from each co-editor continue reading messages.
    while (sm->dones_counter < 3) {
        char* message = bb_consume(sm->allBB);
        if (strcmp(message, "DONE") == 0) {
            sm->dones_counter++;
            free(message);
        } else {
            printf("%s\n",message);
            free(message);
        }
    }
    printf("DONE\n");
    bb_destroy(sm->allBB);
    free(sm);
}