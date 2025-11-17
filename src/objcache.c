#include "objcache.h"
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4 * 1024 // 4K

typedef struct obj_slabctl {
  int ref_count;
  struct obj_slabctl *next;
  struct obj_slabctl *prev;
} obj_slabctl_t;

typedef struct objc_cache {
  char *name;
  size_t size;
  int align;
  constructor c;
  destructor d;
  obj_slabctl_t *free_slab; // partial or complete slab
} objc_cache_t;

objc_cache_t *objc_cache_create(char *name, size_t size, int align,
                                constructor c, destructor d) {
  objc_cache_t *cache = (objc_cache_t *)malloc(sizeof(*cache));
  cache->name = name;
  cache->size = size;
  cache->align = align;
  cache->c = c;
  cache->d = d;
  cache->free_slab = NULL; // slab is not created yet

  return cache;
}
