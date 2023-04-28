#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];
extern void forkret(void);


void kthreadinit(struct proc *p)
{
  initlock(&p->tid_lock, "nexttid");

  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->lock, "thread");
    kt->tstate = UNUSED;
    kt->proc = p;

    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

struct kthread *mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->thread;
  pop_off();
  return kt;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
struct kthread* allocthread(struct proc *p)
{
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    acquire(&kt->lock);
    if(kt->tstate == UNUSED) {
      goto found;
    } else {
      release(&kt->lock);
    }
  }
  return 0;

found:
  kt->tid = alloctid(p);
  kt->tstate = USED;

  // Allocate a trapframe page.
  if((kt->trapframe = (struct trapframe *)kalloc()) == 0){
    freethread(kt);
    release(&kt->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = get_kthread_trapframe(p, kt)->kernel_sp;

  return kt;
}

// free a kthread structure and the data hanging from it,
// kt->lock must be held.
void freethread(struct kthread *kt)
{
  acquire(&kt->lock);
  if(kt->trapframe)
    kfree((void*)kt->trapframe);
  kt->trapframe = 0;
  kt->tstate = UNUSED;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->tid = 0;
  kt->proc = 0;
  release(&kt->lock);
}

int alloctid(struct proc *p)
{
  int tid;
  
  acquire(&p->tid_lock);
  tid = p->next_tid;
  p->next_tid = tid + 1;
  release(&p->tid_lock);

  return tid;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int kthread_exit(int status)
{
  struct proc *p = myproc();
  struct kthread *kt;

  int found_alive = 0;

  acquire(&p->lock);
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    if (kt != mykthread() && kt->tstate != ZOMBIE)
    {
      found_alive = 1;
    }
    release(&kt->lock);
  }

  if (!found_alive)
  {
    exit(status); // never to return
  }

  acquire(&kt->lock);
  kt->xstate = status;
  kt->tstate = ZOMBIE;
  release(&kt->lock);

  return 0;
}

int wakeup_all_threads(struct proc *p)
{
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    kt->killed = 1;
    if (kt->tstate == SLEEPING)
    {
      kt->tstate = RUNNABLE;
    }
    release(&kt->lock);
  }
  return 0;
}

int kthread_kill(int ktid)
{
  struct proc *p = myproc();
  struct kthread *kt;

  acquire(&p->lock);
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    if (kt->tid == ktid)
    {
      kt->killed = 1;
      if (kt->tstate == SLEEPING)
      {
        kt->tstate = RUNNABLE;
      }
      return 0;
    }
    release(&kt->lock);
  }
  release(&p->lock);
  return -1; // no matching tid found within process
}

int kthread_killed(struct kthread *kt)
{
  int k;
  
  acquire(&kt->proc->lock);
  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  release(&kt->proc->lock);
  return k;
}

void setkthreadkilled(struct kthread *kt)
{
  acquire(&kt->proc->lock);
  acquire(&kt->lock);
  kt->killed = 1;
  release(&kt->lock);
  release(&kt->proc->lock);
}

// calling thread holding lock
int exit_threads(struct proc *p, int status)
{
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    if (kt != mykthread())
    {
      acquire(&kt->lock);
      kt->xstate = status;
      kt->tstate = ZOMBIE;
      release(&kt->lock);
    }
  }
  return 0;
}

int kthread_join(uint64 addr)
{
  return 0;
}