#include "threadpool.h"

void *get_request(threadpool *pool)
{
    sem_wait(&pool->pool_sem);
    pthread_mutex_lock(&pool->pool_locker);
    void *ret = NULL;
    if (!list_empty(pool->pool_request_list))
    {
        ret = list_front(pool->pool_request_list);
        list_pop_front(pool->pool_request_list);
    }
    pthread_mutex_unlock(&pool->pool_locker);
    return ret;
}

void *thread_work(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    while (1)
    {
        void *req = get_request(pool);
        if (req == NULL)
        {
            continue;
        }
        else
        {
            pool->pool_handler(req);
        }
    }
    return NULL;
}

int append_request(threadpool *pool, void *request)
{
    pthread_mutex_lock(&pool->pool_locker);
    if (list_full(pool->pool_request_list))
    {
        return 0;
    }
    list_push_back(pool->pool_request_list, &request);
    pthread_mutex_unlock(&pool->pool_locker);
    sem_post(&pool->pool_sem);
    return 1;
}

threadpool *threadpool_init(void *(*handler)(void *))
{
    threadpool *ret = (threadpool *)malloc(sizeof(threadpool));
    ret->pool_request_list = list_init(MAX_REQUEST, sizeof(void *));
    sem_init(&ret->pool_sem, 0, 0);
    pthread_mutex_init(&ret->pool_locker, NULL);
    ret->pool_handler = handler;
    ret->pool_threads = (pthread_t *)malloc(THREAD_NUMBER * sizeof(pthread_t));
    ret->pool_threads_num = THREAD_NUMBER;

    int i = 0;
    for (; i < ret->pool_threads_num; i++)
    {
        if (pthread_create(&(ret->pool_threads)[i], NULL, thread_work, (void *)ret) != 0)
        {
            free(ret->pool_threads);
        }
        if (pthread_detach(ret->pool_threads[i]))
        {
            free(ret->pool_threads);
        }
    }
    return ret;
}

void *threadpool_destroy(threadpool *ptr)
{
    sem_destroy(&ptr->pool_sem);
    pthread_mutex_destroy(&ptr->pool_locker);
    list_destroy(ptr->pool_request_list);
    return NULL;
}
