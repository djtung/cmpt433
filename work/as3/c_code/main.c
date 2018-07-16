#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "audioMixer.h"
#include "zenController.h"
#include "udpListener.h"

int main() {

	AudioMixer_init();
	Zen_startThread();
	Udp_startThread();

	// there isn't a specified way to close the application
	// just wait an inordinately large amount of time here
	nanosleep((const struct timespec[]){{3600, 0}}, NULL);

	Udp_stopThread();
	Zen_stopThread();
	AudioMixer_cleanup();

	return 0;
}