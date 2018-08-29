#ifndef BARRIER_H
#define BARRIER_H

static void memory_barrier() { asm volatile("" ::: "memory");}

#endif // BARRIER_H
