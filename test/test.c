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

int main(void) {
  // objc_cache_t *cache = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  // test_t *p = (test_t *)objc_cache_alloc(cache);
  // printf("x: %d\n y: %d\n", p->x, p->y);
  // test_t *q = (test_t *)objc_cache_alloc(cache);
  // printf("x1: %d\n y1: %d\n", q->x, q->y);

  objc_cache_t *cache = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  test_t *temp;
  for (int i = 0; i < 435; i++) {
    test_t *p = (test_t *)objc_cache_alloc(cache);
    if (i == 0) {
      temp = p;
    }
  }
  objc_free(cache, temp);
  printf("temp addr: %p\n", temp);

  // test_t *p = (test_t *)objc_cache_alloc(cache);
  // printf("addr p: %p\n", p);
  // objc_cache_alloc(cache);
  // objc_cache_alloc(cache);
  // objc_cache_alloc(cache);
  // objc_free(cache, p);
  // test_t *q = (test_t *)objc_cache_alloc(cache);
  // printf("addr q: %p\n", q);

  objc_cache_destroy(cache);

  objc_cache_info_t cache_info = objc_cache_info(cache);

  printf("\ncache: %d\nunused: %d\nsizeof slabctl: %d\ntotal buf: %d\nbuffer size: %d\n", cache_info.cache,
         cache_info.unused, cache_info.slabctl, cache_info.total_buf, cache_info.buffer_size);
  return 0;
}
