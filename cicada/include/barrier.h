#pragma once

static void memory_barrier() { asm volatile("" ::: "memory");}
