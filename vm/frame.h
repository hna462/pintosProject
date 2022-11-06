#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include <hash.h>
#include <list.h>

struct frame 
{
    struct thread* thread;      /* Owner */
    void *kpage;                /* Kernel physical address */
    void *upage;                /* User Virtual Memory Address */
    // struct lock lock;           /* Lock for each frame table entry */
    bool pinned;            /* Pinned frame cannot be evicted */
    struct list_elem list_elem;    /* Linked List elem */
    struct hash_elem hash_elem;     /* Hash Table elem  */
};
void frame_table_init ();
void * frame_allocate (enum palloc_flags flags, void *upage);
void frame_free(void *kpage, bool free_kpage, bool with_lock);

void frame_pin(void *kpage);
void frame_unpin(void *kpage);

#endif /* vm/frame.h */