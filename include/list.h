#ifndef _LIST_H_
#define _LIST_H_

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>


//START_SIZE can not be 1
#define START_SIZE 20

typedef struct list_t
{
    uint front;
    uint tail;
    void *storage;
    // void *ptr;
    size_t max_element;
    size_t element_size;

} list;

list *list_init(size_t, size_t);
void list_destroy(list *);
int list_push_back(list *, void *);
void list_pop_front(list *);
void *list_front(list *);
int list_empty(list *);
void list_clear(list *);
size_t list_capacity(list *);
size_t list_size(list *);
int list_full(list *);

#endif