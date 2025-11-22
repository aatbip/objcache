#include "objcache.h"
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

void create_multiple_slabs(objc_cache_t *cache, objc_cache_info_t cache_info);

int main(void) {
  objc_cache_t *cache = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  objc_cache_info_t cache_info = objc_cache_info(cache);

  printf("--------------------------\n"
         "%-18s %5d B\n"
         "%-18s %5d B\n"
         "%-18s %5d B\n"
         "%-18s %5d  \n"
         "%-18s %5d B\n"
         "--------------------------\n",
         "Size of cache:", cache_info.cache, "Unused:", cache_info.unused, "Size of slabctl:", cache_info.slabctl,
         "Total buf:", cache_info.total_buf, "Size of each buf:", cache_info.buffer_size);

  return 0;
}

/*Allocates object for `cache_info.total_buf * 3 + 1` times i.e. 4 slabs will be created which will form
 * a circular doubly linked list.
 * <-- SLAB 1 <--> SLAB 2 <--> SLAB 3 <--> SLAB 4 -->
 * where, all are empty slabs except for SLAB 4 where only 1 object is allocated.
 * */
void create_multiple_slabs(objc_cache_t *cache, objc_cache_info_t cache_info) {
  for (int i = 0; i < (cache_info.total_buf * 3 + 1); i++) {
    objc_cache_alloc(cache);
  }
}
