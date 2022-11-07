#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/swap.h"
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"



static bool page_install(void *upage, void *kpage, bool writable);


/* Get page from the page table by it's virtual address */
struct page*
page_get (const void* vaddr){
    if (thread_current()->page_table == NULL){
        return NULL;
    }
    struct page p;
    struct hash_elem* e;
    p.upage = (void *) pg_round_down (vaddr);
    e = hash_find (thread_current()->page_table, &p.hash_elem);
    if (e != NULL) {
        return hash_entry (e, struct page, hash_elem);
    }
    else {
        return NULL;
    }
}

bool
page_set_on_swap(const void* upage, size_t swap_slot){
    struct page *p = page_get(upage);
    if (p == NULL){
        PANIC("Tried to set non-existing page on Swap.");
        return false;
    }
    p->pstatus = ON_SWAP;
    p->has_frame = false;
    p->swap_slot = swap_slot;
    p->kpage = NULL;
    return true;
}

bool
page_create (void *upage, enum pstatus starting_status, void *aux){

    ASSERT(upage != NULL);


    struct page *new_page = (struct page*) malloc(sizeof(struct page));
    if (new_page == NULL){
        PANIC("Could not allocate new page in page_create");
    }

    /* default field values */
    new_page->upage = upage;
    new_page->thread = thread_current();
    new_page->kpage = NULL;
    new_page->writable = true; /* All pages are writable by default */
    new_page->file = NULL;
    new_page->file_offset = NULL;
    new_page->file_bytes = NULL;
    new_page->zero_bytes = NULL;
    new_page->swap_slot = NULL;
    new_page->pstatus = starting_status;
    new_page->has_frame = false;

    struct page *aux_data = (struct page * ) aux;

    switch(starting_status){
        case FROM_FILE:
            // if(aux_data->file_bytes == 0){
            //     new_page->pstatus = ZERO_PAGE;
            // }
            // else {
                new_page->file = aux_data->file;
                new_page->file_offset = aux_data->file_offset;
                new_page->file_bytes = aux_data->file_bytes;
                new_page->zero_bytes = aux_data->zero_bytes;
                new_page->writable = aux_data->writable;
            // }
            
        case ZERO_PAGE:
            /* nothing to initialize */
            break;
        case FROM_FRAME:
            new_page->kpage = aux_data->kpage;
            new_page->has_frame = true;
            break;
        case ON_SWAP:
            PANIC("Cannot directly create a new page table entry on swap");
            break;
    }

    /* hash_insert returns notNULL if elem is already present */
    if (hash_insert(thread_current()->page_table, &new_page->hash_elem) != NULL){
        PANIC("Tried to insert a duplicate page table entry");
    }

    if(starting_status == FROM_FRAME){
        frame_unpin(new_page->kpage);
    }

    return true;
}


bool
page_read_from_file(struct page *p, void *kpage){
    ASSERT(p->file_bytes + p->zero_bytes == PGSIZE);
    file_seek (p->file, p->file_offset);
    int bytes_read = file_read(p->file, kpage, p->file_bytes);
    if (bytes_read != (int)p->file_bytes){
        return false;
    }
    memset(kpage + bytes_read, 0, p->zero_bytes);
    return true;
}

bool
handle_page_fault (void* fault_addr){

    fault_addr = pg_round_down(fault_addr);
    //printf("DEBUG: handle_page_fault for %p \n", fault_addr);
    struct page *p = page_get(fault_addr);
    if (p == NULL){
        return false;
    }
    if (p->has_frame){
        //printf("DEBUG: handle_page_fault HAS FRAME ALREADY");
        return true;
    }

    void* new_kpage = frame_allocate(PAL_USER, p->upage);
    /* Could not allocate a frame for this page */
    if (new_kpage == NULL){
        return false;
    }

    bool writable = true;

    switch (p->pstatus){
        case FROM_FRAME:
            PANIC("A new frame was allocated for page that has frame already");
            break;
        case ZERO_PAGE:
            memset(new_kpage, 0, PGSIZE);
            break;
        case ON_SWAP:
            //printf("DEBUG: got page from swap_in! \n");
            frame_lock_acquire();
            swap_in(p->swap_slot, new_kpage);
            frame_lock_release();
            break;
        case FROM_FILE:
            /* Handle not being able to read from file*/
            if (!page_read_from_file(p, new_kpage)){
                frame_free(new_kpage, true, true);
                return false;
            }
            writable = p->writable;
            break;
        default:
            PANIC("Page with undefined status.");
    }
    
    if(!page_install(p->upage, new_kpage, writable)){
        frame_free(new_kpage, true, true);
        return false;
    }
    pagedir_set_dirty (p->thread->pagedir, new_kpage, false);
    frame_unpin(new_kpage);
    p->has_frame = true;
    p->kpage = new_kpage;
    if(p->pstatus != FROM_FILE){
        p->pstatus = FROM_FRAME;
    }

    return true;
}

bool
preload_multiple_pages_and_pin(const void *start_addr, size_t size){
    /* iterate through all pages */
    void *cur_page;
    for(cur_page = pg_round_down(start_addr); cur_page < start_addr + size; cur_page += PGSIZE){
        /* code is similar to the handle_page_fault but we are pinning each frame*/
        struct page *p = page_get(cur_page);
        ASSERT(p != NULL);
        if (p->has_frame == true){
            frame_pin(p->kpage);
            continue;
        }
        void* new_kpage = frame_allocate(PAL_USER, p->upage);
        ASSERT(new_kpage != NULL);
        switch(p->pstatus){
            case ZERO_PAGE:
                memset(new_kpage, 0, PGSIZE);
                break;
            case ON_SWAP:
                frame_lock_acquire();
                swap_in(p->swap_slot, new_kpage);
                frame_lock_release();
                break;
            case FROM_FILE:
                /* Handle not being able to read from file*/
                if (!page_read_from_file(p, new_kpage)){
                    frame_free(new_kpage, true, true);
                    return false;
                }
                break;
            default:
            PANIC("Page with undefined status.");
        }
        if(!page_install(p->upage, new_kpage, p->writable)){
            frame_free(new_kpage, true, true);
            return false;
        }
        frame_pin(new_kpage);
        p->has_frame = true;
        p->kpage = new_kpage;
        if(p->pstatus != FROM_FILE){
            p->pstatus = FROM_FRAME;
        }

        continue;
    }
    return true;
}

void
unpin_multiple_pages(const void *start_addr, size_t size){
    /* iterate through all pages */
    void *cur_page;
    for(cur_page = pg_round_down(start_addr); cur_page < start_addr + size; cur_page += PGSIZE){
        struct page *p = page_get(cur_page);
        ASSERT(p != NULL);
        frame_unpin(p->kpage);
    }
}

unsigned
page_hash_func(const struct hash_elem *e, void *aux UNUSED){
    const struct page *p = hash_entry(e, struct page, hash_elem);
    return ((uintptr_t) p->upage) >> PGBITS;
}

/* to compare two hash elements. Return true if page A precedes page B.*/
bool
page_less_func(const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED){
    const struct page *a = hash_entry (a_, struct page, hash_elem);
    const struct page *b = hash_entry (b_, struct page, hash_elem);
    return a->upage < b->upage;
}

void
page_destroy_func(struct hash_elem *e, void *aux UNUSED){
  const struct page *p = hash_entry(e, struct page, hash_elem);
  if (p->kpage != NULL) {
    ASSERT (p->has_frame == true);
    frame_free(p->kpage, false, true);
  }
  else if(p->pstatus == ON_SWAP) {
    swap_free (p->swap_slot);
  }
  free (p);
}

void
clear_page_table(){
    //printf("DEBUG: clear_page_table for thread: %p\n", thread_current());
    struct hash *h = thread_current()->page_table;
    if (h != NULL){
        hash_destroy (h, page_destroy_func);
    }
    h = NULL;
}

/* copy pasted install_page from process.c */
static bool
page_install(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL
           && pagedir_set_page(t->pagedir, upage, kpage, writable);
}