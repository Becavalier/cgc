#ifndef	_CGC_H
#define	_CGC_H

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#define UNTAG(p) (((uint64_t) (p)) & 0xfffffffffffffffc)
#define HEADER_SIZE sizeof(header_t)

typedef struct header {
  uint64_t size;  // Block size, include the header (multiple of headers) - 8 bytes.
  struct header *next;  // Pointer to the next free block - 8 bytes.
} header_t;

static header_t *freep = NULL;   // Points to first free block of memory.
static header_t *usedp = NULL;  // Points to first used block of memory.

// Scan the free list and look for a place to put the block at.
static void insert_free_list(header_t *bp) {
  if (freep == NULL) {
    freep = bp;
  } else {
    // Search the whole list to find the place.
    header_t *p = freep;
    if (bp < p) {
      if (bp + bp->size == p) {
        bp->size += p->size;
        bp->next = p->next;
      } else {
        bp->next = p;
      }
      freep = bp;
      return;
    }
    while (true) {
      if (p == NULL) break;
      if (bp > p) {
        if (bp < p->next) {
          if (bp + bp->size == p->next) {
            bp->size += p->next->size;
            bp->next = p->next->next;
          } else {
            bp->next = p->next;
          }
        }
        if (p + p->size == bp) {
          p->next = bp->next;
          p->size += bp->size;
        } else {
          p->next = bp;
        }
        return;
      }
      p = p->next;
    }
  }
}

// Request more memory from the kernel.
static header_t* allocate(size_t num_units) {
  void* vp;
  const size_t page_size = sysconf(_SC_PAGESIZE);
  const size_t allocated_bytes = ((num_units * HEADER_SIZE + page_size - 1) / page_size) * page_size;
  if ((vp = sbrk(allocated_bytes)) == (void*) -1)
    return NULL;

  header_t *up = (header_t*) vp;
  up->size = allocated_bytes / HEADER_SIZE;
  insert_free_list(up);
  return up;
}

// Find a chunk from the free list and put it in the used list (first fit).
void* gc_malloc(size_t alloc_size) {
  const size_t alloc_units = (alloc_size + HEADER_SIZE - 1) / HEADER_SIZE + 1;

  header_t *p = freep;
  header_t *prevp = NULL;
  while (true) {
    if (p == NULL) {
      p = allocate(alloc_units);
      if (p == NULL) return NULL;
    }
    if (p->size >= alloc_units) {
      if (p->size == alloc_units) {
        if (prevp == NULL) {
          freep = p->next;
        } else {
          prevp->next = p->next;
        }
      } else {
        p->size -= alloc_units;
        p += p->size;
        p->size = alloc_units;
      }
      // Add picked block to the used list.
      if (usedp == NULL) {
        p->next = NULL;
        usedp = p;
      } else {
        p->next = usedp;
        usedp = p;
      }
      return (void*) (p + 1);  // Skip header.
    }
  }
}

// Mark any items in the used list.
static void scan_region(void* sp, void* end) {
  header_t* bp;
  for (; sp < end; sp++) {
    uint64_t v = *(uint64_t*) sp;
    bp = UNTAG(usedp);
    while (bp != NULL) {
      if (v >= bp + 1 && v < bp + 1 + bp->size) {
        bp->next = ((uint64_t) bp->next) | 1;  // Mark the current block.
        break;
      }
      bp = UNTAG(bp->next);
    }
  }
}

/**
 * Scanning Areas:
 * 1. BSS segment.
 * 2. Data segment.
 * 3. Stack.
 * 4. Heap (usedp).
*/

static void scan_heap(void) {
  void* vp;
  header_t *bp, *up;
  for (bp = UNTAG(usedp); bp != NULL; bp = UNTAG(bp->next)) {
    if (!((uint64_t) bp->next & 1)) {  // Find the marked block.
      continue;
    }
    for (vp = (void*) (bp + 1); vp < (bp + bp->size + 1); vp++) {
      uint64_t v = *(uint64_t*)vp;
      up = UNTAG(usedp);
      while (up != NULL) {
        if (up != bp && v >= up + 1 && v < up + 1 + up->size) {
          up->next = ((uint64_t) up->next) | 1;
          break;
        }
        up = UNTAG(up->next);
      }
    }
  }
}

/**
 
           (For most Unix linkers)
  High  ------------------------------
        |           stack ↓          |  
        ------------------------------
        |                            |  
        ------------------------------
        |            heap ↑          | 
        ------------------------------  end (start of heap)
        |  bss (uninitialized data)  | 
        ------------------------------  edata 
        |   data (initialized data)  |  
        ------------------------------  etext (start of data)
        |            text            | 
  Low   ------------------------------

*/

void GC_init(char** argv) {
  static int initted;
  if (initted) return;

  initted = 1;

}

void GC_collect(char** argv) {
  
}


#endif	/* _CGC_H */
