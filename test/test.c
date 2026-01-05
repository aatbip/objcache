#include "objc_internal.h"
#include "objcache.h"
#include <assert.h>
#include <stddef.h>
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

typedef struct test1 {
  char t;
  test_t r;
} test_t1;

void c1(void *p, size_t size) {
  test_t1 *q = p;
  q->t = 'y';
  q->r.x = 22;
  q->r.y = 22;
  q->r.t = q->t;
}

typedef struct test_bm {
  int x, y;
} test_bm_t;

static int total_c1_runs = 0;
void c2(void *p, size_t size) {
  test_bm_t *q = p;
  q->x = 33;
  q->y = 22;
  total_c1_runs++;
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
static void test_slab_bm(objc_cache_t *cache);

int main(void) {
  objc_cache_t *cache1 = objc_cache_create("rand", sizeof(test_t), 0, c, NULL);
  test_create_multiple_slabs(cache1);

  objc_cache_t *cache2 = objc_cache_create("rand1", sizeof(test_t1), 0, c1, NULL);
  test_slab_list(cache2);

  objc_cache_t *cache3 = objc_cache_create("rand2", sizeof(test_bm_t), 0, c2, NULL);
  test_slab_bm(cache3);

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
  void *obj_last;
  void **obj_slab1 = malloc(cache->total_buf * sizeof(void *));
  void *obj_first;
  // allocate objects which creates `t` slabs
  for (int i = 0; i < (cache->total_buf * t); i++) {
    void *p = objc_cache_alloc(cache);

    if (i < cache->total_buf) {
      obj_slab1[i] = p;
    }

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

  /*Freeing an object from empty slab should link the slab to the tail because empty slabs reside in the beginning of
   * the list then comes partial slabs followed by complete slabs.*/
  objc_free(cache, obj_first);
  // Slab from which object was just freed to become partial slab
  objc_slabctl_t *partial_slab = GET_SLABCTL(cache, obj_first);
  /*If the slab from where object was just freed is moved to the tail then the previous slabs
   * should be empty slab which can also be verified if ref_count == total_buf.*/
  assert(partial_slab->prev->ref_count == cache->total_buf);
  assert(partial_slab->prev->prev->ref_count == cache->total_buf);
  /*`freebuf` shouldn't be NULL because an object is just freed.*/
  assert(partial_slab->freebuf != NULL);

  /*If partial/complete slab exists then new slab shouldn't be created during new allocations. Allocator should first
   * search for the partial/complete slab and use it.*/
  void *newobj = objc_cache_alloc(cache);
  /*`newobj` address and `obj_first` address should be same if allocation has happened in the partial slab free
   * buffer. Slab count should also not change from `t`.*/
  assert(newobj == obj_first);
  assert(cache->slab_count == t);
  /*`freebuf` should be NULL because an object is just allocated.*/
  assert(partial_slab->freebuf == NULL);

  /*Allocating again should create a new slab since no partial/complete slabs are remaining.*/
  objc_cache_alloc(cache);
  assert(cache->slab_count == t + 1);
  /*Now `cache->freeslab` should point at this new slab and it should have `ref_count=1`*/
  assert(GET_SLABCTL(cache, cache->free_slab)->ref_count == 1);

  /* Cache structure at this point -
   *
   * --------      ---------      ---------        ----------
   *  SLAB 1  <-->   SLAB 2  <-->   SLAB 3   <-->    SLAB 4
   * --------      ---------      ---------        ----------
   *                                               ^
   *                                               '
   *                                               '
   *                                               `- cache->free_slab
   * SLAB 1, SLAB 2 and SLAB 3 are empty.
   * SLAB 4 is partial slab with ref_count = 1 and cache->free_slab points at SLAB 4.
   *
   */

  /*Free objc_slab1[0] which belongs to SLAB 3 in the list now. Recall, SLAB 3 moved to the tail from the head when
   * an object from the slab was freed earlier in objc_free(cache, obj_first); */
  objc_free(cache, obj_slab1[0]);
  /*Now `cache->free_slab` should point to the base of SLAB 3 i.e. obj_slab1[0].*/
  assert(cache->free_slab == obj_slab1[0]);
  /* Cache structure at this point -
   *
   * --------      ---------      ---------        ----------
   *  SLAB 1  <-->   SLAB 2  <-->   SLAB 3   <-->    SLAB 4
   * --------      ---------      ---------        ----------
   *                              ^
   *                              '
   *                              '
   *                              `- cache->free_slab
   * SLAB 1, SLAB 2 are empty.
   * SLAB 3 is partial slab with only one free_buf left and cache->free_slab points at SLAB 3.
   * SLAB 4 is partial slab with ref_count = 1.
   *
   */

  /*Allocates in SLAB 3 because it has one free_buf left.*/
  objc_cache_alloc(cache);
  /*Allocates in SLAB 4 and makes ref_count = 2.*/
  objc_cache_alloc(cache);
  assert(GET_SLABCTL(cache, cache->free_slab)->ref_count == 2);

  /*Freeing all objects of the slab should bring it to the tail since complete slab (i.e. all objects free) resides
   * towards the tail.*/
  for (int i = 0; i < cache->total_buf; i++) {
    objc_free(cache, obj_slab1[i]);
  }
  assert(GET_SLABCTL(cache, cache->free_slab)->next->ref_count == 0);
  assert(GET_SLABBASE(GET_SLABCTL(cache, cache->free_slab)->next) == obj_slab1[0]);

  printf("test_slab_list() success\n");
  objc_printf(cache);
  objc_cache_destroy(cache);
  free(obj_slab1);
}

void test_slab_bm(objc_cache_t *cache) {
  void **obj_arr = malloc(sizeof(void *) * cache->total_buf);
  for (int i = 0; i < cache->total_buf; i++) {
    void *obj = objc_cache_alloc(cache);
    obj_arr[i] = obj;
  }
  assert(GET_SLABCTL(cache, cache->free_slab)->ref_count == cache->total_buf);
  assert(GET_SLABCTL(cache, cache->free_slab)->freebuf == NULL);

  int idx = 50; // let's free 50th buffer from the slab

  objc_free(cache, obj_arr[idx]);
  void *freed_obj = obj_arr[idx];
  assert(((char *)freed_obj - (char *)GET_SLABBASE(freed_obj)) / cache->buffer_size == idx);

  void *obj = objc_cache_alloc(cache);
  assert(freed_obj == obj);

  // constructor should run exactly `cache->total_buf` times if it doesn't run twice
  assert(total_c1_runs == cache->total_buf);

  printf("test_slab_bm() success\n");
  objc_printf(cache);
  objc_cache_destroy(cache);
  free(obj_arr);
}
