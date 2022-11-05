
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/off_t.h"

/* max size of process stack*/
#define STACK_MAX_SIZE 8 * (1024 * 1024) /* 8 Megabytes */

/* Current status of the page: */
enum pstatus {
  ZERO_PAGE,        /* All zero page */
  HAS_FRAME,         /* Is present in physical memory */
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
};


bool handle_page_fault(void* fault_addr); /* called in exception.c*/

struct page* page_create_from_filesys (void *upage, bool writable, struct file* file, off_t file_offset,
                                       uint32_t read_bytes, uint32_t zero_bytes);

struct page* page_create_zeropage (void *upage);

struct page* page_get (const void* vaddr);

hash_hash_func page_hash_func;
hash_less_func page_less_func;
#endif /* vm/page.h */
