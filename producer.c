#include "System.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>

Producer* p_create(int id, int num) {
    //Allocate place for the producer struct.
    Producer* p = (Producer*)malloc(sizeof(Producer));
    if (!p) {
        perror("malloc failed for a producer");
        exit(1);
    }

    // Create the private buffer for the producer.
    BoundedBuffer* pbb = bb_create(num+1);

    p->massegesNum = num;
    p->pbb = pbb;
    p->producerId = id;
    p->NEWS_counter = 0;
    p->SPORTS_counter = 0;
    p->WEATHER_counter = 0;
    return p;
}

void p_produce(Producer* p) { 
    // Initialise the random. 
    srand(time(NULL));
    int random_type;

    char massege[100];
    // Create the messages for the producer.
    for(int i= 0 ; i < p->massegesNum ; i++) {
        //randomly pick number 0-2 and accordingly pick the type and create the message.
        random_type = rand() % 3;
        switch (random_type)
        {
        case 0:
            sprintf(massege, "producer %d NEWS %d", p->producerId, p->NEWS_counter);
            p->NEWS_counter++;
            break;

        case 1:
            sprintf(massege, "producer %d SPORTS %d", p->producerId, p->SPORTS_counter);
            p->SPORTS_counter++;
            break;

        case 2:
            sprintf(massege, "producer %d WEATHER %d", p->producerId, p->WEATHER_counter);
            p->WEATHER_counter++;
            break;
        }
        bb_produce(p->pbb,massege); 
    }
    // Write done after finishing the mwssages.
    sprintf(massege, "DONE");
    bb_produce(p->pbb, massege);
}

