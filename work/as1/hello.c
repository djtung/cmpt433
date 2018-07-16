// Jonathan Tung 301220825
// Assignment 1
// Hello world and Joystick Game
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define TRIGGER_FILE0 "/sys/class/leds/beaglebone:green:usr0/trigger"
#define TRIGGER_FILE1 "/sys/class/leds/beaglebone:green:usr1/trigger"
#define TRIGGER_FILE2 "/sys/class/leds/beaglebone:green:usr2/trigger"
#define TRIGGER_FILE3 "/sys/class/leds/beaglebone:green:usr3/trigger"

#define BRIGHTNESS_FILE0 "/sys/class/leds/beaglebone:green:usr0/brightness"
#define BRIGHTNESS_FILE1 "/sys/class/leds/beaglebone:green:usr1/brightness"
#define BRIGHTNESS_FILE2 "/sys/class/leds/beaglebone:green:usr2/brightness"
#define BRIGHTNESS_FILE3 "/sys/class/leds/beaglebone:green:usr3/brightness"

#define GPIO_EXPORT_PATH "/sys/class/gpio/export"

#define JOY_FILE_UP_DIRECTION "/sys/class/gpio/gpio26/direction"
#define JOY_FILE_RIGHT_DIRECTION "/sys/class/gpio/gpio47/direction"
#define JOY_FILE_DOWN_DIRECTION "/sys/class/gpio/gpio46/direction"
#define JOY_FILE_LEFT_DIRECTION "/sys/class/gpio/gpio65/direction"

#define JOY_FILE_UP "/sys/class/gpio/gpio26/value"
#define JOY_FILE_RIGHT "/sys/class/gpio/gpio47/value"
#define JOY_FILE_DOWN "/sys/class/gpio/gpio46/value"
#define JOY_FILE_LEFT "/sys/class/gpio/gpio65/value"

#define GPIO_JSUP 26
#define GPIO_JSRT 47
#define GPIO_JSDN 46
#define GPIO_JSLFT 65

#define DIRECTION_IN "in"
#define TRIGGER_NONE "none"

#define LED_ON "1"
#define LED_OFF "0"

#define LED_FLASH_DELAY_nS 100000000L

// opens a file for writing, returns the pointer to file object
FILE* openFileWrite(const char* filename) {
	FILE *pFile = fopen(filename, "w");

	if (pFile == NULL) {
		printf("ERROR OPENING %s.", filename);
	}

	return pFile;
}

// opens a file for reading, returns the pointer to file object
FILE* openFileRead(const char* filename) {
	FILE *pFile = fopen(filename, "r");

	if (pFile == NULL) {
		printf("ERROR OPENING %s.", filename);
	}

	return pFile;
}

// writes the "none" trigger to the specified file
int writeTriggerNone(FILE* file) {
	int charWritten = fprintf(file, TRIGGER_NONE);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// writes 0 to the specified file
int writeOff(FILE* file) {
	int charWritten = fprintf(file, LED_OFF);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// writes 1 to the specified file
int writeOn(FILE* file) {
	int charWritten = fprintf(file, LED_ON);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// writes "in" to the specified file
int writeIn(FILE* file) {
	int charWritten = fprintf(file, DIRECTION_IN);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// for the 4 joystick directions, write them to the export
int writeGPIO(FILE* file) {
	int charWritten = fprintf(file, "%d", GPIO_JSUP);
	fflush(file);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return charWritten;
	}

	charWritten = fprintf(file, "%d", GPIO_JSRT);
	fflush(file);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return charWritten;
	}

	charWritten = fprintf(file, "%d", GPIO_JSDN);
	fflush(file);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return charWritten;
	}

	charWritten = fprintf(file, "%d", GPIO_JSLFT);
	fflush(file);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

int closeFile(FILE* stream) {
	return fclose(stream);
}

// get next input from the joystick. Waits for depression before returning
int getJSInput() {
	char bufUp[5], bufRight[5], bufDown[5], bufLeft[5];
	int result = 0;
	int shouldExit = 0;
	FILE* jsUp = NULL;
	FILE* jsRight = NULL;
	FILE* jsDown = NULL;
	FILE* jsLeft = NULL;

	// poll for direction press
	while (1) {
		jsUp = openFileRead(JOY_FILE_UP);
		jsRight = openFileRead(JOY_FILE_RIGHT);
		jsDown = openFileRead(JOY_FILE_DOWN);
		jsLeft = openFileRead(JOY_FILE_LEFT);

		fgets(bufUp, 5, jsUp);
		fgets(bufRight, 5, jsRight);
		fgets(bufLeft, 5, jsLeft);
		fgets(bufDown, 5, jsDown);

		if (!atoi(bufUp)) {
			result = 1;
			break;
		} else if (!atoi(bufRight)) {
			result = 2;
			break;
		} else if (!atoi(bufDown)) {
			result = 3;
			break;
		} else if (!atoi(bufLeft)) {
			result = 4;
			break;
		}

		closeFile(jsUp);
		closeFile(jsRight);
		closeFile(jsDown);
		closeFile(jsLeft);
	}

	// wait till direction is de-pressed
	switch(result) {
		case 1:
			do {
				jsUp = openFileRead(JOY_FILE_UP);
				fgets(bufUp, 5, jsUp);
				if (atoi(bufUp)) {
					shouldExit = 1;
				}
				closeFile(jsUp);
			} while (!shouldExit);
			return 1;
		case 2:
			do {
				jsRight = openFileRead(JOY_FILE_RIGHT);
				fgets(bufRight, 5, jsRight);
				if (atoi(bufRight)) {
					shouldExit = 1;
				}
				closeFile(jsRight);
			} while (!shouldExit);
			return 0;
		case 3:
			do {
				jsDown = openFileRead(JOY_FILE_DOWN);
				fgets(bufDown, 5, jsDown);
				if (atoi(bufDown)) {
					shouldExit = 1;
				}
				closeFile(jsDown);
			} while (!shouldExit);
			return 2;
		case 4:
			do {
				jsLeft = openFileRead(JOY_FILE_LEFT);
				fgets(bufLeft, 5, jsLeft);
				if (atoi(bufLeft)) {
					shouldExit = 1;
				}
				closeFile(jsLeft);
			} while (!shouldExit);
			return 0;
	}

	return -1;
}

// initialize starting LED trigger and GPIOs state
void init() {
	srand(time(NULL));

	// init leds to off
	FILE* trigger0 = openFileWrite(TRIGGER_FILE0);
	FILE* trigger1 = openFileWrite(TRIGGER_FILE1);
	FILE* trigger2 = openFileWrite(TRIGGER_FILE2);
	FILE* trigger3 = openFileWrite(TRIGGER_FILE3);

	writeTriggerNone(trigger0);
	writeTriggerNone(trigger1);
	writeTriggerNone(trigger2);
	writeTriggerNone(trigger3);

	closeFile(trigger0);
	closeFile(trigger1);
	closeFile(trigger2);
	closeFile(trigger3);

	// init joystick GPIO export
	FILE* gpioExportFile = openFileWrite(GPIO_EXPORT_PATH);
	writeGPIO(gpioExportFile);
	closeFile(gpioExportFile);

	// direction: input for all joystick directions
	FILE* upDir = openFileWrite(JOY_FILE_UP_DIRECTION);
	FILE* leftDir = openFileWrite(JOY_FILE_LEFT_DIRECTION);
	FILE* downDir = openFileWrite(JOY_FILE_DOWN_DIRECTION);
	FILE* rightDir = openFileWrite(JOY_FILE_RIGHT_DIRECTION);

	writeIn(upDir);
	writeIn(leftDir);
	writeIn(downDir);
	writeIn(rightDir);

	closeFile(upDir);
	closeFile(leftDir);
	closeFile(downDir);
	closeFile(rightDir);
}

// turn on a specific LED
void turnOnLED(int led) {
	FILE* brightnessfile;
	switch (led) {
		case 0:
			brightnessfile = openFileWrite(BRIGHTNESS_FILE0);
			writeOn(brightnessfile);
			closeFile(brightnessfile);
			break;
		case 1:
			brightnessfile = openFileWrite(BRIGHTNESS_FILE1);
			writeOn(brightnessfile);
			closeFile(brightnessfile);
			break;
		case 2:
			brightnessfile = openFileWrite(BRIGHTNESS_FILE2);
			writeOn(brightnessfile);
			closeFile(brightnessfile);
			break;
		case 3:
			brightnessfile = openFileWrite(BRIGHTNESS_FILE3);
			writeOn(brightnessfile);
			closeFile(brightnessfile);
			break;
	}
}

// turns off all LEDs
void turnOffLEDs() {
	FILE* brightness0 = openFileWrite(BRIGHTNESS_FILE0);
	FILE* brightness1 = openFileWrite(BRIGHTNESS_FILE1);
	FILE* brightness2 = openFileWrite(BRIGHTNESS_FILE2);
	FILE* brightness3 = openFileWrite(BRIGHTNESS_FILE3);

	writeOff(brightness0);
	writeOff(brightness1);
	writeOff(brightness2);
	writeOff(brightness3);

	closeFile(brightness0);
	closeFile(brightness1);
	closeFile(brightness2);
	closeFile(brightness3);
}

// flash LED with specified delay set number of times
void flashLEDs(int times) {
	int i = 0;

	FILE* brightness0 = openFileWrite(BRIGHTNESS_FILE0);
	FILE* brightness1 = openFileWrite(BRIGHTNESS_FILE1);
	FILE* brightness2 = openFileWrite(BRIGHTNESS_FILE2);
	FILE* brightness3 = openFileWrite(BRIGHTNESS_FILE3);

	for (i = 0; i < times; i++) {
		writeOn(brightness0);
		writeOn(brightness1);
		writeOn(brightness2);
		writeOn(brightness3);

		fflush(brightness0);
		fflush(brightness1);
		fflush(brightness2);
		fflush(brightness3);

		// from https://stackoverflow.com/questions/7684359/how-to-use-nanosleep-in-c-what-are-tim-tv-sec-and-tim-tv-nsec
		nanosleep((const struct timespec[]){{0, LED_FLASH_DELAY_nS}}, NULL);

		writeOff(brightness0);
		writeOff(brightness1);
		writeOff(brightness2);
		writeOff(brightness3);

		fflush(brightness0);
		fflush(brightness1);
		fflush(brightness2);
		fflush(brightness3);

		nanosleep((const struct timespec[]){{0, LED_FLASH_DELAY_nS}}, NULL);
	}

	closeFile(brightness0);
	closeFile(brightness1);
	closeFile(brightness2);
	closeFile(brightness3);
}

// selects a random number (1 or 2) and turns on corresponding LED
int selectTarget() {
	int target = rand() % 2 + 1;

	switch(target) {
		// up
		case 1:
			turnOnLED(0);
			break;
		case 2:
			turnOnLED(3);
			break;
	}

	return target;
}

//hello world and game logic
int main () {
	int result, target;
	int rounds, score = 0;

	printf("Hello embedded world, from Jonathan Tung!\n\n");

	printf("Press the Zen cape's Joystick in the direction of the LED.\n\
  UP for LED 0 (top)\n\
  DOWN for LED 3 (bottom)\n\
  LEFT/RIGHT for exit app.\n\n");

	init();
	flashLEDs(5);

	while(1) {
		printf("Press joystick; current score (%d / %d)\n", score, rounds);
		target = selectTarget();
		result = getJSInput();
		if (!result) {
			printf("Your final score was (%d / %d)\nThank you for playing!\n", score, rounds);
			turnOffLEDs();
			return 0;
		} else {
			if (target == result) {
				printf("Correct!\n");
				score++;
				flashLEDs(1);
			} else {
				printf("Incorrect! :(\n");
				flashLEDs(5);
			}
			rounds++;
		}
	}

	return 0;
}
