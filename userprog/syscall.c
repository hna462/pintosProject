#include "userprog/syscall.h"
#include <stdio.h>
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "kernel/list.h"
//#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
int exec_process(char *file_name);
void exit_process(int status);
void * is_valid_addr(const void *vaddr);
struct process_file* search_fd(struct list* files, int fd);
void clean_single_file(struct list* files, int fd);
// void clean_all_files(struct list* files); // declear in syscall.h used by another c files


void syscall_exit(struct intr_frame *f);
int syscall_exec(struct intr_frame *f);
int syscall_wait(struct intr_frame *f);
int syscall_create(struct intr_frame *f);
int syscall_remove(struct intr_frame *f);
int syscall_open(struct intr_frame *f);
int syscall_filesize(struct intr_frame *f);
int syscall_read(struct intr_frame *f);
int syscall_write(struct intr_frame *f);
void syscall_seek(struct intr_frame *f);
int syscall_tell(struct intr_frame *f);
void syscall_close(struct intr_frame *f);
void syscall_halt(void);

/*get wanted value*/
void pop_stack(int *esp, int *a, int offset){
	int *tmp_esp = esp;
	*a = *((int *)is_valid_addr(tmp_esp + offset));
}

/*halt*/
void syscall_halt(void){
	shutdown_power_off();
}

static void syscall_handler(struct intr_frame *);

void
syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    int *p = f->esp;
	is_valid_addr(p);

 uint32_t* args = ((uint32_t*) f->esp);
  	int system_call = *p;
	switch (system_call)
	{
		case SYS_HALT: syscall_halt(); break;
		case SYS_EXIT: syscall_exit(f); break;
		case SYS_EXEC: f->eax = syscall_exec(f); break;
		case SYS_WAIT: f->eax = syscall_wait(f); break;
		case SYS_CREATE: f->eax = syscall_creat(f); break;
		case SYS_REMOVE: f->eax = syscall_remove(f); break;
		case SYS_OPEN: f->eax = syscall_open(f); break;
		case SYS_FILESIZE: f->eax = syscall_filesize(f); break;
		case SYS_READ: f->eax = syscall_read(f); break;
		case SYS_WRITE: f->eax = syscall_write(f); break;
		case SYS_SEEK: syscall_seek(f); break;
		case SYS_TELL: f->eax = syscall_tell(f); break;
		case SYS_CLOSE: syscall_close(f); break;

		default:
		printf("Default %d\n",*p);
	}
}
/*create children process*/
int
exec_process(char *file_name)
{
//TODO
}

/*exit_process*/
void
exit_process(int status)
{
//TODO
}

/*test is_valid addr*/
void *
is_valid_addr(const void *vaddr)
{
	void *page_ptr = NULL;
	if (!is_user_vaddr(vaddr) || !(page_ptr = pagedir_get_page(thread_current()->pagedir, vaddr)))
	{
		exit_process(-1);
		return 0;
	}
	return page_ptr;
}

  /* Find fd and return process file struct in the list,
  if not exist return NULL. */
struct process_file *
search_fd(struct list* files, int fd)
{
	struct process_file *proc_f;
	for (struct list_elem *e = list_begin(files); e != list_end(files); e = list_next(e))
	{
		proc_f = list_entry(e, struct process_file, elem);
		if (proc_f->fd == fd)
			return proc_f;
	}
	return NULL;
}

  /* close and free specific process files
  by the given fd in the file list. Firstly,
  find fd in the list, then remove it. */
void
clean_single_file(struct list* files, int fd)
{
	struct process_file *proc_f = search_fd(files,fd);
	if (proc_f != NULL){
		file_close(proc_f->ptr);
		list_remove(&proc_f->elem);
    	free(proc_f);
	}
}

  /* close and free all process files in the file list */
void
clean_all_files(struct list* files)
{
	struct process_file *proc_f;
	while(!list_empty(files))
	{
		proc_f = list_entry (list_pop_front(files), struct process_file, elem);
		file_close(proc_f->ptr);
		list_remove(&proc_f->elem);
		free(proc_f);
	}
}

/*syscall_exit*/
void
syscall_exit(struct intr_frame *f)
{
	int status;
	pop_stack(f->esp, &status, 1);
	exit_process(status);
}

/*judge filename isvalue*/
int
syscall_exec(struct intr_frame *f)
{
	char *file_name = NULL;
	pop_stack(f->esp, &file_name, 1);
	if (!is_valid_addr(file_name))
		return -1;

	return exec_process(file_name);
}

/*wait children process*/
int
syscall_wait(struct intr_frame *f)
{
	tid_t child_tid;
	pop_stack(f->esp, &child_tid, 1);
	return process_wait(child_tid);
}
