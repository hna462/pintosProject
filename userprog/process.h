#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* PROJECT2: USERPROG */ 
struct process_args
{
    char *program_name; /* program name === argv[0] */
    char **tokens; /* argv[1+]... */
    uint16_t argc; /* number of tokens + program_name */
    char *save_ptr; /* save_ptr for strtok_r parsing */
};

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

#endif /* userprog/process.h */