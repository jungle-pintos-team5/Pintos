#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"


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




void check_address(const void *addr)
{
  struct thread *cur = thread_current();
  if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(cur->pml4, addr) == NULL)
    exit(-1);
	
}
// fd로 파일 찾는 함수
static struct file *find_file_by_fd(int fd) {
	struct thread *cur = thread_current();

	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}
	return cur->fd_table[fd];
}
// fd인자를 받아 파일 크기 리턴
int filesize(int fd) {
	struct file *open_file = find_file_by_fd(fd);
	if (open_file == NULL) {
		return -1;
	}
	return file_length(open_file);}
// 파일 생성하는 시스템 콜
// 성공일 경우 true, 실패일 경우 false 리턴
bool create(const char *file, unsigned initial_size) {		// file: 생성할 파일의 이름 및 경로 정보, initial_size: 생성할 파일의 크기
	check_address(file);
	return filesys_create(file, initial_size);
}

void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	/** project2-System Call */
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
        // case SYS_FORK:
        //     f->R.rax = fork(f->R.rdi);
        //     break;
        case SYS_EXEC:
            f->R.rax = exec(f->R.rdi);
            break;
        // case SYS_WAIT:
        //     f->R.rax = process_wait(f->R.rdi);
        //     break;
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
        // case SYS_READ:
        //     f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        //     break;
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

void 
halt(void) 
{
    power_off();
}

void 
exit(int status) 
{
    struct thread *t = thread_current();
    t->exit_status = status;
    printf("%s: exit(%d)\n", t->name, t->exit_status); // Process Termination Message
    thread_exit();
}

int write(int fd, const void *buffer, unsigned size)
{
  check_address(buffer); // 주소 유효성 검사
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  return -1;
}
int open(const char *file) {
	check_address(file);  // ← 반드시 제일 먼저
  
	struct file *open_file = filesys_open(file);
	if (open_file == NULL)
	  return -1;
  
	int fd = add_file_to_fdt(open_file);
	if (fd == -1)
	  file_close(open_file);
  
	return fd;
  }
  int add_file_to_fdt(struct file *file) {
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;
  
	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx])
	  cur->fd_idx++;
  
	if (cur->fd_idx >= FDCOUNT_LIMIT)
	  return -1;
  
	fdt[cur->fd_idx] = file;
	return cur->fd_idx++;
  }
  


// 파일 위치(offset)로 이동하는 함수
void seek(int fd, unsigned position) {
    struct file *f = process_get_file(fd);
    if (f != NULL)
        file_seek(f, position);  
}

int tell(int fd) {
    struct file *f = process_get_file(fd);
    if (f == NULL)
        return 0;
    return file_tell(f);  
}
// 열린 파일을 닫는 시스템 콜. 파일을 닫고 fd제거
void close(int fd) {
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL) {
		return;
	}
	remove_file_from_fdt(fd);
}
// fd에 해당하는 파일 포인터 반환
struct file *process_get_file(int fd) {
	struct thread *cur = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	  return NULL;
	return cur->fd_table[fd];
  }
  
  // fd_table에서 fd 슬롯 제거
  void remove_file_from_fdt(int fd) {
	struct thread *cur = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	  return;
	cur->fd_table[fd] = NULL;
  }
  // 파일 삭제하는 시스템 콜
// 성공일 경우 true, 실패일 경우 false 리턴
bool remove(const char *file) {			// file: 제거할 파일의 이름 및 경로 정보
	check_address(file);
	return filesys_remove(file);
}
// 현재 프로세스를 cmd_line에서 지정된 인수를 전달하여 이름이 지정된 실행 파일로 변경
int exec(char *file_name) {
	check_address(file_name);

	int file_size = strlen(file_name)+1;
	char *fn_copy = palloc_get_page(PAL_ZERO);
	if (fn_copy == NULL) {
		exit(-1);
	}
	strlcpy(fn_copy, file_name, file_size);

	if (process_exec(fn_copy) == -1) {
		return -1;
	}

	NOT_REACHED();
	return 0;
}