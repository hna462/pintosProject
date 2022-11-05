#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"


static bool install_page(void *upage, void *kpage, bool writable);


/* Get page from the page table by it's virtual address */
struct page*
get_page (const void* vaddr){
    if (thread_current()->page_table == NULL){
        return NULL;
    }
    struct page p;
    struct hash_elem* e;
    p.vaddr = (void *) pg_round_down (vaddr);
    e = hash_find (thread_current()->page_table, &p.hash_elem);
    if (e != NULL) {
        return hash_entry (e, struct page, hash_elem);
    }
    else {
        return NULL;
    }
}

struct page* 
create_page_filesys (void *vaddr, bool writable, struct file* file, off_t file_offset,
            uint32_t read_bytes, uint32_t zero_bytes){
    ASSERT(vaddr == pg_round_down(vaddr));
    //printf("DEBUG: create_page_filesys for vaddr: %p\n", vaddr);
    struct thread *t = thread_current();
    struct page *p;
    p = (struct page*) malloc(sizeof(struct page));

    // if ((vaddr > PHYS_BASE - STACK_MAX_SIZE) && (t->user_esp - 32 < address))

    /* allocation failed */
    if (p == NULL){
        return false;
    }

    p->vaddr = vaddr;
    p->writable = writable;
    p->thread = t;
    //TODO: State: in frame / in swap / in file (elf file)
    // SWAP Info: which block? 
    p->file = file;
    p->file_offset = file_offset;
    p->file_bytes = read_bytes;
    p->zero_bytes = zero_bytes;

    /* hash_insert returns notNULL if elem is already present */
    if (hash_insert(t->page_table, &p->hash_elem) != NULL){
        free(p);
        return false;
    }

    return true;
}

bool
read_page_from_filesys(struct page *p, void *kpage){
    file_seek (p->file, p->file_offset);
    int bytes_read = file_read(p->file, kpage, p->file_bytes);
    if (bytes_read != (int)p->file_bytes){
        return false;
    }
    ASSERT(p->file_bytes + p->zero_bytes == PGSIZE);
    memset(kpage + bytes_read, 0, p->zero_bytes);
    return true;
}

bool
handle_page_fault (void* fault_addr){
    fault_addr = pg_round_down(fault_addr);
    //printf("DEBUG: handle_page_fault for %p \n", fault_addr);
    struct page *p = get_page(fault_addr);
    //printf("DEBUG: hange_page_fault. get_page: %p \n", p);
    if (p == NULL){
        return false;
    }
    void* upage = p->vaddr;
    void* kpage = palloc_get_page(PAL_USER);
    //printf("DEBUG: handle_page_fault upage: %p kpage: %p\n", upage, kpage);
    read_page_from_filesys(p, kpage);
    bool writable = true;
    bool success = install_page(upage, kpage, writable);
    //printf("DEBUG: handle_page_fault success: %d\n", success);
    return success;
}


unsigned
page_hash_func(const struct hash_elem *e, void *aux UNUSED){
    const struct page *p = hash_entry(e, struct page, hash_elem);
    return ((uintptr_t) p->vaddr) >> PGBITS;
}

/* to compare two hash elements. Return true if page A precedes page B.*/
bool
page_less_func(const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED){
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
  return a->vaddr < b->vaddr;
}

/* Copied from process.c */
static bool
install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL
           && pagedir_set_page(t->pagedir, upage, kpage, writable);
}