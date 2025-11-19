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
  objc_slabctl_t *free_slab; // partial or complete slab
} objc_cache_t;

#define GET_SLABCTL(slab, total_buf, buffer_size)                                                                      \
  ((objc_slabctl_t *)((char *)slab + (total_buf * buffer_size) - sizeof(objc_slabctl_t)));

/*Creates a new slab and adds a freelist linked list*/
static void *create_new_slab(unsigned short total_buf, unsigned short buffer_size, objc_cache_t *cache) {
  void *slab = malloc(PAGE_SIZE);

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
  return slab;
}

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

  /*size of the buffer (buffer_size) = size of the object (cache->size) + size of the bufctl*/
  unsigned short buffer_size = cache->size + sizeof(objc_bufctl_t);
  // printf("buf size: %d\n", buffer_size);

  /*total number of buffers that can be allocated*/
  unsigned short total_buf = (PAGE_SIZE - sizeof(objc_slabctl_t)) / (buffer_size);
  // printf("total buf: %d\n", total_buf);

  /* If a free slab is not available or a slab is not yet created
   * then we create a new empty slab of PAGE_SIZE.*/
  if (!slab) {
    slab = create_new_slab(total_buf, buffer_size, cache);
    objc_slabctl_t *slabctl = GET_SLABCTL(slab, total_buf, buffer_size);

    /*Update the freebuf entry in slabctl to point at the next buffer head*/
    slabctl->freebuf = ((objc_bufctl_t *)(slab + cache->size))->next;
    /*Update next and prev pointers to NULL on the first slab creation*/
    slabctl->next = NULL;
    slabctl->prev = NULL;
    /*Update the ref_count to since an object is allocated*/
    slabctl->ref_count = 1;

    /*Update the free_slab entry in the objc_cache struct*/
    cache->free_slab = slab;

    // run the constructor
    cache->c(slab, cache->size);

    return slab;
  }
  objc_slabctl_t *slabctl = GET_SLABCTL(slab, total_buf, buffer_size);

  void *cur_freebuf = slabctl->freebuf;

  /*If the slab is empty i.e. all buffer are allocated*/
  if (!cur_freebuf) {
    /*A new slab is created which is a complete slab i.e. all buffers are free*/
    void *comp_slab = create_new_slab(total_buf, buffer_size, cache);
    /*slabctl of the new complete slab*/
    objc_slabctl_t *comp_slabctl = GET_SLABCTL(comp_slab, total_buf, buffer_size);

    /*Doubly linked list based updates -
     * Update prev and next of the new complete slab*/
    comp_slabctl->prev = slabctl;
    /*Update next of the tail of doubly linked list to point to the head*/
    comp_slabctl->next = comp_slabctl->prev->next;
    /*Update the next of the previous slab*/
    slabctl->next = comp_slabctl;
    /*Update the head of doubly linked list to point to the tail*/
    comp_slabctl->next->prev = comp_slabctl;

    /*Update the freebuf entry in comp_slabctl to point at the next buffer head*/
    comp_slabctl->freebuf = ((objc_bufctl_t *)(comp_slab + cache->size))->next;

    /*Update the ref_count to since an object is allocated*/
    comp_slabctl->ref_count = 1;

    /*Update the free_slab entry in the objc_cache struct*/
    cache->free_slab = comp_slab;

    // run the constructor
    cache->c(comp_slab, cache->size);

    return comp_slab;
  }

  /*Update the freebuf entry in slabctl to point at the next buffer head*/
  slabctl->freebuf = ((objc_bufctl_t *)(cur_freebuf))->next;
  /*Update the ref_count since an object is allocated*/
  slabctl->ref_count++;
  void *obj = cur_freebuf - cache->size;
  // run the constructor
  cache->c(obj, cache->size);
  return obj;
}
