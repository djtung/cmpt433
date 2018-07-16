#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>

#include "audioMixer.h"

#define CAPE_SLOTS_FILE "/sys/devices/platform/bone_capemgr/slots"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"

#define BB_ADC_NAME "BB-ADC"
#define BB_I2C1_NAME "BB-I2C1"

#define I2CDRV_LINUX_BUS0 "/dev/i2c-0"
#define I2CDRV_LINUX_BUS1 "/dev/i2c-1"
#define I2CDRV_LINUX_BUS2 "/dev/i2c-2"

#define GPIO_IN "in"
#define GPIO_OUT "out"
#define GPIO_PIN_ON 1
#define GPIO_PIN_OFF 0

#define JOY_FILE_UP_DIRECTION "/sys/class/gpio/gpio26/direction"
#define JOY_FILE_RIGHT_DIRECTION "/sys/class/gpio/gpio47/direction"
#define JOY_FILE_DOWN_DIRECTION "/sys/class/gpio/gpio46/direction"
#define JOY_FILE_LEFT_DIRECTION "/sys/class/gpio/gpio65/direction"
#define JOY_FILE_PUSH_DIRECTION "/sys/class/gpio/gpio27/direction"

#define JOY_FILE_UP "/sys/class/gpio/gpio26/value"
#define JOY_FILE_RIGHT "/sys/class/gpio/gpio47/value"
#define JOY_FILE_DOWN "/sys/class/gpio/gpio46/value"
#define JOY_FILE_LEFT "/sys/class/gpio/gpio65/value"
#define JOY_FILE_PUSH "/sys/class/gpio/gpio27/value"

#define JS_FILE_BUFFER_SIZE 5
#define UPTIME_BUFFER_SIZE 50

#define GPIO_JSUP 26
#define GPIO_JSRT 47
#define GPIO_JSDN 46
#define GPIO_JSLFT 65
#define GPIO_JSPSH 27

#define I2C_DEVICE_ADDRESS 0x1C
#define ACCL_CTRL_REG1 0x2A

#define ACCL_BUFF_X_MSB 1
#define ACCL_BUFF_X_LSB 2
#define ACCL_BUFF_Y_MSB 3
#define ACCL_BUFF_Y_LSB 4
#define ACCL_BUFF_Z_MSB 5
#define ACCL_BUFF_Z_LSB 6

#define ACCL_BUFF_SIZE 7

#define ACCL_TRIGGER_THRESH 9000

#define ACCL_POLLING_TIME_nS 10000000
#define ACCL_DEBOUNCE_TIME_nS 250000000

static unsigned char ACCL_DATA_ADDRS[ACCL_BUFF_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

static int done = 0;
static pthread_t jsTid, accelTid;
static pthread_t aXTid, aYTid, aZTid;

static int Zen_Hardware_init();
static void* jsThread(void* arg);
static void* accelThread(void* arg);
static void* accelXThread(void* arg);
static void* accelYThread(void* arg);
static void* accelZThread(void* arg);
static FILE* openFileWrite(const char* filename);
static FILE* openFileRead(const char* filename);
static int closeFile(FILE* stream);
static int writeGPIO(FILE* file);
static int writeIn(FILE* file);
static int initI2cBus(char* bus, int address);
static void writeI2cReg(int i2cFileDesc, unsigned char regAddr, unsigned char value);

int16_t x, y, z;

void Zen_startThread() {
	Zen_Hardware_init();
	pthread_create(&jsTid, NULL, *jsThread, NULL);
	pthread_create(&accelTid, NULL, *accelThread, NULL);
}

void Zen_stopThread() {
	done = 1;
	pthread_join(jsTid, NULL);
	pthread_join(accelTid, NULL);
}

int Zen_getUptime() {
	char buff[UPTIME_BUFFER_SIZE];
	FILE *fp;

	fp = fopen("/proc/uptime", "r");
	fscanf(fp, "%s", buff);
	fclose(fp);

	return atoi(buff);
}

static int Zen_Hardware_init() {
	// open the cape manager file
	FILE* capeManager = openFileWrite(CAPE_SLOTS_FILE);
	// enable I2C-1
	int charWritten = fprintf(capeManager, BB_I2C1_NAME);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return 0;
	}
	fclose(capeManager);

	// init accelerometer
	int i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);
	// active mode
	writeI2cReg(i2cFileDesc, ACCL_CTRL_REG1, 0x01);
	close(i2cFileDesc);

	// init joystick GPIO export
	FILE* gpioExportFile = openFileWrite(GPIO_EXPORT_PATH);
	writeGPIO(gpioExportFile);
	closeFile(gpioExportFile);

	// direction: input for all joystick directions
	FILE* upDir = openFileWrite(JOY_FILE_UP_DIRECTION);
	FILE* leftDir = openFileWrite(JOY_FILE_LEFT_DIRECTION);
	FILE* downDir = openFileWrite(JOY_FILE_DOWN_DIRECTION);
	FILE* rightDir = openFileWrite(JOY_FILE_RIGHT_DIRECTION);
	FILE* pushDir = openFileWrite(JOY_FILE_PUSH_DIRECTION);

	writeIn(upDir);
	writeIn(leftDir);
	writeIn(downDir);
	writeIn(rightDir);
	writeIn(pushDir);

	closeFile(upDir);
	closeFile(leftDir);
	closeFile(downDir);
	closeFile(rightDir);
	closeFile(pushDir);

	return 1;
}

// opens a file for writing, returns the pointer to file object
static FILE* openFileWrite(const char* filename) {
	FILE *pFile = fopen(filename, "w");

	if (pFile == NULL) {
		printf("ERROR OPENING %s", filename);
	}

	return pFile;
}

// opens a file for reading, returns the pointer to file object
static FILE* openFileRead(const char* filename) {
	FILE *pFile = fopen(filename, "r");

	if (pFile == NULL) {
		printf("ERROR OPENING %s.", filename);
	}

	return pFile;
}

static int closeFile(FILE* stream) {
	return fclose(stream);
}

// writes GPIO_IN to the specified file
static int writeIn(FILE* file) {
	int charWritten = fprintf(file, GPIO_IN);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

static int initI2cBus(char* bus, int address)
{
	int i2cFileDesc = open(bus, O_RDWR);
	if (i2cFileDesc < 0) {
		printf("I2C DRV: Unable to open bus for read/write (%s)\n", bus);
		perror("Error is:");
		exit(-1);
	}

	int result = ioctl(i2cFileDesc, I2C_SLAVE, address);
	if (result < 0) {
		perror("Unable to set I2C device to slave address.");
		exit(-1);
	}
	return i2cFileDesc;
}

static void writeI2cReg(int i2cFileDesc, unsigned char regAddr, unsigned char value)
{
	unsigned char buff[2];
	buff[0] = regAddr;
	buff[1] = value;
	int res = write(i2cFileDesc, buff, 2);
	if (res != 2) {
		perror("Unable to write i2c register");
		exit(-1);
	}
}

/*static unsigned char readI2cReg(int i2cFileDesc, unsigned char regAddr)
{
	// To read a register, must first write the address
	int res = write(i2cFileDesc, &regAddr, sizeof(regAddr));
	if (res != sizeof(regAddr)) {
		perror("Unable to write i2c register.");
		exit(-1);
	}

	// Now read the value and return it
	char value = 0;
	res = read(i2cFileDesc, &value, sizeof(value));
	if (res != sizeof(value)) {
		perror("Unable to read i2c register");
		exit(-1);
	}
	return value;
}*/

// reads a 'length' sized number of addresses (each 1 byte) starting from 'regAddr' into 'values'
static void bulkReadI2cReg(int i2cFileDesc, unsigned char* regAddr, char* values, int length)
{
	size_t numBytes = length * sizeof(*regAddr);
	// To read a register, must first write the address
	int res = write(i2cFileDesc, regAddr, numBytes);
	if (res != numBytes) {
		perror("Unable to write i2c register.");
		exit(-1);
	}

	numBytes = length * sizeof(*values);
	// Now read the values and return them
	res = read(i2cFileDesc, values, numBytes);
	if (res != numBytes) {
		perror("Unable to read i2c register");
		exit(-1);
	}
}

// for the 4 joystick directions, write them to the export
static int writeGPIO(FILE* file) {
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

	charWritten = fprintf(file, "%d", GPIO_JSPSH);
	fflush(file);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

static void* jsThread(void* arg) {
	char bufUp[JS_FILE_BUFFER_SIZE], bufRight[JS_FILE_BUFFER_SIZE];
	char bufDown[JS_FILE_BUFFER_SIZE], bufLeft[JS_FILE_BUFFER_SIZE];
	char bufPush[JS_FILE_BUFFER_SIZE];
	int result, shouldExit, oldVolume, oldBPM, oldBeat;
	FILE* jsUp = NULL;
	FILE* jsRight = NULL;
	FILE* jsDown = NULL;
	FILE* jsLeft = NULL;
	FILE* jsPush = NULL;

	while(!done) {
		result = 0;
		shouldExit = 0;
		// poll for direction press
		while (!done) {
			jsUp = openFileRead(JOY_FILE_UP);
			jsRight = openFileRead(JOY_FILE_RIGHT);
			jsDown = openFileRead(JOY_FILE_DOWN);
			jsLeft = openFileRead(JOY_FILE_LEFT);
			jsPush = openFileRead(JOY_FILE_PUSH);

			fgets(bufUp, 5, jsUp);
			fgets(bufRight, 5, jsRight);
			fgets(bufLeft, 5, jsLeft);
			fgets(bufDown, 5, jsDown);
			fgets(bufPush, 5, jsPush);

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
			} else if (!atoi(bufPush)) {
				result = 5;
				break;
			}

			closeFile(jsUp);
			closeFile(jsRight);
			closeFile(jsDown);
			closeFile(jsLeft);
			closeFile(jsPush);
		}

		if (!done) {
			closeFile(jsUp);
			closeFile(jsRight);
			closeFile(jsDown);
			closeFile(jsLeft);
			closeFile(jsPush);
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
				oldVolume = AudioMixer_getVolume();
				AudioMixer_setVolume(oldVolume+5);
				break;
			case 2:
				do {
					jsRight = openFileRead(JOY_FILE_RIGHT);
					fgets(bufRight, 5, jsRight);
					if (atoi(bufRight)) {
						shouldExit = 1;
					}
					closeFile(jsRight);
				} while (!shouldExit);
				oldBPM = AudioMixer_getBPM();
				AudioMixer_setBPM(oldBPM+5);
				break;
			case 3:
				do {
					jsDown = openFileRead(JOY_FILE_DOWN);
					fgets(bufDown, 5, jsDown);
					if (atoi(bufDown)) {
						shouldExit = 1;
					}
					closeFile(jsDown);
				} while (!shouldExit);
				oldVolume = AudioMixer_getVolume();
				AudioMixer_setVolume(oldVolume-5);
				break;
			case 4:
				do {
					jsLeft = openFileRead(JOY_FILE_LEFT);
					fgets(bufLeft, 5, jsLeft);
					if (atoi(bufLeft)) {
						shouldExit = 1;
					}
					closeFile(jsLeft);
				} while (!shouldExit);
				oldBPM = AudioMixer_getBPM();
				AudioMixer_setBPM(oldBPM-5);
				break;
			case 5:
				do {
					jsPush = openFileRead(JOY_FILE_PUSH);
					fgets(bufPush, 5, jsPush);
					if (atoi(bufPush)) {
						shouldExit = 1;
					}
					closeFile(jsPush);
				} while (!shouldExit);
				oldBeat = AudioMixer_getBeat();
				AudioMixer_setBeat((oldBeat+1) % 3);
				break;
		}
	}
	return NULL;
}

static int getX() {
	return (int) x;
}

static void setX(int16_t newX) {
	x = newX;
}

static int getY() {
	return (int) y;
}

static void setY(int16_t newY) {
	y = newY;
}

static int getZ() {
	return (int) z;
}

static void setZ(int16_t newZ) {
	z = newZ;
}

static void* accelThread(void* arg) {
	int i2cFileDesc;
	char buff[ACCL_BUFF_SIZE];

	pthread_create(&aXTid, NULL, *accelXThread, NULL);
	pthread_create(&aYTid, NULL, *accelYThread, NULL);
	pthread_create(&aZTid, NULL, *accelZThread, NULL);

	while (!done) {
		i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);
		bulkReadI2cReg(i2cFileDesc, ACCL_DATA_ADDRS, buff, ACCL_BUFF_SIZE);
		setX((buff[ACCL_BUFF_X_MSB] << 8) | (buff[ACCL_BUFF_X_LSB]));
		setY((buff[ACCL_BUFF_Y_MSB] << 8) | (buff[ACCL_BUFF_Y_LSB]));
		setZ((buff[ACCL_BUFF_Z_MSB] << 8) | (buff[ACCL_BUFF_Z_LSB]));
		//printf("x:%d y:%d z:%d\n", x, y, z);
		close(i2cFileDesc);
	}

	pthread_join(aXTid, NULL);
	pthread_join(aYTid, NULL);
	pthread_join(aZTid, NULL);

	return NULL;
}

static void* accelXThread(void* arg) {
	int diff, prev, next;
	while (!done) {
		prev = getX();
		nanosleep((const struct timespec[]){{0, ACCL_POLLING_TIME_nS}}, NULL);
		next = getX();

		diff = abs(prev-next);
		if (diff > ACCL_TRIGGER_THRESH) {
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, ACCL_DEBOUNCE_TIME_nS}}, NULL);
		}

	}
	return NULL;
}

static void* accelYThread(void* arg) {
	int diff, prev, next;
	while (!done) {
		prev = getY();
		nanosleep((const struct timespec[]){{0, ACCL_POLLING_TIME_nS}}, NULL);
		next = getY();

		diff = abs(prev-next);
		if (diff > ACCL_TRIGGER_THRESH) {
			AudioMixer_queueSound(&snareDrum);
			nanosleep((const struct timespec[]){{0, ACCL_DEBOUNCE_TIME_nS}}, NULL);
		}

	}
	return NULL;
}

static void* accelZThread(void* arg) {
	int diff, prev, next;
	while (!done) {
		prev = getZ();
		nanosleep((const struct timespec[]){{0, ACCL_POLLING_TIME_nS}}, NULL);
		next = getZ();

		diff = abs(prev-next);
		if (diff > ACCL_TRIGGER_THRESH) {
			AudioMixer_queueSound(&bassDrum);
			nanosleep((const struct timespec[]){{0, ACCL_DEBOUNCE_TIME_nS}}, NULL);
		}

	}
	return NULL;
}