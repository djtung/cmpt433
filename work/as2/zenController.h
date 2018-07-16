// zenController.h
// Module to provide interaction with the Zen Cape including 
// reading from the potentiometer and converting the reading to 
// array size as per Assignment 2 descriptions, also driving the
// 14 Segment Display.

#ifndef _ZEN_CONTROLLER_H_
#define _ZEN_CONTROLLER_H_

// Begin/end the background thread which turns on the display
// and continually polls the potentiometer for "array size" value
// given by the assignment description.
// The caller should call both Zen_Display_init() and Zen_Pot_init() 
// first before attempting to spawn a thread
void Zen_startThread();
void Zen_stopThread();

// Initialize the system to be able to use the 14 segment display
// Enables I2C1 in the cape manager and exports the correct GPIO pins
// Returns 1 on success and 0 on failure
int Zen_Display_init();

// Set a number to show on the 14 segment display
int Zen_Display_setValue(int number);

// Initialize the system to be able to use the potentiometer
// Enables A2D in the cape manager
// Returns 1 on success and 0 on failure
int Zen_Pot_init();

// Returns an array size from the voltage reading and converted from the
// piecewise linear function
int Zen_Pot_getArraySize();

#endif