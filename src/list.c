#include "list.h"

list *list_init(size_t max_element, size_t element_size)
{

    list *ret = (list *)malloc(sizeof(list));
    ret->element_size = element_size;
    ret->max_element = max_element + 1; // need a emement space to figure out the list is full or empty
    ret->storage = (void *)malloc((max_element + 1) * element_size);
    ret->front = 0;
    ret->tail = 0;
    return ret;
}

void list_destroy(list *listp)
{
    free(listp->storage);
    free(listp);
}

int list_resize(list *listp, size_t new_length)
{
    listp->storage = realloc(listp->storage, new_length);
    if (listp->storage == NULL)
    {
        return 0;
    }
    listp->max_element = listp->max_element * 2 + 1;
    return 1;
}

int list_push_back(list *listp, void *value)
{
    //list full
    if ((listp->tail + 1) % listp->max_element == listp->front)
    {
        return 0;
    }
    memcpy(listp->storage + listp->element_size * listp->tail, value, listp->element_size);
    listp->tail = (listp->tail + 1) % listp->max_element;
    return 1;
}

void list_pop_front(list *listp)
{
    listp->front = (listp->front + 1) % listp->max_element;
}

int list_empty(list *listp)
{
    return listp->front == listp->tail;
}

void *list_front(list *listp)
{
    if (list_empty(listp))
    {
        return NULL;
    }
    return listp->storage + listp->front * listp->element_size;
}

void list_clear(list *listp)
{
    listp->tail = listp->front;
}

size_t list_size(list *listp)
{
    return (listp->max_element + listp->tail - listp->front) % listp->max_element;
}

size_t list_capacity(list *listp)
{
    return listp->max_element - 1;
}

int list_full(list *listp)
{
    return (listp->tail + 1) % listp->max_element == listp->front;
}