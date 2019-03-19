#include <common.h>
#include <klib.h>
#include <slab.h>
#include <lock.h>

volatile int exclusion = 0;

static void pmm_init() {
  Log("HEAP AREA: [%p, %p)", _heap.start, _heap.end);
  kmem_init(_heap.start, _heap.end);
  Log("INIT PASSED.");
}

static void *kalloc(size_t size) {
  Assert(((struct kmem_item *)0x201070)->used == false, "201070 is used!");
  lock(&exclusion);
  Assert(((struct kmem_item *)0x201070)->used == false, "201070 is used!");
  struct kmem_cache *cp = kmem_cache_create(size);
  void *ret = kmem_cache_alloc(cp);
  Assert(ret, "MALLOC RETURNED NULL");
  Assert(((struct kmem_item *)0x201070)->used == false, "201070 is used!");
  Assert(ret >= _heap.start && ret <= _heap.end, "MALLOC NOT IN HEAP AREA");
  unlock(&exclusion);
  return ret;
}

static void kfree(void *ptr) {
  lock(&exclusion);
  kmem_cache_free(ptr);
  unlock(&exclusion);
}

MODULE_DEF(pmm) {
  .init = pmm_init,
  .alloc = kalloc,
  .free = kfree,
};
