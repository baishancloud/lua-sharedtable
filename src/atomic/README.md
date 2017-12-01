# Atomic Operations

Atomic operations are ones which manipulate memory in a way that appears indivisible,
no thread can observe the operation half-complete

The function can be used with any integral scalar or pointer type that is 1, 2, 4, or 8 bytes in length.

If you handle integer in multi_processes, you can use the atomic function, that don't need lock.

## Usage:

Example

```
#include <sys/mman.h>
#include <unistd.h>
#include "atomic.h"

int main()
{
    int child, ret;
    int64_t *v = mmap(NULL, sizeof(int64_t), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *v = 0;

    child = fork();
    if (child == 0) {
        for (int i = 0; i < 10000; i++) {
            st_atomic_inc(v);
        }

        exit(0);
    }

    for (int i = 0; i < 10000; i++) {
        st_atomic_inc(v);
    }

    waitpid(child, &ret, 0);

    munmap(v, size);
    return 0;
}
```
