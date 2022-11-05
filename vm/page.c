#include "vm/page.h"
#include "vm/frame.h"
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

struct page* 
page_create_from_filesys (void *upage, bool writable, struct file* file, off_t file_offset,
            uint32_t read_bytes, uint32_t zero_bytes){

    ASSERT(upage == pg_round_down(upage));
    //printf("DEBUG: page_create_from_filesys for vaddr: %p\n", vaddr);
    struct thread *t = thread_current();
    struct page *p;
    p = (struct page*) malloc(sizeof(struct page));

    /* allocation failed */
    if (p == NULL){
        return false;
    }

    p->pstatus = FROM_FILE;
    p->upage = upage;
    p->kpage = NULL;
    p->writable = writable;
    p->thread = t;
    // TODO SWAP Info: which block? 
    p->file = file;
    p->file_offset = file_offset;
    p->file_bytes = read_bytes;
    p->zero_bytes = zero_bytes;

    /* hash_insert returns notNULL if elem is already present */
    if (hash_insert(t->page_table, &p->hash_elem) != NULL){
        free(p);
        PANIC("Tried to insert a duplicate page table entry");
        return false;
    }

    return true;
}

struct page* 
page_create_zeropage (void *upage){

    struct thread *t = thread_current();
    struct page *p;
    p = (struct page*) malloc(sizeof(struct page));

    /* allocation failed */
    if (p == NULL){
        return false;
    }

    p->pstatus = ZERO_PAGE;
    p->upage = upage;
    p->kpage = NULL;
    p->thread = t;
    // TODO SWAP Info: which block? 

    /* hash_insert returns notNULL if elem is already present */
    if (hash_insert(t->page_table, &p->hash_elem) != NULL){
        free(p);
        PANIC("Tried to insert a duplicate page table entry");
        return false;
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
    if (p->pstatus == HAS_FRAME){
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
        case HAS_FRAME:
            PANIC("A new frame was allocated for page that has frame already");
            break;
        case ZERO_PAGE:
            memset(new_kpage, 0, PGSIZE);
            break;
        case ON_SWAP:
            //TODO: Load from swap
            break;
        case FROM_FILE:
            /* Handle not being able to read from file*/
            if (!page_read_from_file(p, new_kpage)){
                //TODO: free frame and page
                return false;
            }
            writable = p->writable;
            break;
        default:
            PANIC("Page with undefined status.");
    }

    if(!page_install(p->upage, new_kpage, writable)){
        //TODO: free frame and page
        return false;
    }
    pagedir_set_dirty (p->thread->pagedir, new_kpage, false);
    frame_unpin(new_kpage);
    p->kpage = new_kpage;
    p->pstatus = HAS_FRAME;

    return true;
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