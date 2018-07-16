// udpListener.h
// Module to relay information to and from the C app to web server

#ifndef _UDP_LISTENER_H_
#define _UDP_LISTENER_H_

// Begin/end the background thread which starts listening on UDP socket
void Udp_startThread();
void Udp_stopThread();

#endif