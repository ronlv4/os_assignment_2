#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()) || kthread_killed(mykthread()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

uint64 sys_kthread_create(void)
{
  uint64 fn, stack, szp;
  // uint sz;

  argaddr(0, &fn);
  argaddr(1, &stack);
  argaddr(2, &szp);
  // sz = (uint)(*(uint64 *)szp);
  uint sz = MAX_STACK_SIZE;
  // cast fn to kthread_create first argument type
  void *(*start_func)(void *) = (void *(*)(void *))fn;
  
  // cast stack to kthread_create second argument type
  void *stack_ptr = (void *)stack;

  return kthread_create(start_func, stack_ptr, sz);
  }

uint64 sys_kthread_id(void)
{
  return mykthread()->tid;
}

uint64 sys_kthread_kill(void)
{
  int ktid;

  argint(0, &ktid);
  return kthread_kill(ktid);
}

uint64 sys_kthread_exit(void)
{
  int n;
  argint(0, &n);
  kthread_exit(n);
  return 0; // not reached
}

uint64 sys_kthread_join(void)
{
  int ktid;
  uint64 status;

  argint(0, &ktid);
  argaddr(1, &status);

  return kthread_join(ktid, status);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
