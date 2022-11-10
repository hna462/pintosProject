#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init();
size_t swap_out(void *kpage);
void swap_in(size_t swap_slot, void *kpage);
void swap_free (size_t swap_slot);

#endif /* vm/swap.h */