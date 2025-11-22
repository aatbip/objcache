#include "objc_internal.h"
#include "objcache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct test {
  int x, y;
  char t;
} test_t;

void c(void *p, size_t size) {
  test_t *q = p;
  q->x = 33;
  q->y = 22;
}

static inline void objc_printf(objc_cache_t *cache) {
  printf("---------------------------\n"
         "%-18s %5zu B\n"
         "%-18s %5d B\n"
         "%-18s %5zu B\n"
         "%-18s %5d  \n"
         "%-18s %5d B\n"
         "%-18s %5d  \n"
         "--------------------------\n\n",
         "Size of cache:", sizeof(*cache), "Unused:", cache->unused, "Size of slabctl:", sizeof(objc_slabctl_t),
         "Total buf:", cache->total_buf, "Size of each buf:", cache->buffer_size, "Slab count:", cache->slab_count);
}

static void test_create_multiple_slabs(objc_cache_t *cache);
static void test_slab_list(objc_cache_t *cache);

int main(void) {
  objc_cache_t *cache1 = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  test_create_multiple_slabs(cache1);

  objc_cache_t *cache2 = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  test_slab_list(cache2);

  return 0;
}

/*Allocates object for `cache_info.total_buf * i + 1` times i.e. 4 slabs will be created which will form
 * a circular doubly linked list.
 * <-- SLAB 1 <--> SLAB 2 <--> SLAB 3 <--> SLAB 4 -->
 * where, all are empty slabs except for SLAB 4 where only 1 object is allocated.
 * */
static void test_create_multiple_slabs(objc_cache_t *cache) {
  uint8_t t = 3;
  for (int i = 0; i < (cache->total_buf * t + 1); i++) {
    objc_cache_alloc(cache);
  }
  unsigned short slab_count = cache->slab_count;
  // slab count should be t + 1
  assert(slab_count == t + 1);
  printf("test_create_multiple_slabs() success\n");
  objc_printf(cache);
  objc_cache_destroy(cache);
}

/*Validate whether the slabs are arranged in a circular doubly linked list.
 * Also validate if the slabs are rearranged correctly in the list after
 * freeing object of the empty/partial slab.*/
static void test_slab_list(objc_cache_t *cache) {
  uint8_t t = 3;
  void *obj_first;
  void *obj_last;
  // allocate objects which creates `t` slabs
  for (int i = 0; i < (cache->total_buf * t); i++) {
    void *p = objc_cache_alloc(cache);

    /*Save the pointer to the first allocated object which belongs to the
     * 1st slab.*/
    if (i == 0) {
      obj_first = p;
    }

    /*Save the pointer to the last allocated object which belongs to the
     * `t`th slab.*/
    if (i == cache->total_buf * t - 1) {
      obj_last = p;
    }
  }
  /*When `cache->total_buf * t` number of allocations are done, it will have to create `t` slabs
   * to fit all allocations since each slab can allocate upto `total_buf` number of objects. So, the above
   * loop will create `t` slabs and allocate objects in it making all the slabs empty slabs i.e. all objects
   * allocated.*/

  /*cur_slabctl is the slab that is currently being pointed by `cache->free_slab`.
   * This slab should be in the tail of the list.*/
  objc_slabctl_t *cur_slabctl = GET_SLABCTL(cache, cache->free_slab);

  /*Verify the circular linked list.
  Since there are 3 slabs, going `prev` 3 times should reach at the cur_slabctl to realize a
   * circular doubly linked list.*/
  assert(cur_slabctl->prev->prev->prev == cur_slabctl);

  /* Calculate the base address of the current slab using `obj` which is the buffer
   * in the current slab. It should be equal to the base address using cur_slabctl.*/
  assert(GET_SLABBASE(obj_last) == GET_SLABBASE(cur_slabctl));

  /* All slabs should be empty i.e. all buffers allocated. Since `cache->total_buf * t` allocations were made, `t`
   * number of slabs should be created and all buffers should be allocated.
   * */
  objc_slabctl_t *cur = cur_slabctl;
  do {
    assert(cur->freebuf == NULL);
    cur = cur->prev;
  } while (cur != cur_slabctl);

  printf("test_slab_list() success\n");
  objc_printf(cache);
  objc_cache_destroy(cache);
}
