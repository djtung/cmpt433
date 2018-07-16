// udpListener.h
// Module to create the socket which will provide a UI for the client
// to interact with the host running the sorter program.

#ifndef _UDP_LISTENER_H_
#define _UDP_LISTENER_H_

// Initialize the socket for UDP connection and
// starts the UDP Listener on the TARGET.
// The caller should pass in a mutex which will be unlocked by
// the thread when the program is to end
void Udp_startThread(void* arg);
void Udp_stopThread();

#endif