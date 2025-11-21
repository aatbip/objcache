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
#define GET_SLABBASE(ptr) ((void *)((uintptr_t)(ptr) & ~(PAGE_SIZE - 1)))

/*New slab has to be created in two cases:
 * i. When an object is being allocated for the first time. In this case, `cache->freebuf` is NULL since
 * slab doesn't exist yet.
 * ii. When the previous slab is empty i.e. all buffers are allocated of the slab. In this case, a new slab
 * is created and a circular doubly linked list of a previous slabs is maintained.
 *
 * This function allocates `PAGE_SIZE` sized buffer and initializes all of the members of the struct type
 * `objc_slabctl_t`.*/
static void *create_new_slab(objc_cache_t *cache) {
  void *slab;
  /*Using posix_memalign for now to always align the slab in page sized units. Alignment is needed
   * to calculate the base of the slab. This will be updated to use the `mmap` after I visit this syscall
   * real soon!*/
  posix_memalign(&slab, PAGE_SIZE, PAGE_SIZE);
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
  /*-------TEST CODE--------*/
  static int count = 0;
  count++;
  printf("count: %d\n", count);
  /*----------------------*/
  return slab;
}

/*Go thorugh the slab doubly linked list and return slab base address if a partial slab is found.*/
static void *find_partial_slab(objc_cache_t *cache, void *slab) {
  objc_slabctl_t *start = GET_SLABCTL(cache, slab);
  objc_slabctl_t *cur = start->next;
  while (cur != start) {
    if (cur->freebuf != NULL) {
      /*If next slab in the list have free buffer then we return this slab base.*/
      return GET_SLABBASE(cur);
    }
    cur = cur->next;
  }
  return NULL;
}

/*Checks if a new slab has to be created or not and updates the `freebuf` and `ref_count` members
 * of the struct `objc_slabctl_t` and returns the free object (buffer) `obj` where object can be allocated
 * by running the constructor function.*/
static void *get_obj(objc_cache_t *cache) {

  void *slab = cache->free_slab;

  /*If `slab` is NULL then that means slab is not yet created for the cache. In this case, we directly
   * create a new slab.*/
  if (!slab) {
    slab = create_new_slab(cache);
    if (!slab)
      return NULL;
  }

  /* If `GET_SLABCTL(cache, slab)->freebuf` is null that means the current slab is empty (all buffers allocated). In
   * this case, we first go through the slab list to check for a partially free slab i.e. `slabctl->freebuf != NULL`.
   * If no partially free slab is found after one complete round through the circular linked list then we create a new
   * slab. If partially free slab is found, then we update the slab pointer to this partially free slab.*/
  if (!GET_SLABCTL(cache, slab)->freebuf) {
    void *partial_slab = find_partial_slab(cache, slab);
    slab = partial_slab ? partial_slab : create_new_slab(cache);
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

  printf("cache: %zu\nunused: %d\nsizeofslabctl: %zu\ntotal buf: %d\nbuffer size: %d\noffset: %zu\n", sizeof(*cache),
         cache->unused, sizeof(objc_slabctl_t), cache->total_buf, cache->buffer_size, cache->slabctl_offset);

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

void objc_free(objc_cache_t *cache, void *obj) {
  objc_bufctl_t *bufctl = (objc_bufctl_t *)(char *)obj + cache->size;
  /*Get slab base address using bit mask*/
  void *slab = GET_SLABBASE(obj);
  objc_slabctl_t *slabctl = GET_SLABCTL(cache, slab);

  /*Put the `bufctl` of the freed `obj` back in the free linked list*/
  objc_bufctl_t *temp_bufctl = slabctl->freebuf;
  slabctl->freebuf = bufctl;
  bufctl->next = temp_bufctl;

  slabctl->ref_count--;

  if (slabctl->next == slabctl)
    // do nothing if only one slab exists
    return;

  /*If more then one slab exists.*/
  if (slabctl->next->freebuf != NULL && slabctl->next->ref_count > 0) { // complete slab check might not needed?
    /*If the next slab is not empty slab i.e. all buffer allocated and if the next slab
     * is not also a complete slab i.e. all buffer free (in which case ref_count == 0), it
     * means the next slab is a partial slab. So if the next buffer is the partial slab
     * then we can point the current `cache->free_slab` to this slab from where obj is just
     * freed.*/
    cache->free_slab = slab;
    return;
  }
  objc_slabctl_t *cur = slabctl->next;
  /*Iterate until a partial or complete slab is found.*/
  while (cur->freebuf != NULL) {
    cur = cur->next;
  }
  /*If cur is the partial/complete slab then `cur->prev` is the last empty slab. We need to
   * rearrange the next and prev pointers of the `slabctl` (from which obj is being freed) with
   * this last empty slab so that empty slab moves toward the head and partial/complete slabs
   * follow.*/
  objc_slabctl_t *empty_slab = cur->prev;
  // unlink the slab
  slabctl->prev->next = slabctl->next;
  slabctl->next->prev = slabctl->prev;

  // place the slab after the last empty slab
  slabctl->next = empty_slab->next;
  slabctl->prev = empty_slab;
  empty_slab->next->prev = slabctl;
  empty_slab->next = slabctl;

  cache->free_slab = slab;
}

/*This function is incomplete!!*/
void objc_cache_destroy(objc_cache_t *cache) {
  free(cache->free_slab);
  free(cache);
}

objc_cache_info_t objc_cache_info(objc_cache_t *cache) {
  objc_cache_info_t cache_info = {.cache = sizeof(*cache),
                                  .unused = cache->unused,
                                  .slabctl = sizeof(objc_slabctl_t),
                                  .buffer_size = cache->buffer_size,
                                  .total_buf = cache->total_buf};
  return cache_info;
}
