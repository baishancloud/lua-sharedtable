#ifndef _CALLBACK_H_INCLUDED_
#define _CALLBACK_H_INCLUDED_

typedef void * (*st_callback_malloc_f) (void *pool, size_t size);
typedef void * (*st_callback_realloc_f) (void *pool, void *ptr, size_t size);
typedef void * (*st_callback_calloc_f) (void *pool, size_t nmemb, size_t size);
typedef void (*st_callback_free_f) (void *pool, void *ptr);

typedef int (*st_callback_alloc_with_ret_f) (void *pool, size_t size, void **ptr);
typedef int (*st_callback_free_with_ret_f) (void *pool, void *ptr);

typedef struct st_callback_memory_s st_callback_memory_t;

struct st_callback_memory_s {
    void *pool;

    st_callback_malloc_f malloc;
    st_callback_realloc_f realloc;
    st_callback_calloc_f calloc;
    st_callback_free_f free;

    st_callback_alloc_with_ret_f alloc_with_ret;
    st_callback_free_with_ret_f free_with_ret;
};

#endif /* _CALLBACK_H_INCLUDED_ */
