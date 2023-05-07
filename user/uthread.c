#include "kernel/types.h"
#include "user/user.h"
#include "user/uthread.h"

struct uthread uthreads[MAX_UTHREADS];

struct uthread *self;

int thread_count = 0;

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    struct uthread *ut;

    for (ut = uthreads; ut < &uthreads[MAX_UTHREADS]; ut++)
    {
        if (ut->state == FREE)
        {
            ut->priority = priority;
            memset(&ut->context, 0, sizeof(ut->context));
            ut->context.ra = (uint64)start_func;
            ut->context.sp = (uint64)ut->ustack + STACK_SIZE;
            ut->state = RUNNABLE;
            return 0;
        }
    }

    return -1;
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
    struct uthread *ut;

    for (ut = uthreads; ut < &uthreads[MAX_UTHREADS]; ut++)
    {
        if (ut->state == RUNNABLE)
        {
            struct uthread *old = self;
            self = ut;
            uswtch(&old->context, &ut->context);
        }
    }
    return 0;
}

void uthread_exit()
{
    self->state = FREE;

    int found_alive = 0;
    struct uthread *ut;

    for (ut = uthreads; ut < &uthreads[MAX_UTHREADS]; ut++)
    {
        if (ut
    }
    if (thread_count == 0)
    {
        exit(0);
    }

    transfer_control();
}

int uthread_start_all()
{
    static int first = 1;

    if (first)
    {
        first = 0;
        (*(void (*)())(self->context.ra))();
        return 0;
    }

    return -1;

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

struct uthread *uthread_self()
{
    return self;
}