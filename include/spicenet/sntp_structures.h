#include <pthread.h>

#ifndef DONE
typedef struct app_tree
{
    int read[2];
    int apid;
    pthread_mutex_t mutex;
    pthread_cond_t read_ready;
    struct app_tree *next;
} sntp_app_t; //can be defined as something different for the user
#define DONE 1
#endif