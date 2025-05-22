#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "kernel/stdio.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/string.h"
#include "userprog/process.h"
void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
struct lock synclock;
void
syscall_init (void) {
	lock_init(&synclock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	int sys_number = f->R.rax;

    // Argument 순서
    // %rdi %rsi %rdx %r10 %r8 %r9

    switch (sys_number) {
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            exit(f->R.rdi);
            break;
        case SYS_FORK:
            f->R.rax = fork(f->R.rdi);
            break;
        case SYS_EXEC:
            f->R.rax = exec(f->R.rdi);
            break;
        case SYS_WAIT:
            f->R.rax = process_wait(f->R.rdi);
            break;
        case SYS_CREATE:
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;
        case SYS_REMOVE:
            f->R.rax = remove(f->R.rdi);
            break;
        case SYS_OPEN:
            f->R.rax = open(f->R.rdi);
            break;
        case SYS_FILESIZE:
            f->R.rax = filesize(f->R.rdi);
            break;
        case SYS_READ:
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_WRITE:
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_SEEK:
            seek(f->R.rdi, f->R.rsi);
            break;
        case SYS_TELL:
            f->R.rax = tell(f->R.rdi);
            break;
        case SYS_CLOSE:
            close(f->R.rdi);
            break;
        default:
            exit(-1);
    }
	
}
void halt(){
	power_off();
}
void checkaddress(void *cad){//유저 프로세스에서 정보를 넘기기위해 준 포인터
	struct thread *t = thread_current();
	if(is_kernel_vaddr(cad)||cad == NULL||pml4_get_page(t->pml4,cad) == NULL)
		exit(-1);
}
void exit (int status){
	struct thread *t = thread_current();
	t->exit_s = status;
	printf ("%s: exit(%d)\n", t->name,t->exit_s);
	thread_exit();
}
pid_t fork (const char *thread_name){
	checkaddress(thread_name);
	process_fork(thread_name,NULL);
}
int exec (const char *cmd_line){
	checkaddress(cmd_line);
	char *copy = palloc_get_page(PAL_ZERO);
	if (copy == NULL)
        return -1;
	memcpy(copy,cmd_line,strlen(cmd_line)+1);
	if (process_exec(copy) == -1)
        return -1;
	NOT_REACHED();
}
int wait (pid_t pid){
	return process_wait(pid);
}
bool create (const char *file, unsigned initial_size){
	checkaddress(file);
	lock_acquire(&synclock);
	bool check = filesys_create(file,initial_size);
	lock_release(&synclock);
	return check;
}
bool remove(const char *file){
	checkaddress(file);
	lock_acquire(&synclock);
	bool check = filesys_remove(file);
	lock_release(&synclock);
	return check;
}
int open (const char *file){
	checkaddress(file);
	lock_acquire(&synclock);
	struct file *o = filesys_open(file);
	int fd = add_file(o);
	lock_release(&synclock);
	return fd;
}
int filesize (int fd){
	lock_acquire(&synclock);
	struct file *f = get_file(fd);
	if(f == NULL){
		return -1;
	}
	int length = file_length(f);
	lock_release(&synclock);
	return length;
}
int read (int fd, void *buffer, unsigned size){
	checkaddress(buffer);
	if (fd == 1||fd == 2){ 
        return -1;
	}
	
	off_t rb;
	if(fd == 0){
		uint8_t *buf = buffer;
		int i = 0;
        for (; i < size; i++) {
            buf[i] = input_getc();
			if (buf[i] == '\0')
                break;
        }
		rb = i;
	}else{
	lock_acquire(&synclock);
	struct file *o = get_file(fd);
	if(o == NULL){
		lock_release(&synclock);
		return -1;
	}
	rb = file_read(o,buffer,size);
	lock_release(&synclock);
	}
	return rb;
}
int write (int fd, const void *buffer, unsigned size){
	checkaddress(buffer);
	if(fd == 0){
		return -1;
	}
	if(fd == 1||fd == 2){
		putbuf((const char *)buffer,size);
		return size;
	}
	lock_acquire(&synclock);
	struct file *f = get_file(fd);
	if (f == NULL){
		lock_release(&synclock);
        return -1;
	}
	off_t ofs = file_write(f,buffer,size);
	lock_release(&synclock);
	return ofs;
}
void seek (int fd, unsigned position){
	lock_acquire(&synclock);
	struct file *f = get_file(fd);
	if(f == NULL){
		lock_release(&synclock);
        exit(-1);
	}
	file_seek(f,position);
	lock_release(&synclock);
}
unsigned tell (int fd){
	lock_acquire(&synclock);
	struct file *f = get_file(fd);
	if(f == NULL){
		lock_release(&synclock);
        exit(-1);
	}
	off_t rftv = file_tell(f);
	lock_release(&synclock);
	return rftv;
}
void close (int fd){
	lock_acquire(&synclock);
	close_file(fd);
	lock_release(&synclock);
}
struct file* get_file(int fd){
	struct thread *t = thread_current();
	struct file **fdt = t->fdt;
	if(fd<0||fd>=t->fdcnt){
		return NULL;
	}
	return fdt[fd];
}
int add_file(struct file *f){
	struct thread *t = thread_current();
	if(f == NULL||t->fdcnt>=128)
		return -1;
	struct file **fdt = t->fdt;
	if(t->fdcnt>3){
	for(int i = 3;i<t->fdcnt;i++){
		if(fdt[i] == NULL){
			fdt[i] = f;
			return i;
		}
	}
	}
	fdt[t->fdcnt] = f;
	return t->fdcnt++;
}
void close_file(int fd){
	struct thread *t = thread_current();
	if(fd>=t->fdcnt|| t->fdt[fd] == NULL){
		return -1;
	}
	struct file **fdt = t->fdt;
	struct file *fdf = fdt[fd];
	fdt[fd] = NULL;
	file_close(fdf);
}
struct thread* getchild(pid_t pid){
	struct thread *curr = thread_current();
    struct thread *t;

    for (struct list_elem *e = list_begin(&curr->children); e != list_end(&curr->children); e = list_next(e)) {
        t = list_entry(e, struct thread, ichild);

        if (pid == t->tid)
            return t;
    }
    return NULL; 
}