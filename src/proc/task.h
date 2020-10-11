#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stddef.h>
#include <mm/mm.h>
#include <lib/lock.h>
#include <lib/time.h>
#include <lib/types.h>
#include <lib/signal.h>

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define MAX_TASKS (MAX_PROCESSES*16)
#define MAX_FILE_HANDLES 256

#define CURRENT_PROCESS cpu_locals[current_cpu].current_process
#define CURRENT_THREAD cpu_locals[current_cpu].current_thread
#define CURRENT_TASK cpu_locals[current_cpu].current_task

struct regs_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct ctx_t {
    struct regs_t regs;
    uint8_t *fxstate;
};

struct thread_t {
    tid_t tid;
    tid_t task_id;
    pid_t process;
    lock_t lock;
    int in_syscall;
    int last_syscall;
    int event_abrt;
    uint64_t yield_target;
    int paused;
    int active_on_cpu;
    size_t kstack;
    size_t ustack;
    size_t thread_errno;
    size_t fs_base;
    struct ctx_t ctx;
    event_t **event_ptr;
    int *out_event_ptr;
    size_t event_timeout;
    int event_num;
};

#define AT_ENTRY 10
#define AT_PHDR 20
#define AT_PHENT 21
#define AT_PHNUM 22

struct auxval_t {
    size_t at_entry;
    size_t at_phdr;
    size_t at_phent;
    size_t at_phnum;
};

struct child_event_t {
    pid_t pid;
    int status;
};

struct process_t {
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    uid_t uid;
    struct pagemap_t *pagemap;
    struct thread_t **threads;
    char cwd[2048];
    lock_t cwd_lock;
    int *file_handles;
    lock_t file_handles_lock;
    size_t cur_brk;
    lock_t cur_brk_lock;
    struct child_event_t *child_events;
    size_t child_event_i;
    lock_t child_event_lock;
    event_t child_event;
    lock_t usage_lock;
    struct rusage_t own_usage;
    struct rusage_t child_usage;
    struct sigaction signal_handlers[SIGNAL_MAX];
    sigset_t sigmask;
};

int task_send_child_event(pid_t, struct child_event_t *);

extern int64_t task_count;

extern lock_t scheduler_lock;

extern struct process_t **process_table;
extern struct thread_t **task_table;

void init_sched(void);
void yield(void);
void relaxed_sleep(uint64_t);

enum tcreate_abi {
    tcreate_fn_call,
    tcreate_elf_exec
};

struct tcreate_fn_call_data{
    void *fsbase;
    void (*fn)(void *);
    void *arg;
};

struct tcreate_elf_exec_data {
    void *entry;
    const char **argv;
    const char **envp;
    const struct auxval_t *auxval;
};

#define tcreate_fn_call_data(fsbase_, fn_, arg_) \
    &((struct tcreate_fn_call_data){.fsbase=fsbase_, .fn=fn_, .arg=arg_})
#define tcreate_elf_exec_data(entry_, argv_, envp_, auxval_) \
    &((struct tcreate_elf_exec_data){.entry=entry_, .argv=argv_, .envp=envp_, .auxval=auxval_})

tid_t task_tcreate(pid_t, enum tcreate_abi, const void *);
pid_t task_pcreate(void);
int task_tkill(pid_t, tid_t);
int task_tpause(pid_t, tid_t);
int task_tresume(pid_t, tid_t);

void force_resched(void);

int kill(pid_t, int);

#endif
