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
#include "devices/shutdown.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
int exec_process(char *file_name);
void exit_process(int status);
void * is_valid_addr(const void *vaddr);
struct process_file* search_fd(struct list* files, int fd);
void clean_single_file(struct list* files, int fd);


void syscall_exit(struct intr_frame *f);
int syscall_exec(struct intr_frame *f);
int syscall_wait(struct intr_frame *f);
int syscall_creat(struct intr_frame *f);
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
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	int *p = f->esp;
	is_valid_addr(p);

	thread_current()->latest_esp = f->esp;

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
	tid_t child_tid = process_execute(file_name);
	return child_tid;
}

/*exit_process*/
void
exit_process(int status)
{
	struct list *parent_children = &thread_current()->parent->children;
	struct thread * cur_thread = thread_current();
	struct list_elem *e = NULL;
	struct child *ch = NULL;
	//enum intr_level old_intr_level = intr_disable();
	for(e = list_begin(parent_children); e != list_end(parent_children); e = list_next(e)){
		ch = list_entry(e, struct child, elem);
		//printf("DEBUG exit_process iter curr %d thisThread %d\n", ch->tid, thread_current()->tid);
		if (ch->tid == cur_thread->tid){
				ch->waiting = true;
				ch->exit_code = status;
		}
	}
	cur_thread->exit_code = status;
	if (thread_current()->parent->waiting_for != NULL){
        if (thread_current()->parent->waiting_for->tid == thread_current()->tid){
            //printf("DEBUG exit sema up fro thread: %d\n", thread_current()->tid);
            sema_up(&thread_current()->parent->waiting_for->wait_sema);
        }
        
    }
	//intr_set_level(old_intr_level);	
	thread_exit();
}

/*test is_valid addr*/
void *
is_valid_addr(const void *vaddr)
{
	if (!is_user_vaddr(vaddr)){
		//printf("DEBUG invalid user addr\n");
		exit_process(-1);
		return 0;
	}
	void * page_ptr = page_ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (!page_ptr)
	{
		//printf("DEBUG INVALID PAGE PTR\n");
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
    palloc_free_page(proc_f);
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
		palloc_free_page(proc_f);
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
	if (!is_valid_addr(file_name)){
		return -1;
	}
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

int
syscall_read(struct intr_frame *f)
{
	int *p = f->esp;
	int ret;
	int **call_args = f->esp;
	int size = call_args[3];
	void *buffer = call_args[2];
	int fd = call_args[1];
	if (!is_user_vaddr((uint32_t*) buffer)){
		exit_process(-1);
	}
	if (fd == 0){
		int i;
		uint8_t *buffer = buffer;
		for (i = 0; i < size; i++)
			buffer[i] = input_getc();
		ret = size;
	}
	else{ // fd != 0
		struct process_file* fptr = search_fd(&thread_current()->files, fd);
			if(fptr==NULL)
				ret = -1;
			else
			{
				acquire_filesys_lock();
				// if (!preload_multiple_pages_and_pin(buffer, size)){
				// 	release_filesys_lock();
				// 	exit_process(-1);
				// }
				ret = file_read (fptr->ptr, buffer, size);
				// unpin_multiple_pages(buffer, size);
				release_filesys_lock();
			}
	}
	return ret;
}

int
syscall_write(struct intr_frame *f)
{
	int ret;
	int **call_args = f->esp;
	int size = call_args[3];
	void *buffer = call_args[2];
	int fd = call_args[1];
	
	is_valid_addr(buffer);
	is_valid_addr(buffer+size-1);

	if (fd == 1){
		putbuf(buffer, size);
		ret = size;
	}
	else {
		struct process_file* fptr = search_fd(&thread_current()->files, fd);
			if(fptr==NULL)
				ret = -1;
			else{
				acquire_filesys_lock();
				// if (!preload_multiple_pages_and_pin(buffer, size)){
				// 	release_filesys_lock();
				// 	exit_process(-1);
				// }
				ret = file_write (fptr->ptr, buffer, size);
				// unpin_multiple_pages(buffer, size);
				release_filesys_lock();
			}
	}
	return ret;
}

int
syscall_creat(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(*(p+1));
	is_valid_addr(p+2);
	void *name = *(p+1);
	int size = *(p+2);
	acquire_filesys_lock();
	int res = filesys_create(name, size);
	release_filesys_lock();
	return res;
}

int
syscall_open(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	is_valid_addr(*(p+1));
	char *file_name = *(p+1);
	acquire_filesys_lock();
	struct file* fptr = filesys_open (file_name);
	release_filesys_lock();
	int ret;
	if(fptr == NULL) ret = -1;
	else{
		struct process_file *pfile = palloc_get_page(0);
		pfile->ptr = fptr;
		pfile->fd = thread_current()->fd_count;
		thread_current()->fd_count++;
		list_push_back (&thread_current()->files, &pfile->elem);
		ret = pfile->fd;
	}
	return ret;
}

int syscall_filesize(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	int fd = *(p+1);
	acquire_filesys_lock();
	int ret = file_length (search_fd(&thread_current()->files, fd)->ptr);
	release_filesys_lock();
	return ret;
}

int syscall_remove(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	char *file_name = *(p+1);
	acquire_filesys_lock();
	bool ret;
	if(filesys_remove(file_name)==NULL)
		ret = false;
	else
		ret = true;
	release_filesys_lock();
	return ret;
}

void syscall_seek(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	is_valid_addr(p+2);
	char *fd = *(p+1);
	int position = *(p+2);
  acquire_filesys_lock();
	file_seek(search_fd(&thread_current()->files, fd)->ptr, position);
  release_filesys_lock();
}

int syscall_tell(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	int fd = *(p+1);
	acquire_filesys_lock();
	int ret = file_tell(search_fd(&thread_current()->files, fd)->ptr);
	release_filesys_lock();
	return ret;
}

void syscall_close(struct intr_frame *f){
	int *p = f->esp;
	is_valid_addr(p+1);
	int fd = *(p+1);
	acquire_filesys_lock();
	clean_single_file(&thread_current()->files, fd);
	release_filesys_lock();
}