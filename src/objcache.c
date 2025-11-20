#include "objcache.h"
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4 * 1024 // 4K

typedef struct objc_bufctl {
  void *next;
} objc_bufctl_t;

typedef struct objc_slabctl {
  int ref_count;
  objc_bufctl_t *freebuf; // pointer to the bufctl
  struct objc_slabctl *next;
  struct objc_slabctl *prev;
} objc_slabctl_t;

typedef struct objc_cache {
  char *name;
  size_t size;
  int align;
  constructor c;
  destructor d;
  objc_slabctl_t *free_slab;  // pointer to the start of the slab
  unsigned short buffer_size; // size of obj + bufctl
  unsigned short total_buf;   // total number of buffers that fits in a slab
  size_t slabctl_offset;      // offset where slabctl lives inside the page
  unsigned short unused;      // unused bytes
} objc_cache_t;

#define GET_SLABCTL(cache, slab) ((objc_slabctl_t *)((char *)(slab) + (cache)->slabctl_offset))

/*New slab has to be created in two cases:
 * i. When an object is being allocated for the first time. In this case, `cache->freebuf` is NULL since
 * slab doesn't exist yet.
 * ii. When the previous slab is empty i.e. all buffers are allocated of the slab. In this case, a new slab
 * is created and a circular doubly linked list of a previous slabs is maintained.
 *
 * This function allocates `PAGE_SIZE` sized buffer and initializes all of the members of the struct type
 * `objc_slabctl_t`.*/
static void *create_new_slab(objc_cache_t *cache) {
  void *slab = malloc(PAGE_SIZE);
  if (!slab)
    return NULL;
  objc_slabctl_t *slabctl = GET_SLABCTL(cache, slab);

  char *start = (char *)slab;
  char *end = start + cache->total_buf * cache->buffer_size;

  /*Create a linked list of bufctl*/
  for (char *p = start; p < end; p += cache->buffer_size) {
    objc_bufctl_t *bufctl = (objc_bufctl_t *)(p + cache->size);

    char *next_buf = p + cache->buffer_size + cache->size;

    if (next_buf >= end) {
      bufctl->next = NULL;
    } else {
      bufctl->next = (objc_bufctl_t *)next_buf;
    }
  }

  /* Initialize slab metadata */
  slabctl->ref_count = 0;
  slabctl->freebuf = ((objc_bufctl_t *)(start + cache->size));

  if (!cache->free_slab) {
    /*Slab is being created first time. Both next and prev points to slabctl
     * because only there is only one slab at this point.*/
    slabctl->next = slabctl->prev = slabctl;
  } else {
    /* A new complete slab is being created when previous slab becomes empty
    i.e. all buffers allocated (slabctl->freebuf is NULL)*/

    /*Create a doubly linked list of the slabs.*/
    objc_slabctl_t *prev_slabctl = GET_SLABCTL(cache, cache->free_slab);
    slabctl->next = prev_slabctl->next;
    slabctl->prev = prev_slabctl;
    prev_slabctl->next = slabctl;
    if (prev_slabctl->prev == prev_slabctl) {
      /*For second slab*/
      prev_slabctl->prev = slabctl;
    } else {
      /*From third slab and onwards*/
      slabctl->next->prev = slabctl;
    }
  }
  /*new slab becomes the current partial one*/
  cache->free_slab = slab;

  return slab;
}

/*Checks if a new slab has to be created or not and updates the `freebuf` and `ref_count` members
 * of the struct `objc_slabctl_t` and returns the free object (buffer) `obj` where object can be allocated
 * by running the constructor function.*/
static void *get_obj(objc_cache_t *cache) {

  void *slab = cache->free_slab;

  /*If `slab` is NULL then that means slab is not yet created for the cache. Otherwise, if
   * `GET_SLABCTL(cache, slab)->freebuf` is null that means the current slab is empty (all buffers allocated). So in
   * both the cases, we create a new slab.*/
  if (!slab || !GET_SLABCTL(cache, slab)->freebuf) {
    slab = create_new_slab(cache);
    if (!slab)
      return NULL;
  }

  objc_slabctl_t *slabctl = GET_SLABCTL(cache, slab);

  /*Get the current free bufctl*/
  objc_bufctl_t *cur_freebuf = slabctl->freebuf;
  /*Update freebuf to point to the next bufctl*/
  slabctl->freebuf = cur_freebuf->next;

  void *obj = (void *)((char *)cur_freebuf - cache->size);

  slabctl->ref_count++;

  return obj;
}

objc_cache_t *objc_cache_create(char *name, size_t size, int align, constructor c, destructor d) {
  objc_cache_t *cache = (objc_cache_t *)malloc(sizeof(*cache));

  if (!cache)
    return NULL;

  cache->name = name;
  cache->size = size;
  cache->align = align;
  cache->c = c;
  cache->d = d;
  cache->free_slab = NULL; // NULL because slab is not created yet
  cache->buffer_size = size + sizeof(objc_bufctl_t);
  cache->total_buf = (PAGE_SIZE - sizeof(objc_slabctl_t)) / (cache->buffer_size);

  cache->unused = (PAGE_SIZE - sizeof(objc_slabctl_t)) % (cache->buffer_size);

  cache->slabctl_offset = cache->total_buf * cache->buffer_size + cache->unused;

  printf("cache: %zu\n unused: %d\n sizeof slabctl: %zu\n total buf: %d\n buffer size: %d\n offset: %zu\n",
         sizeof(*cache), cache->unused, sizeof(objc_slabctl_t), cache->total_buf, cache->buffer_size,
         cache->slabctl_offset);

  return cache;
}

/*This function runs the constructor to allocate the object into the slab buffer.*/
void *objc_cache_alloc(objc_cache_t *cache) {
  void *obj = get_obj(cache);

  if (!obj || !cache)
    return NULL;

  // run the constructor
  cache->c(obj, cache->size);

  return obj;
}

void objc_cache_destroy(objc_cache_t *cache) {
  free(cache->free_slab);
  free(cache);
}
