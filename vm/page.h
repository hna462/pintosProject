
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/off_t.h"

/* max size of process stack*/
#define STACK_MAX_SIZE 8 * (1024 * 1024) /* 8 Megabytes */

struct page
{
    void *vaddr;            /* user virtual address */
    bool writable;          /* writable or read-only */
    struct thread *thread;  /* thread that owns the page */
    struct hash_elem hash_elem; /* thread's `hash_table' hash element. */

    struct file *file;          /* File */
    off_t file_offset;          /* File Offset */
    uint32_t file_bytes;         /* Bytes to read/write */
    uint32_t zero_bytes;         /* File zero bytes*/
};


bool handle_page_fault(void* fault_addr); /* called in exception.c*/
struct page* create_page (void *vaddr, bool writable);
struct page* get_page (const void* vaddr);

hash_hash_func page_hash_func;
hash_less_func page_less_func;
#endif /* vm/page.h */
