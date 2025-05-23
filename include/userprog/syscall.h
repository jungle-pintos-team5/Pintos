#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
int allocate_fd(struct file *f);

#endif /* userprog/syscall.h */
