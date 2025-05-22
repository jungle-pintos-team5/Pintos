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
	printf ("system call!\n");
	switch (f->R.rax)
	{
	case SYS_HALT:
			halt(); // 핀토스 종료
		break;
	case SYS_EXIT:
			exit(f->R.rdi);	// 프로세스 종료
		break;
	case SYS_FORK:
		break;
	case SYS_EXEC:
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		break;
	case SYS_OPEN:
		break;
	case SYS_FILESIZE:
		break;
	case SYS_READ:
		break;
	case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		break;
	case SYS_TELL:
		break;
	case SYS_CLOSE:
		break;
	default:
		printf ("system call!\n");
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

	printf("%s: exit[%d]\n", thread_name(), status); 
	thread_exit();	
}

int write (int fd, const void *buffer, unsigned size) {
  // fd가 1이면 표준 출력
  if (fd == 1) {
    // putbuf: 커널 콘솔에 buffer의 내용을 size만큼 출력
    putbuf(buffer, size);
    return size;  // 출력한 바이트 수 반환
  }

  return -1;
}

bool create(const char *file, unsigned initial_size){
	check_addr(file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file){
	check_addr(file);
	return filesys_remove (file);
}
