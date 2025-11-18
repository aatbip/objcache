#include "objcache.h"
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4 * 1024 // 4K

typedef struct objc_bufctl {
  void *next;
} objc_bufctl_t;

typedef struct objc_slabctl {
  int ref_count;
  objc_bufctl_t *freebuf; // head of the freebuf
  struct objc_slabctl *next;
  struct objc_slabctl *prev;
} objc_slabctl_t;

typedef struct objc_cache {
  char *name;
  size_t size;
  int align;
  constructor c;
  destructor d;
  objc_slabctl_t *free_slab; // partial or complete slab
} objc_cache_t;

objc_cache_t *objc_cache_create(char *name, size_t size, int align, constructor c, destructor d) {
  objc_cache_t *cache = (objc_cache_t *)malloc(sizeof(*cache));
  cache->name = name;
  cache->size = size;
  cache->align = align;
  cache->c = c;
  cache->d = d;
  cache->free_slab = NULL; // slab is not created yet

  return cache;
}

void *objc_cache_alloc(objc_cache_t *cache) {
  // printf("page size: %d\nobjc_slabctl_t: %zu\ncache size: %zu \n
  // objc_bufctl_t: %zu\n",
  //        PAGE_SIZE, sizeof(objc_slabctl_t), cache->size,
  //        sizeof(objc_bufctl_t));

  void *slab = cache->free_slab;

  unsigned short buffer_size = cache->size + sizeof(objc_bufctl_t);
  // printf("buf size: %d\n", buffer_size);

  /*total number of buffers that can be allocated*/
  unsigned short total_buf = (PAGE_SIZE - sizeof(objc_slabctl_t)) / (buffer_size);
  // printf("total buf: %d\n", total_buf);

  if (!slab) {
    /* If a free slab is not available or a slab is not yet created
     * then we create a new empty slab of PAGE_SIZE.*/
    slab = malloc(PAGE_SIZE);

    /*fragmented bytes*/
    // unsigned short unused =
    //     (PAGE_SIZE - sizeof(objc_slabctl_t)) % (buffer_size);

    for (char *p = slab; p < (char *)slab + total_buf * buffer_size; p += buffer_size) {

      objc_bufctl_t *bufctl = (objc_bufctl_t *)(p + cache->size);
      char *next_buf = p + buffer_size;
      if (next_buf >= (char *)slab + total_buf * buffer_size) {
        bufctl->next = NULL;
      } else {
        bufctl->next = (objc_bufctl_t *)next_buf;
      }
    }
    objc_slabctl_t *slabctl = (objc_slabctl_t *)((char *)slab + (total_buf * buffer_size) - sizeof(objc_slabctl_t));

    slabctl->freebuf = ((objc_bufctl_t *)(slab + cache->size))->next;
    cache->free_slab = slab;

    cache->c(slab, cache->size);

    return slab;
  }
  objc_slabctl_t *slabctl = (objc_slabctl_t *)((char *)slab + (total_buf * buffer_size) - sizeof(objc_slabctl_t));

  void *cur_freebuf = slabctl->freebuf;

  slabctl->freebuf = ((objc_bufctl_t *)(cur_freebuf))->next;
  void *cur_freeslab = cur_freebuf - cache->size;
  cache->c(cur_freeslab, cache->size);
  return cur_freeslab; // current free slab
}
