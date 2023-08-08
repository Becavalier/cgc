#ifndef	_CGC_H
#define	_CGC_H

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#if defined(__linux__)
#include <assert.h>
#endif

#define vpointer_t unsigned long
#define UNTAG_HEADER(p) (header_t*)(((vpointer_t) (p)) & 0xfffffffffffffffc)
#define HEADER_SIZE sizeof(header_t)

typedef struct header {
  size_t size;  // Block size, include the header (multiple of headers) - 8 bytes.
  struct header *next;  // Pointer to the next free block - 8 bytes.
} header_t;

static void* stack_bottom;
static header_t *freep = NULL, *usedp = NULL;   // Points to first free/used block of memory.

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
        if (bp + bp->size == p->next) {
          bp->size += p->next->size;
          bp->next = p->next->next;
        } else {
          bp->next = p->next;
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
  void* offset_end = end - sizeof(vpointer_t);
  for (; sp <= offset_end; sp++) {
    void* v = *(vpointer_t*)sp;
    bp = UNTAG_HEADER(usedp);
    while (bp != NULL) {
      if (v >= bp + 1 && v < bp + 1 + bp->size) {
        bp->next = (header_t*) (((vpointer_t) bp->next) | 1);  // Mark in used blocks.
        break;
      }
      bp = UNTAG_HEADER(bp->next);
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
  for (bp = UNTAG_HEADER(usedp); bp != NULL; bp = UNTAG_HEADER(bp->next)) {
    if (!((vpointer_t) bp->next & 1)) {  // Find the marked block.
      continue;
    }
    void* offset_end = (void*)((size_t)(bp + bp->size + 1) - sizeof(vpointer_t));
    for (vp = (void*) (bp + 1); vp <= offset_end; vp++) {
      void* v = *(vpointer_t*)vp;
      up = UNTAG_HEADER(usedp);
      while (up != NULL) {
        if (up != bp && v >= up + 1 && v < up + 1 + up->size) {
          up->next = (header_t*) (((vpointer_t) up->next) | 1);
          break;
        }
        up = UNTAG_HEADER(up->next);
      }
    }
  }
}

void gc_init(void *sb) {
  static bool gc_initted;
  if (!gc_initted) {
#if defined(__linux__)
    FILE *statfp = fopen("/proc/self/stat", "r");
    assert(statfp != NULL);
    vpointer_t stack_bottom_v = 0;
    fscanf(statfp, 
      "%*d "  // pid.
      "%*s "  // tcomm.
      "%*c "  // state.
      "%*d "  // ppid.
      "%*d "  // pgid.
      "%*d "  // sid.
      "%*d "  // tty_nr.
      "%*d "  // tty_pgrp.
      "%*u "  // flags.
      "%*lu "  // min_flt.
      "%*lu "  // cmin_flt.
      "%*lu "  // maj_flt.
      "%*lu "  // cmaj_flt.
      "%*lu "  // utime.
      "%*lu "  // stime.
      "%*ld "  // cutime.
      "%*ld "  // cstime.
      "%*ld "  // priority.
      "%*ld "  // nice.
      "%*ld "  // num_threads.
      "%*ld "  // it_real_value.
      "%*llu "  // start_time.
      "%*lu "  // vsize.
      "%*ld "  // rss.
      "%*lu "  // rsslim.
      "%*lu "  // start_code.
      "%*lu "  // end_code.
      "%lu",  // start_stack.  <-
      &stack_bottom_v);
    stack_bottom = (void*) stack_bottom_v;
    fclose(statfp);
#else
    // "&argc" will be copied into the current stack, -
    // and we use its address as the bottom of the program stack.
    stack_bottom = sb;  
#endif
    gc_initted = true;
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

void sweep(void) {
  header_t *prevp = NULL, *tp = NULL, *p = usedp;
  while (true) {
    if (p == NULL) return;
    if (!((vpointer_t) p->next & 1)) {
      // Free the unused block.
      tp = p;
      p = UNTAG_HEADER(p->next);
      insert_free_list(tp);
      if (prevp == NULL) {
        usedp = p;
      } else {
        prevp->next = p;
      }
    } else {
      p->next = (header_t*)(((vpointer_t) p->next) & ~1);
      prevp = p;
      p = UNTAG_HEADER(p->next);
    }
  }
}

void gc_collect(void) {
  void* stack_top;

#if defined(__linux__)
  extern char end, etext;
  void* data_end = (void*) &end;
  void* data_start = (void*) &etext;
#endif

  if (usedp == NULL) return;

#if defined(__linux__)
  // Scan data area (static and global variables).
  printf("Scan .bss & .data:\n - start: %p\n - end: %p\n", data_start, data_end);
  scan_region(data_start, data_end);
#endif

  // Scan program stack (include GPRs).
  printf("Scan stack:\n - stack_top: %p\n - stack_bottom: %p\n", stack_top, stack_bottom);
  asm volatile (
    "pushfq\n\t" 
    "movq %%rsp, %0" 
    : "=r" (stack_top));
  scan_region(stack_top, stack_bottom);
  asm volatile ("popfq");

  // Scan program heap.
  scan_heap();

  // Sweep.
  sweep();
}


#endif	/* _CGC_H */
