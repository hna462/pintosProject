
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "kernel/list.h"

struct process_file {
	struct file* ptr; /* pointer to the actual file */
	int fd; /* file descriptor corresponding to the file */
	struct list_elem elem;
};

//syscall.h changes

void syscall_init (void);
void clean_all_files(struct list* files);

#endif /* userprog/syscall.h */
