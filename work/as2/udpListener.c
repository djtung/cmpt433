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

#include "sorter.h"

#define MSG_MAX_LEN 1500
#define PORT 12345

#define SMALL_BUFFER_LEN 10
#define LARGE_BUFFER_LEN 20000

static pthread_t tid;
static int done = 0;
static pthread_mutex_t* mutexFromMain;

static void* udpThread(void* arg);
static int parseMessageAndFormatResponse(char* message);
static int handleGetArray(int sockfd, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

void Udp_startThread(void* arg) {
	mutexFromMain = arg;
	pthread_create(&tid, NULL, *udpThread, NULL);
}

void Udp_stopThread() {
	done = 1;
	pthread_join(tid, NULL);
}

static void* udpThread(void* arg) {
	printf("Connect using: \n");
	printf("netcat -u [BBG IP] %d\n", PORT);

	// Buffer to hold packet data:
	char message[MSG_MAX_LEN];

	// Address
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;                   // Connection may be from network
	sin.sin_addr.s_addr = htonl(INADDR_ANY);    // Host to Network long
	sin.sin_port = htons(PORT);                 // Host to Network short
	
	// Create the socket for UDP
	int socketDescriptor = socket(PF_INET, SOCK_DGRAM, 0);

	// Bind the socket to the port (PORT) that we specify
	bind (socketDescriptor, (struct sockaddr*) &sin, sizeof(sin));

	int command = 0;
	
	while (!done) {
		// Get the data (blocking)
		// Will change sin (the address) to be the address of the client.
		// Note: sin passes information in and out of call!
		unsigned int sin_len = sizeof(sin);
		int bytesRx = recvfrom(socketDescriptor, message, MSG_MAX_LEN, 0,
				(struct sockaddr *) &sin, &sin_len);

		if (bytesRx >= 1500) {
			printf("The command is too long. Type 'help' for a list of available commands.\n");
		} else {
			message[bytesRx] = 0;

			command = parseMessageAndFormatResponse(message);
			sin_len = sizeof(sin);
			switch(command) {
				default: // error occurred
				case 0:
				case 1:
				case 2:
				case 3:
					sendto(socketDescriptor, message, strlen(message), 0, (struct sockaddr *) &sin, sin_len);
					break;
				case 4:
					handleGetArray(socketDescriptor, 0, (struct sockaddr *) &sin, sin_len);
					break;
				case 5:
					// unlock mutexFromMain which will call all the stop thread functions
					pthread_mutex_unlock(mutexFromMain);
					done = 1;
					sendto(socketDescriptor, message, strlen(message), 0, (struct sockaddr *) &sin, sin_len);
					break;
			}
		}
	}

	// Close
	close(socketDescriptor);
	
	return NULL;
}

// Parse a message to find the right command
// Return values:
// -1: an error occurred
// 0: help
// 1: count
// 2: get #. The variable 'value' will be filled with the # argument
// 3: get length
// 4: get array
// 5: stop
//
// The correct response will be stored in 'message' except for in case 4. (get array)
// In this case 'message' will not be changed and the caller is responsible for ensuring
//  the correct response
static int parseMessageAndFormatResponse(char* message) {
	char* pch = strtok(message, " ");
	if (!pch) {
		sprintf(message, "Please enter a command.\nType 'help' for available options\n");
		return -1;
	}
	while (pch != NULL) {
		if (!strncmp(pch, "help", 4)) {
			sprintf(message, "Accepted command examples:\n\
count		-- display number arrays sorted.\n\
get length 	-- display length of array currently being sorted.\n\
get array 	-- display the full array being sorted.\n\
get 10 		-- display the tenth element of array currently being sorted.\n\
stop 		-- cause the server program to end\n");
			return 0;
		} else if (!strncmp(pch, "count", 5)) {
			sprintf(message, "Number of arrays sorted = %lld.\n", Sorter_getNumberArraysSorted());
			return 1;
		} else if (!strncmp(pch, "stop", 4)) {
			sprintf(message, "Program Terminating\n");
			return 5;
		} else if (!strncmp(pch, "get", 3)) {
			pch = strtok (NULL, " ");
			if (pch==NULL) {
				sprintf(message, "Enter a second argument to get.\nType 'help' for available options.\n");
				return -1;
			} else {
				if (!strncmp(pch, "length", 6)) {
					sprintf(message, "Current array length = %d\n", Sorter_getArrayLength());
					return 3;
				} else if (!strncmp(pch, "array", 5)) {
					return 4;
				} else {
					int num = atoi(pch);
					if (!num) {
						sprintf(message, "Unknown argument to 'get' or array index of 0.\n");
						return -1;
					} else {
						int length;
						int* arr = Sorter_getArrayData(&length);
						if (num <= 0 || num > length) {
							sprintf(message, "Invalid argument. Must be between 1 and %d (array length).\n", length);
						} else {
							sprintf(message, "Value %d = %d\n", num, arr[num-1]);
						}
						return 2;
					}
				}
			}
		}

		sprintf(message, "Unknown command.\nType 'help' for available options.\n");
		return -1;
	}

	//something wrong happened here
	sprintf(message, "something went horribly wrong\n");
	return -1;
}

static int handleGetArray(int sockfd, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
	char message[MSG_MAX_LEN] = {0};
	char arrayAsStr[LARGE_BUFFER_LEN] = {0};
	char buffer[SMALL_BUFFER_LEN] = {0};
	char* pch;
	int length;
	int i = 0;
	int* arr;
	int currlength = 0;
	int counter = 0;

	arr = Sorter_getArrayData(&length);

	for (i = 0; i < length; i++) {
		sprintf(buffer, "%d", arr[i]);
		strcat(arrayAsStr, buffer);
		currlength += strlen(buffer);

		strcat(arrayAsStr, " ");
		currlength += 1;
	}

	free(arr);

	arrayAsStr[currlength] = 0;

	pch = strtok(arrayAsStr, " ");
	currlength = 0;
	while (pch != NULL) {
		sprintf(buffer, "%s", pch);
		currlength += strlen(buffer);
		if (currlength >= MSG_MAX_LEN-5) { //digits can be up to 4 long
			message[currlength] = '\n';
			message[currlength+1] = 0;
			sendto(sockfd, message, strlen(message), flags, dest_addr, addrlen);
			memset(message, 0, MSG_MAX_LEN);
			currlength = 0 + strlen(buffer);
		}
		strcat(message, buffer);
		strcat(message, ", ");
		currlength += 2;
		counter++;
		if (counter >= 9) {
			strcat(message, "\n");
			currlength++;
			counter = 0;
		}
		pch = strtok(NULL, " ");
	}

	message[currlength] = '\n';
	message[currlength+1] = 0;

	sendto(sockfd, message, strlen(message), flags, dest_addr, addrlen);

	return 1;
}