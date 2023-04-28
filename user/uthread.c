#include "kernel/types.h"
#include "user/user.h"
#include "user/uthread.h"

struct uthread uthreads[MAX_UTHREADS];

struct uthread *self;

int thread_count = 0;


int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    struct uthread *t;
    
    for (t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        t->state = FREE;
        t->priority = priority;

        t->context.ra = (uint64)start_func;
        t->context.sp = (uint64)t->ustack + STACK_SIZE;
    }

    return 0;
}

void uthread_yield()
{
    self->state = RUNNABLE;

    transfer_control();
}


// change self pointer
// context switch
// min accumulator
// accumulator update
int transfer_control()
{
    return 0;
}

void uthread_exit()
{
    self->state = FREE;

    if (thread_count == 0)
    {
        exit(0);
    }

    transfer_control();
}

int uthread_start_all()
{
    return 0;
}


enum sched_priority uthread_set_priority(enum sched_priority priority)
{
    // maybe add validation checks
    enum sched_priority previous_policy = self->priority;
    self->priority = priority;
    return previous_policy;
}
enum sched_priority uthread_get_priority()
{
    return self->priority;
}

struct uthread* uthread_self()
{
    return self;
}