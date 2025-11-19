#include "objcache.h"
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4 * 1024 // 4K

typedef struct objc_bufctl {
  void *next;
} objc_bufctl_t;

typedef struct objc_slabctl {
  int ref_count;
  objc_bufctl_t *freebuf; // head of the bufctl
  struct objc_slabctl *next;
  struct objc_slabctl *prev;
} objc_slabctl_t;

typedef struct objc_cache {
  char *name;
  size_t size;
  int align;
  constructor c;
  destructor d;
  objc_slabctl_t *free_slab;  // pointer to the slab
  unsigned short buffer_size; // size of obj + bufctl
  unsigned short total_buf;   // total number of buffers that fits in a slab
} objc_cache_t;

#define GET_SLABCTL(cache, slab)                                                                                       \
  slab ? ((objc_slabctl_t *)((char *)slab + (cache->total_buf * cache->buffer_size) - sizeof(objc_slabctl_t))) : NULL;

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
  objc_slabctl_t *slabctl = GET_SLABCTL(cache, slab);

  /*fragmented bytes*/
  // unsigned short unused =
  //     (PAGE_SIZE - sizeof(objc_slabctl_t)) % (buffer_size);

  /*Create a linked list of free buffers. bufctl->next points to the next buffer.*/
  for (char *p = slab; p < (char *)slab + cache->total_buf * cache->buffer_size; p += cache->buffer_size) {
    objc_bufctl_t *bufctl = (objc_bufctl_t *)(p + cache->size);
    char *next_buf = p + cache->buffer_size;
    if (next_buf >= (char *)slab + cache->total_buf * cache->buffer_size) {
      bufctl->next = NULL;
    } else {
      bufctl->next = (objc_bufctl_t *)next_buf;
    }
  }

  /*Slab is being created first time.*/
  if (!cache->free_slab) {
    /*Update next and prev pointers to NULL on the first slab creation*/
    slabctl->next = slabctl;
    slabctl->prev = slabctl;
  } else { /* A new complete slab is being created when previous slab becomes empty
  i.e. all buffers allocated (slabctl->freebuf is NULL)*/
    objc_slabctl_t *prev_slab = cache->free_slab;
    /*Doubly linked list based updates -
     * Update prev and next of the new complete slab*/
    slabctl->prev = prev_slab;
    /*Update next of the tail of doubly linked list to point to the head*/
    slabctl->next = prev_slab->prev->next;
    /*Update the next of the previous slab*/
    prev_slab->next = slabctl;
    /*Update the head of doubly linked list to point to the tail*/
    prev_slab->next->prev = slabctl;
  }

  /*Initialize `ref_count`*/
  slabctl->ref_count = 0;
  /*Update the freebuf entry in slabctl to point at the head of bufctl.*/
  slabctl->freebuf = ((objc_bufctl_t *)((char *)slab + cache->size));

  /*Update `free_slab` entry of the current cache to point at the new slab.*/
  cache->free_slab = slab;

  return slab;
}

/*Checks if a new slab has to be created or not and updates the `freebuf` and `ref_count` members
 * of the struct `objc_slabctl_t` and returns the free object (buffer) `obj` where object can be allocated
 * by running the constructor function.*/
static void *get_obj(objc_cache_t *cache) {

  void *slab = cache->free_slab;

  objc_slabctl_t *slabctl = GET_SLABCTL(cache, slab); // slabctl can be NULL is slab is NULL

  void *cur_freebuf = slabctl ? slabctl->freebuf : NULL;

  /*If `slab` is NULL then that means slab is not yet created for the cache. Otherwise, if `cur_freebuf` is null that
   * means the current slab is empty (all buffers allocated). So in both the cases, we create a new slab.*/
  if (!slab || !cur_freebuf) {
    slab = create_new_slab(cache);
    slabctl = GET_SLABCTL(cache, slab);
    cur_freebuf = slabctl->freebuf;
  }

  /*Update the freebuf entry in slabctl to point at the next buffer head*/
  slabctl->freebuf = ((objc_bufctl_t *)(cur_freebuf))->next;
  void *obj = cur_freebuf - cache->size;
  /*Update the ref_count to since an object is allocated*/
  slabctl->ref_count++;

  return obj;
}

objc_cache_t *objc_cache_create(char *name, size_t size, int align, constructor c, destructor d) {
  objc_cache_t *cache = (objc_cache_t *)malloc(sizeof(*cache));
  cache->name = name;
  cache->size = size;
  cache->align = align;
  cache->c = c;
  cache->d = d;
  cache->free_slab = NULL; // slab is not created yet
  cache->buffer_size = cache->size + sizeof(objc_bufctl_t);
  cache->total_buf = (PAGE_SIZE - sizeof(objc_slabctl_t)) / (cache->buffer_size);

  return cache;
}

/*This function runs the constructor to allocate the object into the slab buffer.*/
void *objc_cache_alloc(objc_cache_t *cache) {
  void *obj = get_obj(cache);

  // run the constructor
  cache->c(obj, cache->size);

  return obj;
}

void objc_cache_destroy(objc_cache_t *cache) {
  free(cache->free_slab);
  free(cache);
}
