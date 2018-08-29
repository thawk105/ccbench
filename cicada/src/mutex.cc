#include <pthread.h>
#include "include/common.hpp"
#include "include/debug.hpp"

void mutexInit()
{
	if (pthread_mutex_init(&Lock, NULL)) {
			printf("mutexLockError\n");
			exit(1);
	}
}

void mutexInit(pthread_mutex_t& lock)
{
	if (pthread_mutex_init(&lock, NULL)) {
		ERR;
	}
}
