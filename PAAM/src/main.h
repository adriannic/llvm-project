#pragma once

#include <stdint.h>
struct Context {
  uint64_t Ra;
  uint64_t Sp;

  uint64_t S0;
  uint64_t S1;
  uint64_t S2;
  uint64_t S3;
  uint64_t S4;
  uint64_t S5;
  uint64_t S6;
  uint64_t S7;
  uint64_t S8;
  uint64_t S9;
  uint64_t S10;
  uint64_t S11;
};

struct Spinlock {
  uint8_t Locked;
  struct Cpu *Cpu;
};

enum Procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct Proc {
  struct Spinlock Lock;
  struct Context Context;
  enum Procstate State;
};

struct Cpu {
  struct Proc *Proc;
  struct Context Context;
  int Noff;
  int Intena;
};

struct Cpu *mycpu();
void panic(char *Str);
int holding(struct Spinlock *Lk);
void acquire(struct Spinlock *Lk);
void release(struct Spinlock *Lk);
void swtch(struct Context *A, struct Context *B);
