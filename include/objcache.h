#ifndef OBJCACHE_H
#define OBJCACHE_H

#include <stdint.h>
#include <stdio.h>

typedef struct objc_cache objc_cache_t;

typedef struct objc_cache_info {
  int cache;                  // size of cache in bytes
  uint16_t unused;            // size of unused bytes in current free slab
  uint8_t slabctl;            // size of slabctl
  unsigned short buffer_size; // size of obj + bufctl
  unsigned short total_buf;   // total number of buffers that fits in a slab
  unsigned short slab_count;  // number of slabs created in the cache
} objc_cache_info_t;

typedef void (*constructor)(void *, size_t);
typedef void (*destructor)(void *, size_t);

objc_cache_t *objc_cache_create(char *name, size_t size, int align, constructor c, destructor d);

void *objc_cache_alloc(objc_cache_t *cache);

void objc_free(objc_cache_t *cache, void *obj);

void objc_cache_destroy(objc_cache_t *cache);

objc_cache_info_t objc_cache_info(objc_cache_t *cache);

#endif // !OBJCACHE_H
