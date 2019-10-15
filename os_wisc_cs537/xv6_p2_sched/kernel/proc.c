#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// two-level scheduler
struct two_level_picker {
    struct spinlock lock;
    int rsv_idx[200];       // mark pid based on tickets
    int total_percent;
    int bid_idx[NPROC];     // a sorted list of bidders
    int rr_ptr;
} picker;

// helper functions
void reset_sched_info(struct proc *p) {
    p->percent = 0;
    p->bid = -1;
    p->chosen = 0;
    p->time = 0;
    p->charge_micro = 0;
    p->charge_nano = 0;
}

int rand_int(void) {
    static uint seed = 100;
    // linear congurential generator
    const uint a = 1103515245;  // param from glibc
    const uint c = 12345;
    seed = (a * seed + c) % ((uint)(1) << 30);
//    const uint a = 1664525;
//    const uint c = 1013904223;
//    seed = (a * seed + c);
    return (seed % 200);
}

void swap(int *e1, int *e2) {
    int temp = *e1;
    *e1 = *e2;
    *e2 = temp;
}

int get_bid(int idx) {
    return ptable.proc[picker.bid_idx[idx]].bid;
}

void check_bid_array() {
    for (int i = 0; i < NPROC; ++i) {
        if (picker.bid_idx[i] >= 0) {
            cprintf("%d  ", get_bid(i));
        }
    }
    cprintf("\n");
}

// book keeping
void picker_init(void) {
    initlock(&picker.lock, "picker");
    for (int i = 0; i < 200; ++i) {
        picker.rsv_idx[i] = -1;
    }
    picker.total_percent = 0;
    for (int i = 0; i < NPROC; ++i) {
        picker.bid_idx[i] = -1;
    }
    picker.rr_ptr = 0;
}

void add_new_bid(int p_idx) {
    // default w/ bid=0
    if (ptable.proc[p_idx].bid < 0) panic("invalid bid value");
    // insert to first invalid position and then swap left
    int i = 0, j = 0;
    for (i = 0; i < NPROC; ++i) {
        if (picker.bid_idx[i] < 0) {
            break;
        }
    }
    if (i == NPROC) panic("too many processes");
    picker.bid_idx[i] = p_idx;
    for (j = i; j > 0; --j) {
        if (get_bid(j) > get_bid(j-1)) {
            swap(&picker.bid_idx[j], &picker.bid_idx[j-1]);
        }
    }
}

void update_bid(int p_idx, int upgrade) {
    if (ptable.proc[p_idx].bid < 0) panic("invalid bid value");
    // sort it
    int i = 0, j = 0;
    for (i = 0; i < NPROC; ++i) {
        if (p_idx == picker.bid_idx[i]) {
            break;
        }
    }
    if (i == NPROC) panic("cannot find the process to update bid value");
    if (upgrade > 0) {
        // moving left
        for (j = i; j > 0; --j) {
            if (get_bid(j) > get_bid(j-1)) {
                swap(&picker.bid_idx[j], &picker.bid_idx[j-1]);
            }
        }
    } else {
        // moving right
        for (j = i; j < NPROC-1; ++j) {
            if (picker.bid_idx[j+1] >= 0 && get_bid(j) < get_bid(j+1)) {
                swap(&picker.bid_idx[j], &picker.bid_idx[j+1]);
            }
        }
    }
}

void remove_bid(int p_idx) {
    if (ptable.proc[p_idx].bid >= 0) panic("invalid bid value");
    // reset the postion to then swap right
    int i = 0, j = 0;
    for (i = 0; i < NPROC; ++i) {
        if (p_idx == picker.bid_idx[i]) {
            break;
        }
    }
    if (i == NPROC) panic("cannot find the process to remove bid value");
    picker.bid_idx[i] = -1;   // reset
    for (j = i; j < NPROC-1; ++j) {
        if (picker.bid_idx[j+1] >= 0) {
            swap(&picker.bid_idx[j], &picker.bid_idx[j+1]);
        }
    }
}

void update_reserve() {
    int i = 0, j = 0, k = 0;
    // re-calculate
    picker.total_percent = 0;
    for (i = 0; i < NPROC; ++i) {
        if (ptable.proc[i].percent > 0) {
            picker.total_percent += ptable.proc[i].percent;
            for (j = 0; j < ptable.proc[i].percent; ++j) {
                picker.rsv_idx[k] = i;
                ++k;
            }
        }
    }
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  struct proc *p;
  int idx;
  for(p = ptable.proc, idx = 0; p < &ptable.proc[NPROC]; p++, idx++) {
      p->ptable_idx = idx;
  }
  picker_init();
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  int idx;
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc, idx = 0; p < &ptable.proc[NPROC]; p++, idx++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;  // set PC to entry point of forkret

  // by default, new process has a bid of 0
  acquire(&picker.lock);
  reset_sched_info(p);
  p->bid = 0;
  add_new_bid(idx);
  release(&picker.lock);

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  // TODO: should this be special and have a high bid value?
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    // just called allocproc, need cleanup
    acquire(&picker.lock);
    remove_bid(np->ptable_idx);
    release(&picker.lock);
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;  // trap frame include eip, i.e. PC is set to the same as parent

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);  // drop pointer reference to inode
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;   // that's why we do not expect initproc to exit
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  // cleanup on exit
  acquire(&picker.lock);
  if (proc->percent > 0) {
      proc->percent = 0;
      update_reserve();
  } else {
      proc->bid = -1;
      remove_bid(proc->ptable_idx);
  }
  release(&picker.lock);
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack); // each thread/process has its kernel stack
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        reset_sched_info(p);
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  } // infinite loop
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  int found;

  for(;;){
    // infinite loop never returns

    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    acquire(&picker.lock);

    found = 0;
    // level-1: lottery scheduling
    const int num = rand_int();
    const int ptable_idx = picker.rsv_idx[num];
    if (ptable_idx >= 0) {
        p = &ptable.proc[ptable_idx];
        if (p->state == RUNNABLE) {
            //cprintf("pick from reserve pool idx=%d pid=%d percent=%d bid=%d rand_num=%d\n",
            //        ptable_idx, p->pid, p->percent, p->bid, num);
            found = 1;
        }
    }
    // level-2: give highest bidder
    if (!found) {
        for (int i = 0; i < NPROC; ++i) {
            const int idx = picker.bid_idx[i];
            if (idx >= 0) {
                p = &ptable.proc[idx];
                if (p->state == RUNNABLE) {
                    //cprintf("pick from bid pool idx=%d pid=%d percent=%d bid=%d rand_num=%d\n",
                    //        idx, p->pid, p->percent, p->bid, num);
                    found = 2;
                    break;
                }
            }
        }
    }
    // step 3: if no bidder, try not losing this slot
    if (!found) {
        for (int i = 0; i < NPROC; ++i) {
            const int idx = (picker.rr_ptr + i) % NPROC;
            p = &ptable.proc[idx];
            if (p->state == RUNNABLE) {
                //cprintf("pick from leftover idx=%d pid=%d percent=%d bid=%d rand_num=%d\n",
                //        idx, p->pid, p->percent, p->bid, num);
                found = 3;
                picker.rr_ptr = (i+1) % NPROC; // advance the round-robin pointer
                break;
            }
        }
    }
    release(&picker.lock);

    if (found) {
      // stats
      p->chosen += 1;
      p->time +=10;   // in ms, simple assumption here w/o e.g. IO
      if (found == 1) {
          p->charge_micro += 1; // i.e. 100 nanodollar per ms
      } else if (found == 2) {
          p->charge_nano += 25 * p->bid;
          if (p->charge_nano > 1000) {
              p->charge_micro += 1;
              p->charge_nano -= 1000;
          }
      }

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);  // (**old, *new)
      switchkvm();  // back to kernel mode

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);  // enter scheduler; from exit, yield, sleep
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;      // set the sleep condition
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;  // only mark killed here
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// APIs
int proc_reserve(int percent) {
    // do some sanity check
    if (percent <= 0 || percent > 100) {
        return -1;
    }
    acquire(&picker.lock);
    if (picker.total_percent + percent > 200) {
        release(&picker.lock);
        return -1;
    }
    // proceed
    proc->percent = percent;
    proc->bid = -1;
    update_reserve();
    remove_bid(proc->ptable_idx);
    release(&picker.lock);
    return 0;
}

int proc_spot(int bid) {
    // sanity checks
    if (bid < 0) {
        return -1;
    }
    acquire(&picker.lock);
    if (proc->percent > 0) {
        proc->percent = 0;
        update_reserve();
        proc->bid = bid;
        add_new_bid(proc->ptable_idx);
    } else {
        const int upgrade = (bid > proc->bid) ? 1 : 0;
        proc->bid = bid;
        update_bid(proc->ptable_idx, upgrade);
    }
    release(&picker.lock);
    return 0;
}

int proc_getpinfo(struct pstat *stat) {
    for (int i = 0; i < NPROC; ++i) {
        stat->inuse[i] = 0;
        stat->pid[i] = 0;
        stat->chosen[i] = 0;
        stat->time[i] = 0;
        stat->charge[i] = 0;
        stat->percent[i] = 0;
        stat->bid[i] = 0;
        struct proc *p = &ptable.proc[i];
        if (p->state > UNUSED) {
            stat->inuse[i] = 1;
            stat->pid[i] = p->pid;
            stat->chosen[i] = p->chosen;
            stat->time[i] = p->time;
            stat->charge[i] = p->charge_micro;
            stat->percent[i] = p->percent;
            stat->bid[i] = p->bid;
        }
    }
    return 0;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
  release(&ptable.lock);
}


