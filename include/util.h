#pragma once
static void compiler_fence() { asm volatile("" ::: "memory");}
