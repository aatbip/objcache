#ifndef OBJC_INTERNAL_H
#define OBJC_INTERNAL_H

#include "objcache.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4 * 1024 // 4K
#define GET_SLABCTL(cache, slab) ((objc_slabctl_t *)((char *)(slab) + (cache)->slabctl_offset))
#define GET_SLABBASE(ptr) ((void *)((uintptr_t)(ptr) & ~(PAGE_SIZE - 1)))

typedef struct __attribute__((__packed__)) objc_bufctl {
  void *next;
  /* For now `constructed` is `uint8_t` which will be changed to bitmap or other better
   * method later for optimized memory uses.
   * `constructed` state is toggled 1 if object is first constructed. This is to check if
   * object is in constructed state during re-allocation after freeing.*/
  uint8_t constructed;
} objc_bufctl_t;

typedef struct objc_slabctl {
  int ref_count;
  objc_bufctl_t *freebuf; // pointer to the bufctl
  uint8_t *bm_const;      // pointer to the bitmap to track constructed state
  struct objc_slabctl *next;
  struct objc_slabctl *prev;
} objc_slabctl_t;

typedef struct objc_cache {
  char *name;
  size_t size;
  int align;
  constructor c;
  destructor d;
  void *free_slab;            // pointer to the start of the slab
  unsigned short buffer_size; // size of obj + bufctl
  unsigned short total_buf;   // total number of buffers that fits in a slab
  size_t slabctl_offset;      // offset where slabctl lives inside the page
  unsigned short unused;      // unused bytes
  unsigned short slab_count;  // number of slabs created in the cache
} objc_cache_t;

#endif // !OBJC_INTERNAL_H
