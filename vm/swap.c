#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"

/* 4096 bytes / 512 bytes == 8 */
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static struct block *swap_block;
static struct bitmap *swap_bitmap;
static size_t swap_size;

void
swap_init(){
    swap_block = block_get_role(BLOCK_SWAP);
    swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
    ASSERT (swap_size > 0);
    swap_bitmap = bitmap_create(swap_size);
    ASSERT(swap_bitmap != NULL);
    /* initially all of bitmap is free */
    bitmap_set_all(swap_bitmap, true);
}

/* 
write one frame to a swap slot
return the index of the written slot
*/
size_t
swap_out(void* kpage){
    printf("DEBUG: swap_out: %p\n", kpage);
    /* find available region */
    size_t swap_slot = bitmap_scan(swap_bitmap, 0, 1, true);
    ASSERT(swap_slot != BITMAP_ERROR);

    for (int i = 0; i < SECTORS_PER_PAGE; ++i){
        block_write(swap_block, swap_slot * SECTORS_PER_PAGE + i, kpage + (BLOCK_SECTOR_SIZE * i));
    }
    /* set bitmap at swap_slot as used */
    bitmap_set(swap_bitmap, swap_slot, false);

    return swap_slot;
}

/*
read contents from block at swap_slot into the frame
*/
void
swap_in(size_t swap_slot, void *kpage){
    printf("DEBUG: swap_in: %p\n", kpage);
    ASSERT(swap_slot < swap_size);
    /* make sure bitmap at slot index is defined */
    ASSERT(bitmap_test(swap_bitmap, swap_slot) == false);

    for (int i = 0; i < SECTORS_PER_PAGE; ++i){
        block_read(swap_block, swap_slot * SECTORS_PER_PAGE + i, kpage + (BLOCK_SECTOR_SIZE * i));
    }
    /* set bitmap at swap_slot as free */
    bitmap_set(swap_bitmap, swap_slot, true);
}

void
swap_free (size_t swap_slot){
  ASSERT (swap_slot < swap_size);
  ASSERT (bitmap_test(swap_bitmap, swap_slot) == false);
  bitmap_set(swap_bitmap, swap_slot, true);
}