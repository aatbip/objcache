#include "objcache.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct test {
  int x, y;
} test_t;

void c(void *p, size_t size) {
  test_t *q = p;
  q->x = 33;
  q->y = 22;
}

int main(void) {
  objc_cache_t *cache = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  test_t *p = (test_t *)objc_cache_alloc(cache);
  printf("x: %d\n y: %d\n", p->x, p->y);
  test_t *q = (test_t *)objc_cache_alloc(cache);
  printf("x1: %d\n y1: %d\n", q->x, q->y);

  objc_cache_destroy(cache);
  return 0;
}
