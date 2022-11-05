#include "vm/frame.h"

#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>

static struct lock frame_lock;
static struct list frame_list; 
static struct hash frame_table;


static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool  frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

void
init_frame_table ()
{
  lock_init (&frame_lock);
  list_init (&frame_list);
  hash_init (&frame_table, frame_hash_func, frame_less_func, NULL);
}

/* Get frame from the frame table by it's kernel page */
struct frame*
get_frame (const void* kpage){
    struct frame temp_frame;
    temp_frame.kpage = kpage;
    struct hash_elem *h = hash_find (&frame_table, &(temp_frame.hash_elem));
    if (h == NULL) {
        PANIC ("Tried getting frame that's not stored in the table");
    }
    else {
        return hash_entry(h, struct frame, hash_elem);
    }
}

void * allocate_frame (enum palloc_flags flags, void *upage){

    lock_acquire(&frame_lock);

    void* kpage = palloc_get_page (PAL_USER | flags);
    if (kpage == NULL){
        //TODO eviction.
        return kpage;
    }

    struct frame *frame = malloc(sizeof(struct frame));
    if (frame == NULL){
        palloc_free_page(kpage);
        lock_release(&frame_lock);
        return NULL;
    }
    frame->thread = thread_current();
    frame->upage = upage;
    frame->kpage = kpage;
    frame->pinned = true;

    list_push_back (&frame_list, &frame->list_elem);
    hash_insert(&frame_table, &frame->hash_elem);

    lock_release(&frame_lock);

    return frame->kpage;
}

void
free_frame(void *kpage){
    ASSERT (lock_held_by_current_thread(&frame_lock) == true);
    ASSERT (is_kernel_vaddr(kpage));
    ASSERT (pg_round_down (kpage) == kpage);

    lock_acquire(&frame_lock);

    struct frame *f = get_frame(kpage);
    hash_delete (&frame_table, &f->hash_elem);
    list_remove (&f->list_elem);
    palloc_free_page(kpage);
    free(f);

    lock_release(&frame_lock);
}



static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED){
  struct frame *entry = hash_entry(elem, struct frame, hash_elem);
  return hash_bytes( &entry->kpage, sizeof entry->kpage );
}

static bool frame_less_func(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED){
  struct frame *a = hash_entry(a_, struct frame, hash_elem);
  struct frame *b = hash_entry(b_, struct frame, hash_elem);
  return a->kpage < b->kpage;
}