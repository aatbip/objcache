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

static void test_create_multiple_slabs(objc_cache_t *cache);
static void test_slab_list(objc_cache_t *cache);

int main(void) {
  objc_cache_t *cache = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);

  printf("--------------------------\n"
         "%-18s %5zu B\n"
         "%-18s %5d B\n"
         "%-18s %5zu B\n"
         "%-18s %5d  \n"
         "%-18s %5d B\n"
         "%-18s %5d  \n"
         "--------------------------\n",
         "Size of cache:", sizeof(*cache), "Unused:", cache->unused, "Size of slabctl:", sizeof(objc_slabctl_t),
         "Total buf:", cache->total_buf, "Size of each buf:", cache->buffer_size, "Slab count:", cache->slab_count);

  test_create_multiple_slabs(cache);

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
  objc_cache_destroy(cache);
}
