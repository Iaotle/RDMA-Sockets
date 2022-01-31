#include "isn.h"
#include "systems_headers.h"

#define ISN_UPDATE_INTERVAL 4

static uint32_t isn = 0;
static bool clock_started = false;
static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_t clock_thread;

static void* isn_clock(void* unused) {
	while(1) {
        if (usleep(ISN_UPDATE_INTERVAL) != 0) {
            perror("Timer usleep");
        }

		pthread_rwlock_wrlock(&lock);
		isn++;
		pthread_rwlock_unlock(&lock);
	}
}

int isn_start_clock() {
	time_t t;
	srand((unsigned) time(&t));
	isn = rand();
	if(clock_started) return -EALREADY;
	int ret = pthread_create(&clock_thread, NULL, isn_clock, NULL);
	if(ret) {
		perror("ISN clock thread creation failed");
	} else {
		clock_started = true;
	}
	return ret;
}

uint32_t isn_get() {
	pthread_rwlock_rdlock(&lock);
	uint32_t ret = isn;
	pthread_rwlock_unlock(&lock);
	return ret;
}
