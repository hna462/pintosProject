
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/off_t.h"

/* max size of process stack*/
#define STACK_MAX_SIZE 8 * (1024 * 1024) /* 8 Megabytes */

/* Current status of the page: */
enum pstatus {
  ZERO_PAGE,        /* All zero page */
  FROM_FRAME,         /* Is present in physical memory */
  ON_SWAP,          /* Is in swap memory */
  FROM_FILE,           /* Came from filesys */
};

struct page
{
    void *upage;            /* user virtual address */
    void *kpage;            /* physical frame address. NULL if not HAS_FRAME */
    bool writable;          /* writable or read-only */
    struct thread *thread;  /* thread that owns the page */
    struct hash_elem hash_elem; /* thread's `hash_table' hash element. */

    struct file *file;          /* File */
    off_t file_offset;          /* File Offset */
    uint32_t file_bytes;         /* Bytes to read/write */
    uint32_t zero_bytes;         /* File zero bytes*/

    enum pstatus pstatus;
    size_t swap_slot;           /* swap slot index in the swap_bitmap */

    bool has_frame;
};


bool handle_page_fault(void* fault_addr); /* called in exception.c*/

bool page_create (void *upage, enum pstatus starting_status, void *aux);

struct page* page_get (const void* vaddr);

bool page_set_on_swap(const void* upage, size_t swap_slot);

bool preload_multiple_pages_and_pin (const void *start_addr, size_t size);
void unpin_multiple_pages(const void *start_addr, size_t size);

void clear_page_table();

hash_hash_func page_hash_func;
hash_less_func page_less_func;
#endif /* vm/page.h */
