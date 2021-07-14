#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "list.h"

#define THREAD_NUMBER 1
#define MAX_REQUEST 10000

typedef struct threadpool_t
{
    pthread_mutex_t pool_locker;
    sem_t pool_sem;
    pthread_t *pool_threads;
    size_t pool_threads_num;
    void *(*pool_handler)(void *arg);
    list *pool_request_list;
} threadpool;

threadpool *threadpool_init(void *(*)(void *));
void *threadpool_destroy(threadpool *);
int append_request(threadpool *pool, void *);

#endif