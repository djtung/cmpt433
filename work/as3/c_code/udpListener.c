//
// UDP Listening program on port 12345
// Heavily adapted from demo program by Brian Fraser
//

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "audioMixer.h"
#include "zenController.h"

#define MSG_MAX_LEN 200
#define PORT 12345

static pthread_t tid;
static int done = 0;

static struct sockaddr_in sin;
static int socketDescriptor;

static void* udpThreadForServer(void* arg);
static void formatResponse(char* message);
static void doAction(int command);

void Udp_startThread(void* arg) {
	// Address
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;                   // Connection may be from network
	sin.sin_addr.s_addr = htonl(INADDR_ANY);    // Host to Network long
	sin.sin_port = htons(PORT);                 // Host to Network short
	
	// Create the socket for UDP
	socketDescriptor = socket(PF_INET, SOCK_DGRAM, 0);

	// Bind the socket to the port (PORT) that we specify
	bind (socketDescriptor, (struct sockaddr*) &sin, sizeof(sin));

	pthread_create(&tid, NULL, *udpThreadForServer, NULL);
}

void Udp_stopThread() {
	done = 1;
	pthread_join(tid, NULL);
}

static void* udpThreadForServer(void* arg) {
	// Buffer to hold packet data:
	char message[MSG_MAX_LEN];
	int command = 0;

	while (!done) {
		// Get the data (blocking)
		// Will change sin (the address) to be the address of the client.
		// Note: sin passes information in and out of call!
		unsigned int sin_len = sizeof(sin);
		int bytesRx = recvfrom(socketDescriptor, message, MSG_MAX_LEN, 0,
				(struct sockaddr *) &sin, &sin_len);

		message[bytesRx] = 0;

		command = atoi(message);
		doAction(command);

		// always send a status message to the server which 
		// holds all information about the beaglebone
		formatResponse(message);
		sin_len = sizeof(sin);
		sendto(socketDescriptor, message, strlen(message), 0, (struct sockaddr *) &sin, sin_len);
	}

	// Close
	close(socketDescriptor);
	
	return NULL;
}

// server will receive a message of this response:
// "beatType volume bpm procTime" (all ints)
static void formatResponse(char* message) {
	sprintf(message, "%d %d %d %d", AudioMixer_getBeat(), AudioMixer_getVolume(), AudioMixer_getBPM(), Zen_getUptime());
}
// Incoming messages from the zen controller:
// "1" : change mode to none
// "2" : change mode to rock
// "3" : change mode to custom
// "4" : decrement volume by 5
// "5" : increment volume by 5
// "6" : decrement bpm by 5
// "7" : increment bpm by 5
// "8" : get the current proc time (status call)
static void doAction(int command) {
	int temp = 0;

	switch(command) {
		case 1:
			AudioMixer_setBeat(0);
			break;
		case 2:
			AudioMixer_setBeat(1);
			break;
		case 3:
			AudioMixer_setBeat(2);
			break;
		case 4:
			temp = AudioMixer_getVolume()-5;
			AudioMixer_setVolume(temp);
			break;
		case 5:
			temp = AudioMixer_getVolume()+5;
			AudioMixer_setVolume(temp);
			break;
		case 6:
			temp = AudioMixer_getBPM()-5;
			AudioMixer_setBPM(temp);
			break;
		case 7:
			temp = AudioMixer_getBPM()+5;
			AudioMixer_setBPM(temp);
			break;
		case 8:
			// calling function will also handle updating the server with this info
		default:
			break;
	}
}