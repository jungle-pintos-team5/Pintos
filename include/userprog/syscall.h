#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
struct file *find_file(int fd);
void check_addr(void *vaddr);
void halt(void);
void exit(int status);
int write (int fd, const void *buffer, unsigned size);
bool create(const char *file, unsigned initial_size);
bool remove (const char *file);
int open(const char *file);
int allocate_fd(struct file *f);

#endif /* userprog/syscall.h */
