#include "main.h"
#include <stdio.h>
#include <stdlib.h>

#define NPROC 16

void panic(char *Str) {
  fprintf(stderr, "%s\n", Str);
  exit(1);
}

int holding(struct Spinlock *Lk) { return Lk->Locked && Lk->Cpu == mycpu(); }

void acquire(struct Spinlock *Lk) {
  printf("Acquiring lock\n");
  if (holding(Lk))
    panic("acquire");

  while (__sync_lock_test_and_set(&Lk->Locked, 1) != 0)
    ;

  __sync_synchronize();

  Lk->Cpu = mycpu();
}

void release(struct Spinlock *Lk) {
  printf("Releasing lock\n");
  if (!holding(Lk))
    panic("release");

  Lk->Cpu = 0;

  __sync_synchronize();

  __sync_lock_release(&Lk->Locked);
}

struct Proc Proc[NPROC] = {0};
struct Cpu Cpus[6] = {0};

struct Cpu *mycpu(void) { return &Cpus[0]; }

void swtch(struct Context *A, struct Context *B) {
  printf("Changing context\n");
}

void scheduler(void) {
  struct Proc *P;
  struct Cpu *C = mycpu();

  C->Proc = 0;
  printf("Entering scheduler\n");
  for (int I = 0; I < 1; ++I) {
    for (P = Proc; P < &Proc[NPROC]; P++) {
      printf("\nChecking proc %ld\n", P - Proc);
      printf("Runnable? %d\n", P->State == RUNNABLE);

      acquire(&P->Lock);
      if (P->State == RUNNABLE) {
        printf("Running proc %ld\n", P - Proc);

        P->State = RUNNING;
        C->Proc = P;
        swtch(&C->Context, &P->Context);

        printf("Done running proc %ld\n", P - Proc);

        C->Proc = 0;
      }
      release(&P->Lock);
    }
  }
}

int main(void) {
  Proc[0].State = RUNNABLE;
  Proc[1].State = ZOMBIE;
  Proc[2].State = SLEEPING;
  Proc[3].State = RUNNABLE;
  Proc[4].State = RUNNABLE;

  scheduler();

  return 0;
}
