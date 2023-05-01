#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int tid = kthread_id();
    fprintf(2, "%d\n", tid);
    exit(0);
}