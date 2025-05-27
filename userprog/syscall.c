#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "lib/kernel/stdio.h"
#include "lib/string.h"
#include "userprog/process.h"
#include "threads/palloc.h"

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

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void check_addr(void *vaddr){
	struct thread *t = thread_current();

	// 유저 주소인가?
	if(!is_user_vaddr(vaddr)){
		exit(-1);
	}

	// 주소가 page랑 매핑되어 있는가?
	if(pml4_get_page(t->pml4, vaddr) == NULL){
		exit(-1);
	}

	if(vaddr == NULL){
		exit(-1);
	}
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	
	// rdi -> rsi -> rdx -> r10 -> r8 -> r9
	switch (f->R.rax)
	{
		case SYS_HALT:
			halt(); // 핀토스 종료
			break;
		case SYS_EXIT:
			exit(f->R.rdi);	// 프로세스 종료
			break;
		case SYS_FORK:
			f->R.rax = sys_fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
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
			// printf ("system call!\n");
			thread_exit ();
			break;
	}
}

void halt(void) {
	power_off();
}

void exit(int status){
	struct thread *cur = thread_current();
    cur->exit_status = status;

	printf("%s: exit(%d)\n", thread_name(), status); 
	thread_exit();	
}

int write (int fd, const void *buffer, unsigned size) {
	check_addr(buffer);

	// fd가 0은 read용이다. 따라서 fd가 0일 때도 막아야 함.
	if (fd > 64 || size == 0 || fd == 0){
		exit(-1);
	}
	// fd가 1이면 표준 출력
	if (fd == 1) {
		// putbuf: 커널 콘솔에 buffer의 내용을 size만큼 출력
		putbuf(buffer, size);
		return size;  // 출력한 바이트 수 반환
	}

	struct file *find_f = find_file(fd);
	return file_write (find_f, buffer, size);
}

bool create(const char *file, unsigned initial_size){
	check_addr(file);
	return filesys_create(file, initial_size);
}

bool remove (const char *file){
	check_addr(file);
	return filesys_remove (file);
}

int allocate_fd(struct file *f){
	struct thread *t = thread_current();
	if (f == NULL || t->next_fd >= 64){
		return -1;
	}

	if(t->next_fd >= 3){
		for(int i = 3; i<64; i++){
			if(t->fdt[i] == NULL){
				t->fdt[i] = f;
				return i;
			}
		}
		return -1;
	}

	t->fdt[t->next_fd] = f;
	return t->next_fd++;
}

int open(const char *file){
	check_addr(file);

	struct file *open_f = filesys_open(file);
	return allocate_fd(open_f);
}

struct file *find_file(int fd){
	struct thread *t = thread_current();
	struct file *find_f = t->fdt[fd];
	return find_f;
}

int filesize(int fd){
	struct file * file = find_file(fd);
	return file_length(file);
}

int read(int fd, void *buffer, unsigned size){
	check_addr(buffer);
	
	uint8_t *buff = buffer;
	if (fd > 64 || fd == 1){
		exit(-1);
	}

	if (fd == 0){
		int i;

		for(i = 0; i < size; i++){
			buff[i] = input_getc();
			if(buff[i] == '\0'){
				break;
			}
		}
		return i;
	}

	struct file *find_f = find_file(fd);	
	return file_read (find_f, buff, size);
}

void close(int fd){
	if (fd > 64){
		exit(-1);
	}

	struct file *find_f = find_file(fd);
	file_close(find_f);
}

void seek(int fd, unsigned position){
	if(fd > 64){
		exit(-1);
	}

	struct file *find_f = find_file(fd);
	file_seek (find_f, position);
}

unsigned tell(int fd){
	if(fd > 64){
		exit(-1);
	}

	struct file *find_f = find_file(fd);
	return file_tell(find_f);
}

pid_t sys_fork(const char *thread_name, struct intr_frame *parent_if){
	return process_fork(thread_name, parent_if);
}

int exec(const char *cmd_line){
	char *copy = palloc_get_page(PAL_ZERO);
	// strlcpy(copy, cmd_line, PGSIZE);
	memcpy(copy,cmd_line,strlen(cmd_line)+1);
	int process_fail = process_exec(copy);
	if (process_fail < 0){
		return -1;
	}
	return NULL;
}

int wait(pid_t pid){
	// return process_wait(pid);
}