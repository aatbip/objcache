#ifndef __OBJCACHE__
#define __OBJCACHE__

#include <stdio.h>

typedef struct objc_cache objc_cache_t;

typedef void (*constructor)(void *, size_t);
typedef void (*destructor)(void *, size_t);

objc_cache_t *objc_cache_create(char *name, size_t size, int align,
                                constructor c, destructor d);

void *objc_cache_alloc(objc_cache_t *cache);

#endif // !__OBJCACHE__
