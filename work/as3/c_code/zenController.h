// zenController.h
// Module to provide interaction with the Zen Cape including 
// reading using the joystick and the accelerometer
// Also provides functionality for reading from BBG system files

#ifndef _ZEN_CONTROLLER_H_
#define _ZEN_CONTROLLER_H_

// Begin/end the background thread which starts the listeners
// for the joystick and accelerometer
void Zen_startThread();
void Zen_stopThread();

// Get the current uptime from /proc/uptime
int Zen_getUptime();

#endif