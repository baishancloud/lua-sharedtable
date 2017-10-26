#ifndef _CALLBACK_H_INCLUDED_
#define _CALLBACK_H_INCLUDED_

typedef void * (*st_callback_malloc_t) (void *pool, size_t size);
typedef void * (*st_callback_realloc_t) (void *pool, void *ptr, size_t size);
typedef void * (*st_callback_calloc_t) (void *pool, size_t nmemb, size_t size);
typedef void (*st_callback_free_t) (void *pool, void *ptr);

typedef struct st_callback_memory_s st_callback_memory_t;

struct st_callback_memory_s {
    void *pool;

    st_callback_malloc_t malloc_func;
    st_callback_realloc_t realloc_func;
    st_callback_calloc_t calloc_func;
    st_callback_free_t free_func;

    int inited;
};

#define s3_callback_null { \
    .pool = NULL,          \
    .malloc_func = NULL,   \
    .realloc_func = NULL,  \
    .calloc_func = NULL,   \
    .free_func = NULL,     \
    .inited = 0,           \
}

#endif /* _CALLBACK_H_INCLUDED_ */
