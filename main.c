#include "System.h"
#include "buffered_open.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define READ_BUF 256
#define LINE_BUF 256

//--------------- Thread wrapper functions ---------------------------//
void* producer_routine(void* arg) {
    p_produce((Producer*)arg);
    return NULL;
}

void* dispatcher_routine(void* arg) {
    d_sort((Dispatcher*)arg);
    return NULL;
}

void* coeditor_routine(void* arg) {
    ce_job((CoEditor*)arg);
    return NULL;
}

void* screen_manager_routine(void* arg) {
    print_all((ScreenManager*)arg);
    return NULL;
}
//--------------------------------------------------------------------//

// buffered_readline using function from ex2.
int buffered_readline(buffered_file_t *bf, char *line, size_t maxlen)
{
    size_t pos = 0;
    char c;
    ssize_t r;

    while (pos < maxlen - 1) {
        r = buffered_read(bf, &c, 1);
        if (r == 0) break;        // EOF
        if (r < 0) return -1;     // Error

        line[pos++] = c;
        if (c == '\n') break;
    }

    if (pos == 0) return 0;      // EOF and no data

    line[pos] = '\0';
    return 1;                    // Line read
}

int main(int argc, char* argv[]) {

    // We should get only the config file
    if (argc != 2) {
        printf("Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    // Try to open the config file and if failed type an error message.
    buffered_file_t *file = buffered_open(argv[1], O_RDONLY);
    if (!file) {
        perror("Error opening config file");
        return 1;
    }

    // Array to hold producer (we keep it in an array for the dispatcher)
    Producer** producers = NULL;
    int producer_count = 0;
    int co_editor_queue_size = 0;

    // The bounded buffers we need
    BoundedBuffer* sportsQueue = NULL;
    BoundedBuffer* newsQueue = NULL;
    BoundedBuffer* weatherQueue = NULL;
    BoundedBuffer* screenQueue = NULL; // Shared queue for CoEditors -> Screen

    char line[256];
    
    // --- PARSING CONFIGURATION ---
    while (buffered_readline(file, line, sizeof(line)) > 0) {
        // Skip empty lines (standard safety check)
        if (line[0] == '\n' || line[0] == '\r') continue;

        if (strncmp(line, "PRODUCER", 8) == 0) {

            // Read next line: the ID
             buffered_readline(file, line, sizeof(line));
            int id = atoi(line);
            
            // Read next line: queue size = X
             buffered_readline(file, line, sizeof(line));
            char* sizePtr = strchr(line, '=');
            int queueSize = 0;
            if (sizePtr) {
                queueSize = atoi(sizePtr + 1);
            }

            // Realloc array and add new producer
            producer_count++;
            producers = (Producer**)realloc(producers, sizeof(Producer*) * producer_count);
            if(!producers) {
                perror("realloc failed");
                exit(1);
            }

            // Create Producer
            producers[producer_count - 1] = p_create(id, queueSize);

        } 

        else if (strncmp(line, "Co-Editor", 9) == 0) {
            char* sizePtr = strchr(line, '=');
            if (sizePtr) {
                co_editor_queue_size = atoi(sizePtr + 1);
            }
        }
    }
    buffered_close(file);

    // --- INITIALIZATION ---
    
    // Create Dispatcher Queues (Sports, News, Weather) using Co-Editor size
    sportsQueue = bb_create(co_editor_queue_size);
    newsQueue = bb_create(co_editor_queue_size);
    weatherQueue = bb_create(co_editor_queue_size);
    
    // Create Screen Manager Queue (Shared queue for Co-Editors)
    screenQueue = bb_create(co_editor_queue_size);

    // Create Dispatcher
    Dispatcher* dispatcher = d_create(producer_count, producers, sportsQueue, newsQueue, weatherQueue);

    // Create Co-Editors
    CoEditor* sportsEditor = ce_create(sportsQueue, screenQueue);
    CoEditor* newsEditor = ce_create(newsQueue, screenQueue);
    CoEditor* weatherEditor = ce_create(weatherQueue, screenQueue);

    // Create Screen Manager
    ScreenManager* screenManager = sm_create(screenQueue);

    // --- THREAD CREATION ---
    pthread_t* producer_threads = (pthread_t*)malloc(sizeof(pthread_t) * producer_count);
    if(!producer_threads) {
        perror("malloc failed");
        exit(1);
    }
    pthread_t dispatcher_thread;
    pthread_t coeditor_threads[3];
    pthread_t screen_manager_thread;

    // Start Producers
    for (int i = 0; i < producer_count; i++) {
        pthread_create(&producer_threads[i], NULL, producer_routine, producers[i]);
    }

    // Start Dispatcher
    pthread_create(&dispatcher_thread, NULL, dispatcher_routine, dispatcher);

    // Start Co-Editors
    pthread_create(&coeditor_threads[0], NULL, coeditor_routine, sportsEditor);
    pthread_create(&coeditor_threads[1], NULL, coeditor_routine, newsEditor);
    pthread_create(&coeditor_threads[2], NULL, coeditor_routine, weatherEditor);



    // Start Screen Manager
    pthread_create(&screen_manager_thread, NULL, screen_manager_routine, screenManager);

    // --- WAIT FOR COMPLETION ---
    
    // Wait for producers
    for (int i = 0; i < producer_count; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    
    // Wait for dispatcher
    pthread_join(dispatcher_thread, NULL);
    
    // Wait for co-editors
    for (int i = 0; i < 3; i++) {
        pthread_join(coeditor_threads[i], NULL);
    }
    
    // Wait for screen manager
    pthread_join(screen_manager_thread, NULL);

    // --- CLEANUP ---
    
    // Free producer threads array
    free(producer_threads);

    bb_destroy(newsQueue);
    bb_destroy(weatherQueue);
    bb_destroy(sportsQueue);

    free(sportsEditor);
    free(newsEditor);
    free(weatherEditor);

    // Note: Most other memory is freed inside the thread functions (p_produce, d_sort, etc.)
    // However, the producer array itself 'producers' is still allocated.
    free(producers);
    return 0;
}