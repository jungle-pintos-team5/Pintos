#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
int write(int fd, const void *buffer, unsigned size);
bool create(const char *file, unsigned initial_size);

#endif /* userprog/syscall.h */
