#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];
extern void forkret(void);

extern struct spinlock join_lock;

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

struct kthread *mykthread(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->thread;
  pop_off();
  return kt;
}

// Look in the kthread table for an UNUSED kthread.
// If found, initialize state required to run in the kernel,
// and return with kt->lock held.
// If there are no free kthreads, or a memory allocation fails, return 0.
struct kthread *allocthread(struct proc *p)
{
  struct kthread *kt;

  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    if (kt->tstate == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&kt->lock);
    }
  }
  return 0;

found:
  kt->tid = alloctid(p);
  kt->tstate = USED;
  kt->trapframe = get_kthread_trapframe(p, kt);

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = kt->kstack + PGSIZE;

  return kt;
}

// free a kthread structure and the data hanging from it,
// kt->lock must be held.
void freethread(struct kthread *kt)
{
  kt->trapframe = 0;
  kt->tstate = UNUSED;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->tid = 0;
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

int kthread_create( void *(*start_func)(), void *stack, uint stack_size)
{
  struct proc *p = myproc();
  struct kthread *kt;

  if ((kt = allocthread(p)) == 0)
  {
    return -1;
  }

  // Set up new context to start executing at start_func,
  // which returns to user space.
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)start_func;
  kt->context.sp = (uint64)stack + PGSIZE;

  return kt->tid;
}

int start_func_wrapper(void (*start_func)())
{
  start_func();
  kthread_exit(0);
  return 0;
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
  release(&p->lock);

  if (!found_alive)
  {
    exit(status); // never to return
  }

  kthread_wakeup(kt);

  acquire(&kt->lock);
  kt->xstate = status;
  kt->tstate = ZOMBIE;
  release(&kt->lock);

  return 0;
}

int kthread_wakeup(void *chan)
{
  struct kthread *kt;
  struct proc *p = myproc();

  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    if (kt->tstate == SLEEPING && kt->chan == chan)
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

  return -1; // no matching tid found within process
}

int kthread_killed(struct kthread *kt)
{
  int k;

  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  return k;
}

void setkthreadkilled(struct kthread *kt)
{
  acquire(&kt->lock);
  kt->killed = 1;
  release(&kt->lock);
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

int kthread_join(int ktid, uint64 addr)
{
  struct proc *p;

  // find the kthread with the given tid
  struct kthread *jkt;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    for (jkt = p->kthread; jkt < &p->kthread[NKT]; jkt++)
    {
      acquire(&jkt->lock);
      if (jkt->tid == ktid)
      {
        goto found;
      }
      release(&jkt->lock);
    }
  }

  return -1; // no matching tid found

found:
  release(&jkt->lock);

  acquire(&join_lock);

  for (;;)
  {
    acquire(&jkt->lock);
    if (jkt->tstate == ZOMBIE)
    {
      if (addr != 0 && copyout(p->pagetable, addr, (char *)&jkt->xstate, sizeof(jkt->xstate)) < 0)
      {
        release(&join_lock);
        return -1;
      }
      freethread(jkt);
      release(&jkt->lock);
      release(&join_lock);
      return 0;
    }

    release(&jkt->lock);
    if (kthread_killed(jkt))
    {
      release(&join_lock);
      return -1;
    }

    sleep(jkt, &join_lock);
  }
}