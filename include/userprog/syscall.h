#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "lib/user/syscall.h"

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
void close(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int filesize(int fd);
pid_t fork(const char *thread_name);
int exec(const char *cmd_line);
int wait(pid_t pid);

#endif /* userprog/syscall.h */
