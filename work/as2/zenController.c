// Heavily adapted from demo applications by Brian Fraser

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>

#include "sorter.h"

#define CAPE_SLOTS_FILE "/sys/devices/platform/bone_capemgr/slots"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_GENERIC_PATH "/sys/class/gpio/gpio"

#define BB_ADC_NAME "BB-ADC"
#define BB_I2C1_NAME "BB-I2C1"

#define A2D_FILE_VOLTAGE0  "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"
#define A2D_VOLTAGE_REF_V  1.8
#define A2D_MAX_READING    4095

#define PIECEWISE_NUM_POINTS 10
static const float PIECEWISE_A2D[] = {0, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4100};
static const float PIECEWISE_ARRAY_SIZE[] = {1, 20, 60, 120, 250, 300, 500, 800, 1200, 2100};

#define I2CDRV_LINUX_BUS0 "/dev/i2c-0"
#define I2CDRV_LINUX_BUS1 "/dev/i2c-1"
#define I2CDRV_LINUX_BUS2 "/dev/i2c-2"

#define LINUX_LEFTDIGIT_PIN 61
#define LINUX_RIGHTDIGIT_PIN 44

#define GPIO_OUT "out"
#define GPIO_PIN_ON 1
#define GPIO_PIN_OFF 0

#define GPIO_LEFTDIGIT_DIRECTION "/sys/class/gpio/gpio61/direction"
#define GPIO_LEFTDIGIT_VALUE "/sys/class/gpio/gpio61/value"
#define GPIO_RIGHTDIGIT_DIRECTION "/sys/class/gpio/gpio44/direction"
#define GPIO_RIGHTDIGIT_VALUE "/sys/class/gpio/gpio44/value"

#define I2C_DEVICE_ADDRESS 0x20

#define REG_DIRA 0x00
#define REG_DIRB 0x01
#define REG_OUTA 0x14
#define REG_OUTB 0x15

#define DISPLAY_NUM_DIGITS 10
static const int A_DISPLAY_REG_VALUES[] = {0xA3, 0x80, 0x31, 0xB0, 0x90, 0x28, 0xB1, 0x80, 0xB1, 0xB0};
static const int B_DISPLAY_REG_VALUES[] = {0x96, 0x12, 0x0E, 0x06, 0x8A, 0x8C, 0x8C, 0x06, 0x8E, 0x8E};

#define DISPLAY_FLICKER_WAIT_nS 5000000

static pthread_t tid, tid2;
static int done = 0;
static int arraysPerSecond;
static int newArraySize;
static long long prevNumArraysSorted, currNumArraysSorted = 0;

static FILE* openFileWrite(const char* filename);
static int writeOut(FILE* file);
static int writeOn(FILE* file);
static int writeOff(FILE* file);
static int getVoltage0Reading();
static int applyPiecewiseFunc(int reading);
static int initI2cBus(char* bus, int address);
static void writeI2cReg(int i2cFileDesc, unsigned char regAddr, unsigned char value);
//static unsigned char readI2cReg(int i2cFileDesc, unsigned char regAddr);
static void* zenThread(void* arg);
static void* displayThread(void* arg);
static long long calculateNumberForDisplay(long long prev, long long curr);
static void changeNumberOnDisplay(long long num);
static void writeNumberToReg(int num);

void Zen_startThread() {
	pthread_create(&tid, NULL, *zenThread, NULL);
}

void Zen_stopThread() {
	done = 1;
	pthread_join(tid, NULL);
}

int Zen_Display_init() {
	// open the cape manager file
	FILE* capeManager = openFileWrite(CAPE_SLOTS_FILE);

	// enable I2C1
	int charWritten = fprintf(capeManager, BB_I2C1_NAME);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return 0;
	}

	fclose(capeManager);

	// set device to output on all pins
	int i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);

	writeI2cReg(i2cFileDesc, REG_DIRA, 0x00);
	writeI2cReg(i2cFileDesc, REG_DIRB, 0x00);

	close(i2cFileDesc);

	// write LINUX_LEFTDIGIT_PIN and LINUX_RIGHTDIGIT_PIN to export file
	FILE* exportFile = openFileWrite(GPIO_EXPORT_PATH);
	charWritten = fprintf(exportFile, "%d", LINUX_LEFTDIGIT_PIN);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return 0;
	}
	fclose(exportFile);

	exportFile = openFileWrite(GPIO_EXPORT_PATH);
	charWritten = fprintf(exportFile, "%d", LINUX_RIGHTDIGIT_PIN);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return 0;
	}
	fclose(exportFile);

	FILE* leftDigitDirectionFile = openFileWrite(GPIO_LEFTDIGIT_DIRECTION);
	writeOut(leftDigitDirectionFile);
	fclose(leftDigitDirectionFile);
	
	FILE* rightDigitDirectionFile = openFileWrite(GPIO_RIGHTDIGIT_DIRECTION);
	writeOut(rightDigitDirectionFile);
	fclose(rightDigitDirectionFile);

	return 1;
}

int Zen_Display_setValue(int number) {
	return 0;
}

int Zen_Pot_init() {
	// open the cape manager file
	FILE* capeManager = openFileWrite(CAPE_SLOTS_FILE);

	// enable A2D Linux functionality
	int charWritten = fprintf(capeManager, BB_ADC_NAME);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
		return 0;
	}

	fclose(capeManager);

	return 1;
}

int Zen_Pot_getArraySize() {
	return newArraySize;
}

// opens a file for writing, returns the pointer to file object
static FILE* openFileWrite(const char* filename) {
	FILE *pFile = fopen(filename, "w");

	if (pFile == NULL) {
		printf("ERROR OPENING %s", filename);
	}

	return pFile;
}

// writes GPIO_OUT to the specified file
static int writeOut(FILE* file) {
	int charWritten = fprintf(file, GPIO_OUT);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// writes GPIO_PIN_ON to the specified file
static int writeOn(FILE* file) {
	int charWritten = fprintf(file, "%d", GPIO_PIN_ON);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// writes GPIO_PIN_OFF to the specified file
static int writeOff(FILE* file) {
	int charWritten = fprintf(file, "%d", GPIO_PIN_OFF);
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	return charWritten;
}

// "complete" version of writeOn which handles
static int writeOnToFile(const char* filename) {
	FILE* file = openFileWrite(filename);
	writeOn(file);
	fclose(file);

	return 1;
}

static int writeOffToFile(const char* filename) {
	FILE* file = openFileWrite(filename);
	writeOff(file);
	fclose(file);

	return 1;
}

static int getVoltage0Reading() {
	// Open file
	FILE *voltageFile = fopen(A2D_FILE_VOLTAGE0, "r");
	if (!voltageFile) {
		printf("ERROR: Unable to open voltage input file. Cape loaded?\n");
		printf("try:   Call Pot_init first\n");
		exit(-1);
	}

	// Get reading
	int a2dReading = 0;
	int itemsRead = fscanf(voltageFile, "%d", &a2dReading);
	if (itemsRead <= 0) {
		printf("ERROR: Unable to read values from voltage input file.\n");
		exit(-1);
	}

	// Close file
	fclose(voltageFile);

	return a2dReading;
}

static int applyPiecewiseFunc(int reading) {
	if (reading < PIECEWISE_A2D[0]) {
		return PIECEWISE_ARRAY_SIZE[0];
	} else if (reading > PIECEWISE_A2D[PIECEWISE_NUM_POINTS-1]) {
		return PIECEWISE_ARRAY_SIZE[PIECEWISE_NUM_POINTS-1];
	}

	int i = 0;

	while (reading > PIECEWISE_A2D[i]) {
		i++;
	}

	if (reading == (int) PIECEWISE_A2D[i]) {
		return PIECEWISE_ARRAY_SIZE[i];
	} else {
		return (int) (((reading-PIECEWISE_A2D[i])/(PIECEWISE_A2D[i-1]-PIECEWISE_A2D[i]))*
		(PIECEWISE_ARRAY_SIZE[i-1]-PIECEWISE_ARRAY_SIZE[i]))+
		PIECEWISE_ARRAY_SIZE[i];
	}
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

static void* zenThread(void* arg) {
	int reading;
	long long arraysSortedInLastSec = 0;

	pthread_create(&tid2, NULL, *displayThread, NULL);

	while(!done) {
		// get number to show on display
		prevNumArraysSorted = currNumArraysSorted;
		nanosleep((const struct timespec[]){{1, 0}}, NULL); // wait 1 second
		currNumArraysSorted = Sorter_getNumberArraysSorted();
		arraysSortedInLastSec = calculateNumberForDisplay(prevNumArraysSorted, currNumArraysSorted);
		changeNumberOnDisplay(arraysSortedInLastSec);

		// update new array size for sorter thread to get
		reading = getVoltage0Reading();
		newArraySize = applyPiecewiseFunc(reading);
		Sorter_setArraySize(newArraySize);
		printf("%d\n", newArraySize);
	}

	pthread_join(tid2, NULL);

	return NULL;
}

static void* displayThread(void* arg) {
	// drives the individual digits for the display
	// continually shows the internal global 'arraysPerSecond'

	writeOffToFile(GPIO_LEFTDIGIT_VALUE);
	writeOffToFile(GPIO_RIGHTDIGIT_VALUE);	

	while(!done) {
		// left digit
		writeNumberToReg(arraysPerSecond/10);
		writeOnToFile(GPIO_LEFTDIGIT_VALUE);
		nanosleep((const struct timespec[]){{0, DISPLAY_FLICKER_WAIT_nS}}, NULL);
		writeOffToFile(GPIO_LEFTDIGIT_VALUE);

		// right digit
		writeNumberToReg(arraysPerSecond%10);
		writeOnToFile(GPIO_RIGHTDIGIT_VALUE);
		nanosleep((const struct timespec[]){{0, DISPLAY_FLICKER_WAIT_nS}}, NULL);
		writeOffToFile(GPIO_RIGHTDIGIT_VALUE);
	}

	return NULL;
}

static long long calculateNumberForDisplay(long long prev, long long curr) {
	long long res;

	if (prev == curr) {
		return 0;
	} else {
		res = curr - prev;
		if (res > 99) {
			return 99;
		} else {
			return res;
		}
	}
}

static void changeNumberOnDisplay(long long num) {
	arraysPerSecond = (int) num;
}

// Writes the correct hex to I2C bus for displaying specified digit.
// The caller should be responsible for driving unique left/right
// digits if it is intended
static void writeNumberToReg(int num) {
	int i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);

	writeI2cReg(i2cFileDesc, REG_OUTA, A_DISPLAY_REG_VALUES[num]);
	writeI2cReg(i2cFileDesc, REG_OUTB, B_DISPLAY_REG_VALUES[num]);

	close(i2cFileDesc);
}