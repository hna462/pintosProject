#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>

static struct lock frame_lock;
static struct list frame_clock_list; 
static struct hash frame_table;
static struct list_elem *clock_ptr; /* clock algorithm pointer */

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool  frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);
struct frame * find_frame_to_evict(struct thread*);

void
frame_table_init ()
{
    clock_ptr = NULL;
    lock_init (&frame_lock);
    list_init (&frame_clock_list);
    hash_init (&frame_table, frame_hash_func, frame_less_func, NULL);
}

/* Get frame from the frame table by it's kernel page */
struct frame*
frame_get (const void* kpage){
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

void * frame_allocate (enum palloc_flags flags, void *upage){

    lock_acquire(&frame_lock);

    void* kpage = palloc_get_page (PAL_USER | flags);
    if (kpage == NULL){
        /* Could not allocate frame - find frame to evict */
        struct frame *evicted_frame = find_frame_to_evict(thread_current());

        /* was evicted frame dirty? TODO do something with this information */
        // bool is_dirty = pagedir_is_dirty(evicted_frame->thread->pagedir, evicted_frame->upage) ||
        //                 pagedir_is_dirty(evicted_frame->thread->pagedir, evicted_frame->kpage);
       
        pagedir_clear_page(evicted_frame->thread->pagedir, evicted_frame->upage);
        size_t swap_slot = swap_out(evicted_frame->kpage);
        page_set_on_swap(evicted_frame->upage, swap_slot);

        frame_free(evicted_frame->kpage, true, false);
        
        /* let's try again */
        kpage = palloc_get_page (PAL_USER | flags);
        ASSERT(kpage != NULL);
    }

    struct frame *frame = malloc(sizeof(struct frame));
    ASSERT (frame != NULL);

    frame->thread = thread_current();
    frame->upage = upage;
    frame->kpage = kpage;
    frame->pinned = true;

    list_push_back (&frame_clock_list, &frame->list_elem);
    hash_insert(&frame_table, &frame->hash_elem);

    lock_release(&frame_lock);

    return frame->kpage;
}

/* free_kpage: flag that sets whether the actual frame kpage should be evicted or not
   with_lock: flag that sets whether frame free is performed with a lock
    */
void
frame_free(void *kpage, bool free_kpage, bool with_lock){

    if (with_lock){
        lock_acquire(&frame_lock);
    }
    

    ASSERT (lock_held_by_current_thread(&frame_lock) == true);
    ASSERT (is_kernel_vaddr(kpage));
    ASSERT (pg_round_down (kpage) == kpage);

    struct frame *f = frame_get(kpage);
    hash_delete (&frame_table, &f->hash_elem);
    list_remove (&f->list_elem);
    if (free_kpage){
        palloc_free_page(kpage);
    }
    free(f);

    if (with_lock){
        lock_release(&frame_lock);
    }
}

struct frame *
find_frame_to_evict(struct thread* t){
    size_t table_size = list_size(&frame_clock_list);
    if (table_size == 0){
        PANIC("Tried to evict a frame, but frame table is empty");
    }

    /* Go through the "clock" 2 times. More iterations would be redundant. */
    for (int i = 0; i <= 2*table_size; ++i){

        /* If clock_ptr is NULL(not initialized yet) or reached end, we set it 
           to the begging of the list */
        if (clock_ptr == NULL || clock_ptr == list_end(&frame_clock_list)){
            clock_ptr = list_begin(&frame_clock_list);
        }
        /* Get Next frame in the clock */
        else{
            clock_ptr = list_next(clock_ptr);
        }

        struct frame *cur_frame = list_entry(clock_ptr, struct frame, list_elem);
        if (cur_frame->pinned){
            continue;
        }
        else if (pagedir_is_accessed(t->pagedir, cur_frame->upage)){
            pagedir_set_accessed(t->pagedir, cur_frame->upage, false);
            continue;
        }
        /* Found frame to evict */
        /* make sure frame is valid*/
        ASSERT(cur_frame != NULL && cur_frame->kpage != NULL && cur_frame->thread != NULL);
        /* make sure the frame has not been cleared already */
        ASSERT(cur_frame->thread->pagedir != (void*) 0xcccccccc);
        return cur_frame;
    }
    PANIC("Tried to evict a frame, but no frame is available");
}

static void
frame_set_pinned(void *kpage, bool new_val){

    lock_acquire(&frame_lock);

    struct frame *f = frame_get(kpage);
    f->pinned = new_val;

    lock_release(&frame_lock);
}

void
frame_pin(void *kpage){
    frame_set_pinned(kpage, true);
}

void
frame_unpin(void *kpage){
    frame_set_pinned(kpage, false);
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