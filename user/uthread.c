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
            ut->context.sp = (uint64)&ut->ustack + STACK_SIZE;
            ut->state = RUNNABLE;
            return 0;
        }
    }

    return -1;
}

void uthread_yield()
{
    uthread_self()->state = RUNNABLE;

    transfer_control();
}

// change self pointer
// context switch
int transfer_control()
{
    printf("in transfering control\n");
    struct uthread *ut;

    struct uthread *next_ut;
    int found_runnable = 0;
    for (ut = uthreads; ut < &uthreads[MAX_UTHREADS]; ut++)
    {
        if (!found_runnable && ut->state == RUNNABLE)
        {
            next_ut = ut;
        }

        else if (ut->state == RUNNABLE && ut->priority > next_ut->priority)
        {
            next_ut = ut;
        }
    }

    printf("found next runnable\n");
    if (!ut)
    {
        printf("not good\n");
        exit(-1);
    }
    printf("get self\n");
    struct uthread *old = uthread_self();
    self = ut;
    printf("uswtch\n");
    uswtch(&old->context, &ut->context);
    return 0;
}

void uthread_exit()
{
    uthread_self()->state = FREE;

    int found_alive = 0;
    struct uthread *ut;

    for (ut = uthreads; ut < &uthreads[MAX_UTHREADS]; ut++)
    {
        if (ut->state == RUNNABLE || ut->state == RUNNING)
        {
            found_alive = 1;
        }
    }
    if (!found_alive)
    {
        exit(0);
    }

    transfer_control();
}

int uthread_start_all()
{
    printf("starting all\n");
    static int first = 1;

    if (first)
    {
        first = 0;
        printf("transfering control\n");
        transfer_control();
        return 0;
    }

    return -1;
}

enum sched_priority uthread_set_priority(enum sched_priority priority)
{
    // maybe add validation checks
    struct uthread *s = uthread_self();
    enum sched_priority previous_policy = s->priority;
    s->priority = priority;
    return previous_policy;
}
enum sched_priority uthread_get_priority()
{
    if (self != 0)
    {
        return self->priority;
    }
    return -1;
}

struct uthread *uthread_self()
{
    if (self == 0)
    {
        struct uthread *ut = {0};
        return ut;
    }
    return self;
}