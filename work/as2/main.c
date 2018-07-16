// Main program for driving as2
// Calls all the separate modules and waits for a signal from the UDP
// module which will unlock the mutex and do clean up

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "sorter.h"
#include "zenController.h"
#include "udpListener.h"

static pthread_mutex_t mutexMain = PTHREAD_MUTEX_INITIALIZER;

int main() {
	Zen_Display_init();
	Zen_Pot_init();
	Zen_startThread();
	Sorter_startSorting();
	Udp_startThread(&mutexMain);

	pthread_mutex_lock(&mutexMain);
	pthread_mutex_lock(&mutexMain);

	pthread_mutex_unlock(&mutexMain);

	Zen_stopThread();
	Sorter_stopSorting();
	Udp_stopThread();

	return 0;
}